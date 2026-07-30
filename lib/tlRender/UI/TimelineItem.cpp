// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/TimelineItemPrivate.h>

#include <tlRender/Timeline/IRender.h>
#include <tlRender/Timeline/Util.h>

#include <ftk/UI/DrawUtil.h>
#include <ftk/UI/ScreenshotTag.h>
#include <ftk/UI/ScrollArea.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/RenderUtil.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/gap.h>

#include <algorithm>
#include <cmath>

namespace tl
{
    namespace ui
    {
        namespace
        {
            //! Add a rectangle to a mesh, optionally with a color of its own.
            //! Batching the item rectangles this way turns a draw call per item
            //! into one draw call for the whole timeline, which is what the
            //! per-item widgets used to cost.
            void addRect(
                ftk::TriMesh2F& mesh,
                const ftk::Box2I& box,
                const ftk::Color4F* color = nullptr)
            {
                const size_t v = mesh.v.size();
                mesh.v.emplace_back(box.min.x, box.min.y);
                mesh.v.emplace_back(box.max.x + 1, box.min.y);
                mesh.v.emplace_back(box.max.x + 1, box.max.y + 1);
                mesh.v.emplace_back(box.min.x, box.max.y + 1);
                size_t c = 0;
                if (color)
                {
                    mesh.c.emplace_back(color->r, color->g, color->b, color->a);
                    c = mesh.c.size();
                }
                mesh.triangles.push_back({
                    ftk::Vertex2(v + 1, 0, c),
                    ftk::Vertex2(v + 3, 0, c),
                    ftk::Vertex2(v + 2, 0, c) });
                mesh.triangles.push_back({
                    ftk::Vertex2(v + 3, 0, c),
                    ftk::Vertex2(v + 1, 0, c),
                    ftk::Vertex2(v + 4, 0, c) });
            }

            //! Append a mesh, moving it into place. Waveforms arrive as meshes
            //! in their own space; folding them into one mesh keeps a track of
            //! audio to a single draw call.
            void addMesh(
                ftk::TriMesh2F& mesh,
                const ftk::TriMesh2F& value,
                const ftk::V2F& pos)
            {
                const size_t v = mesh.v.size();
                mesh.v.reserve(mesh.v.size() + value.v.size());
                for (const auto& i : value.v)
                {
                    mesh.v.emplace_back(i.x + pos.x, i.y + pos.y);
                }
                mesh.triangles.reserve(mesh.triangles.size() + value.triangles.size());
                for (const auto& i : value.triangles)
                {
                    mesh.triangles.push_back({
                        ftk::Vertex2(i.v[0].v + v),
                        ftk::Vertex2(i.v[1].v + v),
                        ftk::Vertex2(i.v[2].v + v) });
                }
            }

            void clearMesh(ftk::TriMesh2F& mesh)
            {
                mesh.v.clear();
                mesh.c.clear();
                mesh.triangles.clear();
            }
        }

        TimelineItem::Private::Range TimelineItem::Private::getRange(
            const std::vector<Item>& items,
            int x0,
            int x1)
        {
            Range out;
            const auto begin = std::lower_bound(
                items.begin(),
                items.end(),
                x0,
                [](const Item& item, int x) { return item.x + item.w < x; });
            const auto end = std::upper_bound(
                begin,
                items.end(),
                x1,
                [](int x, const Item& item) { return x < item.x; });
            out.begin = begin - items.begin();
            out.end = end - items.begin();
            return out;
        }

        ftk::Color4F TimelineItem::Private::getColor(
            const Item& item,
            const DisplayOptions& displayOptions,
            bool enabled)
        {
            ftk::Color4F out = item.defaultColor;
            if (displayOptions.clipColors && item.otioColor.has_value())
            {
                out = item.otioColor.value();
            }
            return enabled ? out : ftk::greyscale(out);
        }

        ftk::Box2I TimelineItem::Private::getGeom(
            const Track& track,
            const Item& item,
            const ftk::V2I& origin)
        {
            return ftk::Box2I(
                origin.x + item.x,
                track.geom.min.y,
                item.w,
                track.clipHeight);
        }

        ftk::Box2I TimelineItem::Private::getInsideGeom(
            const ftk::Box2I& geom,
            int border)
        {
            ftk::Box2I out = ftk::margin(geom, -(border * 2));
            if (out.w() < 1)
            {
                // An item narrower than the border would draw nothing at all
                // and show the track behind it, which reads as a gap rather
                // than as a very short item. Keep a pixel of it.
                out.min.x = geom.min.x;
                out.max.x = geom.min.x;
            }
            return out;
        }

        ftk::Box2I TimelineItem::Private::getMediaGeom(
            const ftk::Box2I& insideGeom,
            const DisplayOptions& displayOptions,
            int height) const
        {
            return ftk::Box2I(
                insideGeom.min.x,
                insideGeom.min.y +
                (!displayOptions.minimize ?
                    (size.itemFontMetrics.lineHeight + size.margin * 2) :
                    0),
                insideGeom.w(),
                height);
        }

        int TimelineItem::Private::getThumbnailWidth(
            const Item& item,
            const DisplayOptions& displayOptions)
        {
            int out = 0;
            if (displayOptions.thumbnails &&
                item.ioInfo.has_value() &&
                !item.ioInfo->video.empty())
            {
                out = static_cast<int>(
                    displayOptions.thumbnailHeight *
                    ftk::aspectRatio(item.ioInfo->video[0].size));
            }
            return out;
        }

        void TimelineItem::Private::cancelRequests(Item& item)
        {
            std::vector<uint64_t> ids;
            takeRequests(item, ids);
            if (!ids.empty())
            {
                thumbnailSystem->cancelRequests(ids);
            }
        }

        void TimelineItem::Private::takeRequests(
            Item& item,
            std::vector<uint64_t>& ids)
        {
            if (item.infoRequest.future.valid())
            {
                ids.push_back(item.infoRequest.id);
                item.infoRequest = InfoRequest();
            }
            for (const auto& i : item.thumbnailRequests)
            {
                ids.push_back(i.second.id);
            }
            item.thumbnailRequests.clear();
            for (const auto& i : item.waveformRequests)
            {
                ids.push_back(i.second.id);
            }
            item.waveformRequests.clear();
        }

        void TimelineItem::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<Player>& player,
            double scale,
            const ItemOptions& options,
            const DisplayOptions& displayOptions,
            const std::shared_ptr<ItemData>& itemData,
            const std::shared_ptr<IWidget>& parent)
        {
            const OTIO_NS::TimeRange timeRange = player->getTimeRange();
            IMouseWidget::_init(context, "tl::ui::TimelineItem", parent);
            FTK_P();

            _timeRange = timeRange;
            _scale = scale;
            _options = options;
            _displayOptions = displayOptions;
            _data = itemData;

            p.timeUnitsObserver = ftk::Observer<bool>::create(
                itemData->timeUnitsModel->observeTimeUnitsChanged(),
                [this](bool)
                {
                    _timeUnitsUpdate();
                });

            _setMouseHoverEnabled(true);
            _setMousePressEnabled(true, ftk::MouseButton::Left, 0);

            p.player = player;
            p.thumbnailSystem = context->getSystem<ThumbnailSystem>();

            p.scrub = ftk::Observable<bool>::create(false);
            p.timeScrub = ftk::Observable<std::optional<OTIO_NS::RationalTime> >::create();

            _itemsInit(context);
            _itemsScaleUpdate();
            _tracksUpdate();
            _textUpdate();

            p.currentTimeObserver = ftk::Observer<OTIO_NS::RationalTime>::create(
                p.player->observeCurrentTime(),
                [this](const OTIO_NS::RationalTime& value)
                {
                    _p->currentTime = value;
                    setDrawUpdate();
                });

            p.inOutRangeObserver = ftk::Observer<OTIO_NS::TimeRange>::create(
                p.player->observeInOutRange(),
                [this](const OTIO_NS::TimeRange value)
                {
                    _p->inOutRange = value;
                    setDrawUpdate();
                });

            p.cacheInfoObserver = ftk::Observer<PlayerCacheInfo>::create(
                p.player->observeCacheInfo(),
                [this](const PlayerCacheInfo& value)
                {
                    _p->cacheInfo = value;
                    setDrawUpdate();
                });
        }

        TimelineItem::TimelineItem() :
            _p(new Private)
        {}

        TimelineItem::~TimelineItem()
        {
            _cancelRequests();
        }

        std::shared_ptr<TimelineItem> TimelineItem::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<Player>& player,
            double scale,
            const ItemOptions& options,
            const DisplayOptions& displayOptions,
            const std::shared_ptr<ItemData>& itemData,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<TimelineItem>(new TimelineItem);
            out->_init(
                context,
                player,
                scale,
                options,
                displayOptions,
                itemData,
                parent);
            return out;
        }

        void TimelineItem::setStopOnScrub(bool value)
        {
            _p->stopOnScrub = value;
        }

        std::shared_ptr<ftk::IObservable<bool> > TimelineItem::observeScrub() const
        {
            return _p->scrub;
        }

        std::shared_ptr<ftk::IObservable<std::optional<OTIO_NS::RationalTime> > > TimelineItem::observeTimeScrub() const
        {
            return _p->timeScrub;
        }

        void TimelineItem::setFrameMarkers(const std::vector<int>& value)
        {
            FTK_P();
            if (value == p.frameMarkers)
                return;
            p.frameMarkers = value;
            setDrawUpdate();
        }

        void TimelineItem::setScale(double value)
        {
            if (value == _scale)
                return;
            _scale = value;

            // The thumbnails and waveforms were made for the old item widths,
            // so they are dropped and asked for again.
            _cancelRequests();
            _itemsScaleUpdate();

            setSizeUpdate();
            setDrawUpdate();
        }

        void TimelineItem::setDisplayOptions(const DisplayOptions& value)
        {
            FTK_P();
            const bool changed = value != _displayOptions;
            const bool mediaChanged =
                value.thumbnails != _displayOptions.thumbnails ||
                value.thumbnailHeight != _displayOptions.thumbnailHeight ||
                value.waveforms != _displayOptions.waveforms ||
                value.waveformWidth != _displayOptions.waveformWidth ||
                value.waveformHeight != _displayOptions.waveformHeight ||
                value.waveformPrim != _displayOptions.waveformPrim;
            if (!changed)
                return;
            _displayOptions = value;
            if (mediaChanged)
            {
                _cancelRequests();
            }
            p.size.init = true;
            _tracksUpdate();
            _textUpdate();
            setSizeUpdate();
            setDrawUpdate();
        }

        ftk::Size2I TimelineItem::getSizeHint() const
        {
            return _p->size.sizeHint;
        }

        void TimelineItem::setGeometry(const ftk::Box2I& value)
        {
            IMouseWidget::setGeometry(value);
            FTK_P();

            int y =
                p.size.margin +
                p.size.fontMetrics.lineHeight +
                p.size.margin +
                p.size.border * 4 +
                p.size.border +
                value.min.y;
            for (auto& track : p.tracks)
            {
                const bool visible = track.visible;

                ftk::Size2I labelSizeHint;
                ftk::Size2I durationSizeHint;
                int trackInfoHeight = 0;
                if (visible && !_displayOptions.minimize)
                {
                    labelSizeHint = track.label->getSizeHint();
                    durationSizeHint = track.durationLabel->getSizeHint();
                    trackInfoHeight = std::max(
                        labelSizeHint.h,
                        durationSizeHint.h);
                }
                track.label->setGeometry(ftk::Box2I(
                    value.min.x,
                    y + trackInfoHeight / 2 - labelSizeHint.h / 2,
                    labelSizeHint.w,
                    labelSizeHint.h));
                track.durationLabel->setGeometry(ftk::Box2I(
                    value.min.x + track.size.w - durationSizeHint.w,
                    y + trackInfoHeight / 2 - durationSizeHint.h / 2,
                    durationSizeHint.w,
                    durationSizeHint.h));

                // The items are placed relative to the timeline origin when
                // the scale changes; only the track's vertical band and the
                // height of the track labels above it are needed here, so
                // scrolling does not touch the items at all.
                track.geom = ftk::Box2I(
                    value.min.x,
                    y + trackInfoHeight,
                    track.size.w,
                    visible ? track.clipHeight : 0);

                if (visible)
                {
                    y += track.size.h;
                }
            }

            // Put the screenshot tags over an example of each kind of item.
            const std::pair<ItemType, std::shared_ptr<ftk::Spacer> > tagProxies[] =
            {
                { ItemType::Video, p.tagProxies.videoClip },
                { ItemType::Audio, p.tagProxies.audioClip },
                { ItemType::Gap,   p.tagProxies.gap }
            };
            for (const auto& tagProxy : tagProxies)
            {
                ftk::Box2I geom;
                for (const auto& track : p.tracks)
                {
                    if (!track.visible)
                        continue;
                    const auto i = std::find_if(
                        track.items.begin(),
                        track.items.end(),
                        [&tagProxy](const Private::Item& item)
                        {
                            return item.type == tagProxy.first;
                        });
                    if (i != track.items.end())
                    {
                        geom = ftk::Box2I(
                            value.min.x + i->x,
                            track.geom.min.y,
                            i->w,
                            track.clipHeight);
                        break;
                    }
                }
                tagProxy.second->setVisible(geom.w() > 0 && geom.h() > 0);
                tagProxy.second->setGeometry(geom);
            }

            if (auto scrollArea = getParentT<ftk::ScrollArea>())
            {
                p.size.scrollArea = ftk::Box2I(
                    scrollArea->getScrollPos(),
                    scrollArea->getGeometry().size());
            }
        }

        void TimelineItem::styleEvent(const ftk::StyleEvent& event)
        {
            IMouseWidget::styleEvent(event);
            FTK_P();
            if (event.hasChanges())
            {
                p.size.init = true;
            }
        }

        void TimelineItem::sizeHintEvent(const ftk::SizeHintEvent& event)
        {
            IMouseWidget::sizeHintEvent(event);
            FTK_P();
            if (p.size.init)
            {
                p.size.init = false;
                p.size.margin = event.style->getSizeRole(ftk::SizeRole::MarginInside, event.displayScale);
                p.size.spacing = event.style->getSizeRole(ftk::SizeRole::SpacingSmall, event.displayScale);
                p.size.border = event.style->getSizeRole(ftk::SizeRole::Border, event.displayScale);
                p.size.handle = event.style->getSizeRole(ftk::SizeRole::Handle, event.displayScale);
                p.size.fontInfo = event.style->getFont(ftk::FontType::Mono, event.displayScale);
                p.size.fontMetrics = event.fontSystem->getMetrics(p.size.fontInfo);
                p.size.itemFontInfo = event.style->getFont(ftk::FontType::Regular, event.displayScale);
                p.size.itemFontMetrics = event.fontSystem->getMetrics(p.size.itemFontInfo);
                _itemsTextUpdate(event);
            }

            // An item is as tall as its labels, its media, and its border. A
            // track is as tall as its tallest item, so a track of gaps is no
            // taller than the gaps need.
            int itemHeight = p.size.border * 4;
            if (!_displayOptions.minimize)
            {
                itemHeight +=
                    p.size.itemFontMetrics.lineHeight +
                    p.size.margin * 2;
            }
            int tracksHeight = 0;
            for (auto& track : p.tracks)
            {
                track.visible = _isTrackVisible(track.index);

                track.size.w = track.timeRange.duration().rescaled_to(1.0).value() * _scale;
                track.size.h = 0;
                track.clipHeight = 0;
                if (track.visible)
                {
                    for (const auto& item : track.items)
                    {
                        int h = itemHeight;
                        switch (item.type)
                        {
                        case ItemType::Video:
                            if (_displayOptions.thumbnails)
                            {
                                h += _displayOptions.thumbnailHeight;
                            }
                            break;
                        case ItemType::Audio:
                            if (_displayOptions.waveforms)
                            {
                                h += _displayOptions.waveformHeight;
                            }
                            break;
                        default: break;
                        }
                        track.clipHeight = std::max(track.clipHeight, h);
                    }
                    track.size.h = track.clipHeight;
                    if (!_displayOptions.minimize)
                    {
                        track.size.h += std::max(
                            track.label->getSizeHint().h,
                            track.durationLabel->getSizeHint().h);
                    }
                    tracksHeight += track.size.h;
                }
            }
            p.size.sizeHint = ftk::Size2I(
                _timeRange.duration().rescaled_to(1.0).value() * _scale,
                p.size.margin +
                p.size.fontMetrics.lineHeight +
                p.size.margin +
                p.size.border * 4 +
                p.size.border +
                tracksHeight);
        }

        void TimelineItem::clipEvent(const ftk::Box2I& clipRect, bool clipped)
        {
            IMouseWidget::clipEvent(clipRect, clipped);
            if (clipped)
            {
                _cancelRequests();
            }
        }

        void TimelineItem::tickEvent(
            bool parentsVisible,
            bool parentsEnabled,
            const ftk::TickEvent& event)
        {
            IMouseWidget::tickEvent(parentsVisible, parentsEnabled, event);
            FTK_P();

            // Collect whatever the thumbnail system has finished. This used to
            // run once per item widget, which meant walking every item in the
            // timeline on every tick; only the items in view have requests
            // outstanding now.
            bool sizeUpdate = false;
            bool drawUpdate = false;
            for (size_t i = 0; i < p.tracks.size() && i < p.active.size(); ++i)
            {
                auto& track = p.tracks[i];
                for (size_t j = p.active[i].begin; j < p.active[i].end; ++j)
                {
                    auto& item = track.items[j];

                    if (item.infoRequest.future.valid() &&
                        item.infoRequest.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                    {
                        item.ioInfo = item.infoRequest.future.get();
                        item.infoRequest = InfoRequest();
                        sizeUpdate = true;
                        drawUpdate = true;
                    }

                    auto k = item.thumbnailRequests.begin();
                    while (k != item.thumbnailRequests.end())
                    {
                        if (k->second.future.valid() &&
                            k->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                        {
                            item.thumbnails[*k->second.time] = k->second.future.get();
                            k = item.thumbnailRequests.erase(k);
                            drawUpdate = true;
                        }
                        else
                        {
                            ++k;
                        }
                    }

                    auto l = item.waveformRequests.begin();
                    while (l != item.waveformRequests.end())
                    {
                        if (l->second.future.valid() &&
                            l->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                        {
                            item.waveforms[l->second.timeRange->start_time()] = l->second.future.get();
                            l = item.waveformRequests.erase(l);
                            drawUpdate = true;
                        }
                        else
                        {
                            ++l;
                        }
                    }
                }
            }
            if (sizeUpdate)
            {
                setSizeUpdate();
            }
            if (drawUpdate)
            {
                setDrawUpdate();
            }
        }

        void TimelineItem::drawEvent(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            IMouseWidget::drawEvent(drawRect, event);
            _visibleUpdate(drawRect);
            _requestsUpdate();
            _drawItems(drawRect, event);
        }

        void TimelineItem::drawOverlayEvent(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            IMouseWidget::drawOverlayEvent(drawRect, event);
            FTK_P();

            const ftk::Box2I& g = getGeometry();

            int y =
                p.size.scrollArea.min.y +
                g.min.y;
            int h =
                p.size.margin +
                p.size.fontMetrics.lineHeight +
                p.size.margin +
                p.size.border * 4;
            event.render->drawRect(
                ftk::Box2I(g.min.x, y, g.w(), h),
                event.style->getColorRole(ftk::ColorRole::Base));

            y = y + h;
            h = p.size.border;
            event.render->drawRect(
                ftk::Box2I(g.min.x, y, g.w(), h),
                event.style->getColorRole(ftk::ColorRole::Border));

            _drawInOutPoints(drawRect, event);
            _drawFrameMarkers(drawRect, event);
            _drawCacheInfo(drawRect, event);
            _drawTimeLabels(drawRect, event);
            _drawTimeTicks(drawRect, event);
            _drawCurrentTime(drawRect, event);
        }

        void TimelineItem::mouseMoveEvent(ftk::MouseMoveEvent& event)
        {
            IMouseWidget::mouseMoveEvent(event);
            FTK_P();
            switch (p.mouseMode)
            {
            case Private::MouseMode::CurrentTime:
            {
                const OTIO_NS::RationalTime time = posToTime(event.pos.x);
                p.timeScrub->setIfChanged(time);
                p.player->seek(time);
                break;
            }
            default: break;
            }
        }

        void TimelineItem::mousePressEvent(ftk::MouseClickEvent& event)
        {
            IMouseWidget::mousePressEvent(event);
            FTK_P();
            takeKeyFocus();
            if (_options.inputEnabled &&
                ftk::MouseButton::Left == event.button &&
                0 == event.modifiers)
            {
                p.mouseMode = Private::MouseMode::CurrentTime;
                if (p.stopOnScrub)
                {
                    p.player->stop();
                }
                const OTIO_NS::RationalTime time = posToTime(event.pos.x);
                p.scrub->setIfChanged(true);
                p.timeScrub->setIfChanged(time);
                p.player->seek(time);
            }
        }

        void TimelineItem::mouseReleaseEvent(ftk::MouseClickEvent& event)
        {
            IMouseWidget::mouseReleaseEvent(event);
            FTK_P();
            p.scrub->setIfChanged(false);
            p.mouseMode = Private::MouseMode::None;
        }

        void TimelineItem::_timeUnitsUpdate()
        {
            _textUpdate();
            setSizeUpdate();
            setDrawUpdate();
        }

        const OTIO_NS::TimeRange& TimelineItem::getTimeRange() const
        {
            return _timeRange;
        }

        void TimelineItem::setOptions(const ItemOptions& value)
        {
            _options = value;
        }

        OTIO_NS::RationalTime TimelineItem::posToTime(float value) const
        {
            // Before the widget is laid out there is no position to read,
            // so the start of the range stands in: it is inside the range,
            // which everything downstream assumes.
            OTIO_NS::RationalTime out = _timeRange.start_time();
            const ftk::Box2I& g = getGeometry();
            if (g.w() > 0)
            {
                const double normalized = (value - g.min.x) /
                    static_cast<double>(_timeRange.duration().rescaled_to(1.0).value() * _scale);
                out = OTIO_NS::RationalTime(
                    _timeRange.start_time() +
                    OTIO_NS::RationalTime(
                        _timeRange.duration().value() * normalized,
                        _timeRange.duration().rate())).
                    round();
                out = ftk::clamp(
                    out,
                    _timeRange.start_time(),
                    _timeRange.end_time_inclusive());
            }
            return out;
        }

        int TimelineItem::timeToPos(const OTIO_NS::RationalTime& value) const
        {
            const ftk::Box2I& g = getGeometry();
            const OTIO_NS::RationalTime t = value - _timeRange.start_time();
            return g.min.x + t.rescaled_to(1.0).value() * _scale;
        }

        ftk::Box2I TimelineItem::_getClipRect(
            const ftk::Box2I& value,
            double scale)
        {
            ftk::Box2I out;
            const ftk::V2I c = ftk::center(value);
            out.min.x = (value.min.x - c.x) * scale + c.x;
            out.min.y = (value.min.y - c.y) * scale + c.y;
            out.max.x = (value.max.x - c.x) * scale + c.x;
            out.max.y = (value.max.y - c.y) * scale + c.y;
            return out;
        }

        std::string TimelineItem::_timeLabel(
            const OTIO_NS::RationalTime& value) const
        {
            // A position, so it is named in the media's time. The frame the
            // ruler counts off is not the frame the media calls it when the
            // sequence was built out of the frames it has.
            return _data->timeUnitsModel->getLabel(
                _data->toMediaTime ? _data->toMediaTime(value) : value);
        }

        std::string TimelineItem::_getDurationLabel(const OTIO_NS::RationalTime& value) const
        {
            const OTIO_NS::RationalTime rescaled = value.rescaled_to(_data->speed);
            return ftk::Format("{0}").
                arg(_data->timeUnitsModel->getLabel(rescaled));
        }

        bool TimelineItem::_isTrackVisible(int index) const
        {
            FTK_P();
            bool out = true;
            if (_displayOptions.minimize)
            {
                out &= index == p.firstVideoTrack || index == p.firstAudioTrack;
            }
            return out;
        }

        ftk::Size2I TimelineItem::_getLabelMaxSize(
            const std::shared_ptr<ftk::FontSystem>& fontSystem) const
        {
            FTK_P();
            const std::string labelMax = _data->timeUnitsModel->getLabel(_timeRange.duration());
            const ftk::Size2I labelMaxSize = fontSystem->getSize(labelMax, p.size.fontInfo);
            return labelMaxSize;
        }

        double TimelineItem::_getSecondsInc(
            const std::shared_ptr<ftk::FontSystem>& fontSystem)
        {
            FTK_P();
            double out = 0.0;
            const int w = getSizeHint().w;
            const double duration = _timeRange.duration().rescaled_to(1.0).value();
            const int secondsTick = 1.0 / duration * w;
            const int minutesTick = 60.0 / duration * w;
            const int hoursTick = 3600.0 / duration * w;
            const ftk::Size2I labelMaxSize = _getLabelMaxSize(fontSystem);
            const int distanceMin = p.size.border + p.size.margin + labelMaxSize.w;
            if (secondsTick >= distanceMin)
            {
                out = 1.0;
            }
            else if (minutesTick >= distanceMin)
            {
                out = 60.0;
            }
            else if (hoursTick >= distanceMin)
            {
                out = 3600.0;
            }
            return out;
        }

        void TimelineItem::_itemsInit(const std::shared_ptr<ftk::Context>& context)
        {
            FTK_P();

            auto timeline = p.player->getTimeline();
            const auto otioTimeline = timeline->getTimeline();
            for (const auto& child : otioTimeline->tracks()->children())
            {
                auto otioTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(child);
                if (!otioTrack)
                    continue;

                Private::Track track;
                track.index = static_cast<int>(p.tracks.size());
                std::string trackLabel = otioTrack->name();
                std::string screenshotTag;
                if (OTIO_NS::Track::Kind::video == otioTrack->kind())
                {
                    track.type = TrackType::Video;
                    if (trackLabel.empty())
                    {
                        trackLabel = "Video Track";
                    }
                    if (-1 == p.firstVideoTrack)
                    {
                        p.firstVideoTrack = track.index;
                        screenshotTag = "Timeline.VideoTrack";
                    }
                }
                else if (OTIO_NS::Track::Kind::audio == otioTrack->kind())
                {
                    track.type = TrackType::Audio;
                    if (trackLabel.empty())
                    {
                        trackLabel = "Audio Track";
                    }
                    if (-1 == p.firstAudioTrack)
                    {
                        p.firstAudioTrack = track.index;
                        screenshotTag = "Timeline.AudioTrack";
                    }
                }
                track.timeRange = otioTrack->trimmed_range();
                track.label = ftk::Label::create(
                    context,
                    trackLabel,
                    shared_from_this());
                track.label->setMarginRole(ftk::SizeRole::MarginInside);
                track.label->setEnabled(otioTrack->enabled());
                if (!screenshotTag.empty())
                {
                    ftk::setScreenshotTag(track.label, screenshotTag);
                }
                track.durationLabel = ftk::Label::create(
                    context,
                    shared_from_this());
                track.durationLabel->setMarginRole(ftk::SizeRole::MarginInside);
                track.durationLabel->setEnabled(otioTrack->enabled());

                // OTIO works out an item's range in its track by summing the
                // duration of every preceding sibling, so asking each child
                // for its own range makes building the items quadratic: twenty
                // thousand clips took seconds and a hundred thousand never
                // finished. One pass gives the same answer for all of them.
                std::map<const OTIO_NS::Composable*, OTIO_NS::TimeRange> childRanges;
                {
                    OTIO_NS::ErrorStatus errorStatus;
                    for (const auto& i :
                        otioTrack->range_of_all_children(&errorStatus))
                    {
                        if (const auto trimmed =
                            otioTrack->trim_child_range(i.second))
                        {
                            childRanges[i.first] = trimmed.value();
                        }
                    }
                }

                for (const auto& trackChild : otioTrack->children())
                {
                    Private::Item item;
                    const OTIO_NS::Item* otioItem = nullptr;
                    if (auto clip = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Clip>(trackChild))
                    {
                        switch (track.type)
                        {
                        case TrackType::Video:
                            item.type = ItemType::Video;
                            item.defaultColor = ftk::Color4F(.2F, .4F, .4F);
                            break;
                        case TrackType::Audio:
                            item.type = ItemType::Audio;
                            item.defaultColor = ftk::Color4F(.3F, .25F, .4F);
                            break;
                        default:
                            continue;
                        }
                        otioItem = clip.value;

                        // Resolved through the timeline rather than taken from
                        // the clip, so that the item follows the media
                        // reference key instead of the reference the clip was
                        // authored with.
                        const auto mediaReference = timeline->getMediaReference(clip.value);
                        item.path = getPath(
                            mediaReference,
                            _data->dir,
                            _data->options.pathOptions);
                        item.timelinePath = timeline->getPath();
                        item.label = !clip->name().empty() ?
                            clip->name() :
                            item.path.getFileName();
                        item.ioOptions = _data->options.ioOptions;
                        if (ItemType::Video == item.type)
                        {
                            item.ioOptions["USD/CameraName"] = clip->name();
                        }
                    }
                    else if (auto gap = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Gap>(trackChild))
                    {
                        item.type = ItemType::Gap;
                        item.defaultColor = TrackType::Video == track.type ?
                            ftk::Color4F(.25F, .31F, .31F) :
                            ftk::Color4F(.25F, .24F, .3F);
                        item.label = !gap->name().empty() ? gap->name() : "Gap";
                        otioItem = gap.value;
                    }
                    if (!otioItem)
                        continue;

                    if (const auto childRange = childRanges.find(otioItem);
                        childRange != childRanges.end())
                    {
                        item.timeRange = childRange->second;
                    }
                    else if (const auto timeRangeOpt =
                        otioItem->trimmed_range_in_parent())
                    {
                        item.timeRange = timeRangeOpt.value();
                    }
                    item.availableRange = otioItem->available_range();
                    item.trimmedRange = otioItem->trimmed_range();
                    if (const auto color = otioItem->color())
                    {
                        item.otioColor = toColor(color.value());
                    }
                    item.enabled = otioTrack->enabled();
                    track.items.push_back(std::move(item));
                }

                p.tracks.push_back(std::move(track));
            }

            // The items used to carry these tags themselves. Zero size widgets
            // stand in for them so the documentation tool can still point at an
            // example of each kind.
            p.tagProxies.videoClip = ftk::Spacer::create(
                context,
                ftk::Orientation::Horizontal,
                shared_from_this());
            ftk::setScreenshotTag(p.tagProxies.videoClip, "Timeline.VideoClip");
            p.tagProxies.audioClip = ftk::Spacer::create(
                context,
                ftk::Orientation::Horizontal,
                shared_from_this());
            ftk::setScreenshotTag(p.tagProxies.audioClip, "Timeline.AudioClip");
            p.tagProxies.gap = ftk::Spacer::create(
                context,
                ftk::Orientation::Horizontal,
                shared_from_this());
            ftk::setScreenshotTag(p.tagProxies.gap, "Timeline.Gap");
        }

        void TimelineItem::_itemsScaleUpdate()
        {
            FTK_P();
            for (auto& track : p.tracks)
            {
                for (auto& item : track.items)
                {
                    item.x = item.timeRange.start_time().rescaled_to(1.0).value() * _scale;
                    item.w = item.timeRange.duration().rescaled_to(1.0).value() * _scale;
                }
            }
        }

        void TimelineItem::_itemsTextUpdate(const ftk::SizeHintEvent& event)
        {
            FTK_P();
            for (auto& track : p.tracks)
            {
                for (auto& item : track.items)
                {
                    item.labelSize = !_displayOptions.minimize ?
                        event.fontSystem->getSize(item.label, p.size.itemFontInfo) :
                        ftk::Size2I();
                    item.durationSize = !_displayOptions.minimize ?
                        event.fontSystem->getSize(item.durationLabel, p.size.itemFontInfo) :
                        ftk::Size2I();
                    item.labelGlyphs.clear();
                    item.durationGlyphs.clear();
                }
            }
        }

        void TimelineItem::_visibleUpdate(const ftk::Box2I& drawRect)
        {
            FTK_P();
            const ftk::Box2I& g = getGeometry();

            // Ask for a wider band than is drawn so that scrolling does not
            // reveal clips whose thumbnails have not been started yet.
            p.activeRect = _getClipRect(drawRect, _displayOptions.clipRectScale);
            const ftk::Box2I& activeRect = p.activeRect;

            p.activePrev = p.active;
            p.visible.assign(p.tracks.size(), Private::Range());
            p.active.assign(p.tracks.size(), Private::Range());
            for (size_t i = 0; i < p.tracks.size(); ++i)
            {
                const auto& track = p.tracks[i];
                if (!track.visible || track.items.empty())
                    continue;
                p.visible[i] = Private::getRange(
                    track.items,
                    drawRect.min.x - g.min.x,
                    drawRect.max.x - g.min.x);
                p.active[i] = Private::getRange(
                    track.items,
                    activeRect.min.x - g.min.x,
                    activeRect.max.x - g.min.x);
            }
        }

        void TimelineItem::_requestsUpdate()
        {
            FTK_P();
            const ftk::Box2I& g = getGeometry();
            const ftk::Box2I& activeRect = p.activeRect;

            // Let go of the items that have left the band. Only the items on
            // the edges of the band are looked at, so a long timeline does not
            // cost a walk of every item.
            for (size_t i = 0; i < p.activePrev.size() && i < p.active.size(); ++i)
            {
                auto& track = p.tracks[i];
                const auto& prev = p.activePrev[i];
                const auto& active = p.active[i];
                for (size_t j = prev.begin; j < prev.end; ++j)
                {
                    if (j >= active.begin && j < active.end)
                        continue;
                    auto& item = track.items[j];
                    p.cancelRequests(item);
                    item.thumbnails.clear();
                    item.waveforms.clear();
                    item.media.clear();
                }
            }

            for (size_t i = 0; i < p.tracks.size() && i < p.active.size(); ++i)
            {
                auto& track = p.tracks[i];
                for (size_t j = p.active[i].begin; j < p.active[i].end; ++j)
                {
                    auto& item = track.items[j];
                    const bool wantsThumbnails =
                        ItemType::Video == item.type && _displayOptions.thumbnails;
                    const bool wantsWaveforms =
                        ItemType::Audio == item.type && _displayOptions.waveforms;
                    if (!wantsThumbnails && !wantsWaveforms)
                        continue;

                    const ftk::Box2I geom = Private::getGeom(track, item, g.min);
                    const ftk::Box2I insideGeom = Private::getInsideGeom(geom, p.size.border);
                    if (insideGeom.w() <= 0)
                        continue;

                    if (!item.ioInfo.has_value())
                    {
                        if (!item.infoRequest.future.valid())
                        {
                            item.infoRequest = p.thumbnailSystem->getInfo(
                                item.timelinePath,
                                item.path,
                                item.ioOptions);
                        }
                        continue;
                    }

                    const ftk::Box2I mediaGeom = p.getMediaGeom(
                        insideGeom,
                        _displayOptions,
                        wantsThumbnails ?
                        _displayOptions.thumbnailHeight :
                        _displayOptions.waveformHeight);
                    if (wantsThumbnails)
                    {
                        p.requestThumbnails(
                            item,
                            mediaGeom,
                            activeRect,
                            _displayOptions,
                            *_data);
                    }
                    else
                    {
                        p.requestWaveforms(
                            item,
                            mediaGeom,
                            activeRect,
                            _displayOptions,
                            *_data);
                    }
                }
            }
        }

        void TimelineItem::Private::requestThumbnails(
            Item& item,
            const ftk::Box2I& mediaGeom,
            const ftk::Box2I& activeRect,
            const DisplayOptions& displayOptions,
            const ItemData& data)
        {
            item.media.clear();
            const int thumbnailWidth = getThumbnailWidth(item, displayOptions);
            if (thumbnailWidth <= 0 ||
                item.ioInfo->video.empty() ||
                !item.ioInfo->videoTime.has_value())
                return;

            OTIO_NS::TimeRange trimmedRange = item.trimmedRange;
            if (data.options.compat &&
                item.availableRange.start_time() > item.ioInfo->videoTime->start_time())
            {
                //! \bug If the available range is greater than the media time,
                //! assume the media time is wrong (e.g., Picchu) and
                //! compensate for it.
                trimmedRange = OTIO_NS::TimeRange(
                    trimmedRange.start_time() - item.availableRange.start_time(),
                    trimmedRange.duration());
            }

            const int w = mediaGeom.w();
            const double duration = item.timeRange.duration().
                rescaled_to(item.timeRange.start_time()).value();
            if (w <= 0 || duration <= 0.0)
                return;

            // Step whole frames and put each thumbnail where its own frame is.
            // Stepping pixels instead and asking afterwards which frame each one
            // landed on left the image as much as a frame away from the frame it
            // showed: unnoticeable while every thumbnail resolves and they tile
            // into a strip, plain on a sequence where only some of them do.
            const double perFrame = w / duration;
            const int64_t start = static_cast<int64_t>(
                std::floor(item.timeRange.start_time().value()));
            const int64_t end = start + static_cast<int64_t>(duration);
            // Rounded rather than rounded up, so the strip stays about as dense
            // as it was: a step that comes out short overlaps the one before by
            // less than a frame, which reads better than a gap.
            const int64_t frameStep = std::max(
                static_cast<int64_t>(1),
                static_cast<int64_t>(std::round(thumbnailWidth / perFrame)));

            std::map<OTIO_NS::RationalTime, std::shared_ptr<ftk::Image> > thumbnails;
            for (int64_t frame = start; frame < end; frame += frameStep)
            {
                const int x = static_cast<int>(
                    std::round((frame - start) * perFrame));
                const ftk::Box2I box(
                    mediaGeom.min.x + x,
                    mediaGeom.min.y,
                    thumbnailWidth,
                    displayOptions.thumbnailHeight);
                if (!ftk::intersects(box, activeRect))
                    continue;

                const OTIO_NS::RationalTime time(
                    static_cast<double>(frame),
                    item.timeRange.start_time().rate());
                const OTIO_NS::RationalTime mediaTime = toVideoMediaTime(
                    time,
                    item.timeRange,
                    trimmedRange,
                    item.ioInfo->videoTime->duration().rate());

                Item::Media media;
                media.x = x;
                media.w = thumbnailWidth;
                if (const auto i = item.thumbnails.find(mediaTime);
                    i != item.thumbnails.end())
                {
                    media.image = i->second;
                    thumbnails[mediaTime] = i->second;
                }
                else if (item.thumbnailRequests.find(mediaTime) == item.thumbnailRequests.end())
                {
                    item.thumbnailRequests[mediaTime] = thumbnailSystem->getThumbnail(
                        item.timelinePath,
                        item.path,
                        displayOptions.thumbnailHeight,
                        mediaTime,
                        item.ioOptions);
                }
                item.media.push_back(std::move(media));
            }
            item.thumbnails = std::move(thumbnails);
        }

        void TimelineItem::Private::requestWaveforms(
            Item& item,
            const ftk::Box2I& mediaGeom,
            const ftk::Box2I& activeRect,
            const DisplayOptions& displayOptions,
            const ItemData& data)
        {
            item.media.clear();
            if (displayOptions.waveformWidth <= 0 || !item.ioInfo->audio.isValid())
                return;

            OTIO_NS::TimeRange trimmedRange = item.trimmedRange;
            if (data.options.compat &&
                item.ioInfo->audioTime.has_value() &&
                trimmedRange.start_time() < item.ioInfo->audioTime->start_time())
            {
                //! \bug If the trimmed range is less than the media time,
                //! assume the media time is wrong (e.g., ALab trailer) and
                //! compensate for it.
                trimmedRange = OTIO_NS::TimeRange(
                    item.ioInfo->audioTime->start_time() + trimmedRange.start_time(),
                    trimmedRange.duration());
            }

            const int w = mediaGeom.w();
            std::map<OTIO_NS::RationalTime, std::shared_ptr<ftk::TriMesh2F> > waveforms;
            for (int x = 0; x < w; x += displayOptions.waveformWidth)
            {
                // The chunk is clipped to the end of the item rather than being
                // drawn past it and masked, so that all of a track's waveforms
                // can go into one mesh.
                const int width = std::min(displayOptions.waveformWidth, w - x);
                const ftk::Box2I box(
                    mediaGeom.min.x + x,
                    mediaGeom.min.y,
                    width,
                    displayOptions.waveformHeight);
                if (!ftk::intersects(box, activeRect))
                    continue;

                const OTIO_NS::RationalTime time = OTIO_NS::RationalTime(
                    item.timeRange.start_time().value() +
                    (x / static_cast<double>(w)) *
                    item.timeRange.duration().rescaled_to(item.timeRange.start_time()).value(),
                    item.timeRange.start_time().rate()).
                    round();
                const OTIO_NS::RationalTime time2 = OTIO_NS::RationalTime(
                    item.timeRange.start_time().value() +
                    ((x + width) / static_cast<double>(w)) *
                    item.timeRange.duration().rescaled_to(item.timeRange.start_time()).value(),
                    item.timeRange.start_time().rate()).
                    round();
                const OTIO_NS::TimeRange mediaRange = toAudioMediaTime(
                    OTIO_NS::TimeRange::range_from_start_end_time(time, time2),
                    item.timeRange,
                    trimmedRange,
                    item.ioInfo->audio.sampleRate);
                Item::Media media;
                media.x = x;
                media.w = width;
                if (const auto i = item.waveforms.find(mediaRange.start_time());
                    i != item.waveforms.end())
                {
                    media.mesh = i->second;
                    waveforms[mediaRange.start_time()] = i->second;
                }
                else if (item.waveformRequests.find(mediaRange.start_time()) == item.waveformRequests.end())
                {
                    item.waveformRequests[mediaRange.start_time()] = thumbnailSystem->getWaveform(
                        item.timelinePath,
                        item.path,
                        ftk::Size2I(width, displayOptions.waveformHeight),
                        mediaRange,
                        data.options.ioOptions);
                }
                item.media.push_back(std::move(media));
            }
            item.waveforms = std::move(waveforms);
        }

        void TimelineItem::_cancelRequests()
        {
            FTK_P();
            // One cancellation for the whole timeline: cancelling per item cost
            // a walk of every pending request each time, which on a hundred
            // thousand clips took longer than the rest of shutdown.
            std::vector<uint64_t> ids;
            for (auto& track : p.tracks)
            {
                for (auto& item : track.items)
                {
                    p.takeRequests(item, ids);
                    item.thumbnails.clear();
                    item.waveforms.clear();
                    item.media.clear();
                }
            }
            if (!ids.empty())
            {
                p.thumbnailSystem->cancelRequests(ids);
            }
            p.active.assign(p.tracks.size(), Private::Range());
            p.activePrev = p.active;
        }

        void TimelineItem::_drawItems(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();
            const ftk::Box2I& g = getGeometry();
            const bool enabled = isEnabled();

            clearMesh(p.draw.items);
            clearMesh(p.draw.mediaBackgrounds);
            clearMesh(p.draw.waveforms);

            // The rectangles for every item in view go into one mesh with a
            // color per vertex, and the media backgrounds and waveforms into one
            // mesh each. Three draw calls cover the whole timeline; a widget per
            // item cost two or three each.
            for (size_t i = 0; i < p.tracks.size() && i < p.visible.size(); ++i)
            {
                const auto& track = p.tracks[i];
                for (size_t j = p.visible[i].begin; j < p.visible[i].end; ++j)
                {
                    const auto& item = track.items[j];
                    const ftk::Box2I geom = Private::getGeom(track, item, g.min);
                    const ftk::Box2I insideGeom = Private::getInsideGeom(geom, p.size.border);
                    const ftk::Color4F color = Private::getColor(
                        item,
                        _displayOptions,
                        enabled && item.enabled);
                    addRect(p.draw.items, insideGeom, &color);

                    if (ItemType::Video == item.type && _displayOptions.thumbnails)
                    {
                        addRect(
                            p.draw.mediaBackgrounds,
                            p.getMediaGeom(
                                insideGeom,
                                _displayOptions,
                                _displayOptions.thumbnailHeight));
                    }
                    else if (ItemType::Audio == item.type && _displayOptions.waveforms)
                    {
                        const ftk::Box2I mediaGeom = p.getMediaGeom(
                            insideGeom,
                            _displayOptions,
                            _displayOptions.waveformHeight);
                        addRect(p.draw.mediaBackgrounds, mediaGeom);
                        for (const auto& media : item.media)
                        {
                            if (!media.mesh)
                                continue;
                            const ftk::Box2I box(
                                mediaGeom.min.x + media.x,
                                mediaGeom.min.y,
                                media.w,
                                _displayOptions.waveformHeight);
                            if (!ftk::intersects(box, drawRect))
                                continue;
                            addMesh(
                                p.draw.waveforms,
                                *media.mesh,
                                ftk::V2F(box.min.x, box.min.y));
                        }
                    }
                }
            }

            if (!p.draw.items.triangles.empty())
            {
                event.render->drawColorMesh(p.draw.items);
            }
            if (!p.draw.mediaBackgrounds.triangles.empty())
            {
                event.render->drawMesh(
                    p.draw.mediaBackgrounds,
                    ftk::Color4F(0.F, 0.F, 0.F));
            }
            if (!p.draw.waveforms.triangles.empty())
            {
                event.render->drawMesh(
                    p.draw.waveforms,
                    ftk::Color4F(1.F, 1.F, 1.F));
            }

            // Thumbnails are one draw call each because each is its own
            // texture, but they are cached by the renderer instead of being
            // uploaded again on every frame.
            if (_displayOptions.thumbnails)
            {
                _drawThumbnails(drawRect, event);
            }

            if (!_displayOptions.minimize)
            {
                _drawItemLabels(drawRect, event);
            }
        }

        void TimelineItem::_drawThumbnails(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();
            auto render = std::dynamic_pointer_cast<IRender>(event.render);
            if (!render)
                return;
            const ftk::Box2I& g = getGeometry();

            std::unique_ptr<ftk::ClipRectEnabledState> clipRectEnabledState;
            std::unique_ptr<ftk::ClipRectState> clipRectState;
            ftk::ImageOptions imageOptions;

            // Let the renderer cache the thumbnail textures, so the same
            // thumbnail is uploaded once rather than on every frame.
            //
            // The video draw path deliberately does the opposite, because a
            // decoded frame is a new image every time and caching it would only
            // fill the cache with textures that are never looked up again.
            // Thumbnails are the other case: the same images are held for as
            // long as their items are in view and drawn on every frame, so they
            // are worth keying a cache on. On a 198 item timeline with the
            // tracks expanded this is the difference between 3ms and 15ms a
            // frame, for about 5MB of texture memory.
            imageOptions.cache = true;

            for (size_t i = 0; i < p.tracks.size() && i < p.visible.size(); ++i)
            {
                const auto& track = p.tracks[i];
                if (TrackType::Video != track.type)
                    continue;
                for (size_t j = p.visible[i].begin; j < p.visible[i].end; ++j)
                {
                    const auto& item = track.items[j];
                    if (ItemType::Video != item.type || item.media.empty())
                        continue;

                    const ftk::Box2I geom = Private::getGeom(track, item, g.min);
                    const ftk::Box2I insideGeom = Private::getInsideGeom(geom, p.size.border);
                    const ftk::Box2I mediaGeom = p.getMediaGeom(
                        insideGeom,
                        _displayOptions,
                        _displayOptions.thumbnailHeight);

                    for (const auto& media : item.media)
                    {
                        if (!media.image)
                            continue;
                        const ftk::Box2I box(
                            mediaGeom.min.x + media.x,
                            mediaGeom.min.y,
                            media.w,
                            _displayOptions.thumbnailHeight);
                        if (!ftk::intersects(box, drawRect))
                            continue;

                        if (!clipRectEnabledState)
                        {
                            clipRectEnabledState.reset(new ftk::ClipRectEnabledState(render));
                            clipRectState.reset(new ftk::ClipRectState(render));
                            render->setClipRectEnabled(true);
                            render->setOCIOOptions(_displayOptions.ocio);
                            render->setLUTOptions(_displayOptions.lut);
                        }

                        // A thumbnail at the end of an item runs past it, so
                        // the item masks its own thumbnails.
                        render->setClipRect(ftk::intersect(
                            mediaGeom,
                            ftk::intersect(clipRectState->getClipRect(), drawRect)));

                        render->drawImage(
                            media.image,
                            box,
                            ftk::Color4F(1.F, 1.F, 1.F),
                            imageOptions);
                    }
                }
            }
        }

        void TimelineItem::_drawItemLabels(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();
            const ftk::Box2I& g = getGeometry();
            const bool enabled = isEnabled();

            for (size_t i = 0; i < p.tracks.size() && i < p.visible.size(); ++i)
            {
                auto& track = p.tracks[i];
                for (size_t j = p.visible[i].begin; j < p.visible[i].end; ++j)
                {
                    auto& item = track.items[j];
                    const ftk::Box2I geom = Private::getGeom(track, item, g.min);
                    const ftk::Box2I insideGeom = Private::getInsideGeom(geom, p.size.border);
                    const ftk::Box2I labelGeom(
                        insideGeom.min.x + p.size.margin,
                        insideGeom.min.y + p.size.margin,
                        item.labelSize.w,
                        p.size.itemFontMetrics.lineHeight);
                    const ftk::Box2I durationGeom(
                        insideGeom.max.x - item.durationSize.w - p.size.margin,
                        insideGeom.min.y + p.size.margin,
                        item.durationSize.w,
                        p.size.itemFontMetrics.lineHeight);

                    // The text is masked to the item, so a label that falls
                    // outside it draws nothing. Narrow items are the common
                    // case in a long timeline, and testing this rather than
                    // just the draw rectangle keeps them from costing a draw
                    // call each to render nothing.
                    const ftk::Box2I textRect = ftk::intersect(insideGeom, drawRect);
                    const bool drawLabel = ftk::intersects(labelGeom, textRect);
                    const bool drawDuration =
                        ftk::intersects(durationGeom, textRect) &&
                        !ftk::intersects(durationGeom, labelGeom);
                    if (!drawLabel && !drawDuration)
                        continue;

                    std::unique_ptr<ftk::ClipRectEnabledState> clipRectEnabledState;
                    std::unique_ptr<ftk::ClipRectState> clipRectState;
                    if (!ftk::contains(insideGeom, labelGeom) ||
                        !ftk::contains(insideGeom, durationGeom))
                    {
                        clipRectEnabledState.reset(new ftk::ClipRectEnabledState(event.render));
                        clipRectState.reset(new ftk::ClipRectState(event.render));
                        event.render->setClipRectEnabled(true);
                        event.render->setClipRect(textRect);
                    }

                    const ftk::Color4F color = event.style->getColorRole(
                        (enabled && item.enabled) ?
                        ftk::ColorRole::Text :
                        ftk::ColorRole::TextDisabled);
                    if (drawLabel)
                    {
                        if (!item.label.empty() && item.labelGlyphs.empty())
                        {
                            item.labelGlyphs = event.fontSystem->getGlyphs(
                                item.label,
                                p.size.itemFontInfo);
                        }
                        event.render->drawText(
                            item.labelGlyphs,
                            p.size.itemFontMetrics,
                            labelGeom.min,
                            color);
                    }
                    if (drawDuration)
                    {
                        if (!item.durationLabel.empty() && item.durationGlyphs.empty())
                        {
                            item.durationGlyphs = event.fontSystem->getGlyphs(
                                item.durationLabel,
                                p.size.itemFontInfo);
                        }
                        event.render->drawText(
                            item.durationGlyphs,
                            p.size.itemFontMetrics,
                            durationGeom.min,
                            color);
                    }
                }
            }
        }

        void TimelineItem::_drawInOutPoints(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();
            if (_p->inOutRange.has_value() &&
                !compareExact(*_p->inOutRange, _timeRange))
            {
                const ftk::Box2I& g = getGeometry();
                const ftk::Color4F color(.4F, .5F, .9F);

                const int h = p.size.border * 2;
                switch (_displayOptions.inOutDisplay)
                {
                case InOutDisplay::InsideRange:
                {
                    const int x0 = timeToPos(_p->inOutRange->start_time());
                    const int x1 = timeToPos(_p->inOutRange->end_time_exclusive());
                    const ftk::Box2I box(
                        x0,
                        p.size.scrollArea.min.y +
                        g.min.y,
                        x1 - x0 + 1,
                        h);
                    event.render->drawRect(box, color);
                    break;
                }
                case InOutDisplay::OutsideRange:
                {
                    int x0 = timeToPos(_timeRange.start_time());
                    int x1 = timeToPos(_p->inOutRange->start_time());
                    ftk::Box2I box(
                        x0,
                        p.size.scrollArea.min.y +
                        g.min.y,
                        x1 - x0 + 1,
                        h);
                    event.render->drawRect(box, color);
                    x0 = timeToPos(_p->inOutRange->end_time_exclusive());
                    x1 = timeToPos(_timeRange.end_time_exclusive());
                    box = ftk::Box2I(
                        x0,
                        p.size.scrollArea.min.y +
                        g.min.y,
                        x1 - x0 + 1,
                        h);
                    event.render->drawRect(box, color);
                    break;
                }
                default: break;
                }
            }
        }

        void TimelineItem::_drawFrameMarkers(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();
            const ftk::Box2I& g = getGeometry();
            const double rate = _timeRange.duration().rate();
            const ftk::Color4F color(.6F, .4F, .2F);
            std::vector<ftk::Box2I> rects;
            for (const auto& frameMarker : p.frameMarkers)
            {
                const ftk::Box2I g2(
                    timeToPos(OTIO_NS::RationalTime(frameMarker, rate)),
                    p.size.scrollArea.min.y +
                    g.min.y,
                    p.size.border * 2,
                    p.size.margin +
                    p.size.fontMetrics.lineHeight +
                    p.size.margin +
                    p.size.border * 4);
                if (ftk::intersects(g2, drawRect))
                {
                    rects.emplace_back(g2);
                }
            }
            if (!rects.empty())
            {
                event.render->drawRects(rects, color);
            }
        }

        void TimelineItem::_drawCacheInfo(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();

            const ftk::Box2I& g = getGeometry();

            // Draw the video cache.
            if (CacheDisplay::VideoAndAudio == _displayOptions.cacheDisplay ||
                CacheDisplay::VideoOnly == _displayOptions.cacheDisplay)
            {
                ftk::TriMesh2F& mesh = p.draw.cache;
                clearMesh(mesh);
                for (const auto& t : p.cacheInfo.video)
                {
                    const int x0 = timeToPos(t.start_time());
                    const int x1 = timeToPos(t.end_time_exclusive());
                    const int h = CacheDisplay::VideoAndAudio == _displayOptions.cacheDisplay ?
                        p.size.border * 2 :
                        p.size.border * 4;
                    const ftk::Box2I box(
                        x0,
                        p.size.scrollArea.min.y +
                        g.min.y +
                        p.size.margin +
                        p.size.fontMetrics.lineHeight +
                        p.size.margin,
                        x1 - x0 + 1,
                        h);
                    if (ftk::intersects(box, drawRect))
                    {
                        addRect(mesh, box);
                    }
                }
                if (!mesh.v.empty())
                {
                    event.render->drawMesh(mesh, ftk::Color4F(.2F, .4F, .4F));
                }
            }

            // Draw the audio cache.
            if (CacheDisplay::VideoAndAudio == _displayOptions.cacheDisplay)
            {
                ftk::TriMesh2F& mesh = p.draw.cache;
                clearMesh(mesh);
                for (const auto& t : p.cacheInfo.audio)
                {
                    const int x0 = timeToPos(t.start_time());
                    const int x1 = timeToPos(t.end_time_exclusive());
                    const ftk::Box2I box(
                        x0,
                        p.size.scrollArea.min.y +
                        g.min.y +
                        p.size.margin +
                        p.size.fontMetrics.lineHeight +
                        p.size.margin +
                        p.size.border * 2,
                        x1 - x0 + 1,
                        p.size.border * 2);
                    if (ftk::intersects(box, drawRect))
                    {
                        addRect(mesh, box);
                    }
                }
                if (!mesh.v.empty())
                {
                    event.render->drawMesh(mesh, ftk::Color4F(.3F, .25F, .4F));
                }
            }
        }

        void TimelineItem::_drawTimeLabels(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();
            if (_timeRange.duration().value() > 0.0)
            {
                const ftk::Box2I& g = getGeometry();
                const double rate = _timeRange.duration().rate();
                const double seconds = _getSecondsInc(event.fontSystem);
                if (seconds > 0.0)
                {
                    const double t0 = std::floor(posToTime(drawRect.min.x).rescaled_to(1.0).value() / seconds) * seconds;
                    const double t1 = std::ceil(posToTime(drawRect.max.x).rescaled_to(1.0).value() / seconds) * seconds;
                    for (double t = t0; t <= t1; t += seconds)
                    {
                        const int x = timeToPos(OTIO_NS::RationalTime(t, 1.0));
                        const std::string label = _timeLabel(
                            OTIO_NS::RationalTime(t, 1.0).rescaled_to(rate));
                        event.render->drawText(
                            event.fontSystem->getGlyphs(label, p.size.fontInfo),
                            p.size.fontMetrics,
                            ftk::V2I(
                                x +
                                p.size.border +
                                p.size.margin,
                                p.size.scrollArea.min.y +
                                g.min.y +
                                p.size.margin),
                            event.style->getColorRole(ftk::ColorRole::TextDisabled));
                    }
                }
            }
        }

        void TimelineItem::_drawTimeTicks(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();
            if (_timeRange.duration().value() > 0.0)
            {
                const ftk::Box2I& g = getGeometry();
                const int w = getSizeHint().w;
                const double duration = _timeRange.duration().rescaled_to(1.0).value();
                std::vector<ftk::Box2I> rects;

                // Compute the frame ticks.
                const int frameTick = 1.0 / _timeRange.duration().value() * w;
                if (duration > 0.0 && frameTick >= p.size.handle)
                {
                    const OTIO_NS::RationalTime t0 = posToTime(drawRect.min.x);
                    const OTIO_NS::RationalTime t1 = posToTime(drawRect.max.x);
                    const OTIO_NS::RationalTime inc(1.0, _timeRange.duration().rate());
                    for (OTIO_NS::RationalTime t = t0; t <= t1; t += inc)
                    {
                        const int x = timeToPos(t);
                        rects.emplace_back(ftk::Box2I(
                            x,
                            p.size.scrollArea.min.y +
                            g.min.y +
                            p.size.margin +
                            p.size.fontMetrics.lineHeight,
                            p.size.border,
                            p.size.margin +
                            p.size.border * 4));
                    }
                }

                // Compute the time ticks.
                const double seconds = _getSecondsInc(event.fontSystem);
                if (duration > 0.0 && seconds > 0.0)
                {
                    const double t0 = std::floor(posToTime(drawRect.min.x).rescaled_to(1.0).value() / seconds) * seconds;
                    const double t1 = std::ceil(posToTime(drawRect.max.x).rescaled_to(1.0).value() / seconds) * seconds;
                    for (double t = t0; t <= t1; t += seconds)
                    {
                        const int x = timeToPos(OTIO_NS::RationalTime(t, 1.0));
                        rects.emplace_back(ftk::Box2I(
                            x,
                            p.size.scrollArea.min.y +
                            g.min.y +
                            p.size.margin +
                            p.size.fontMetrics.lineHeight,
                            p.size.border,
                            p.size.margin +
                            p.size.fontMetrics.lineHeight +
                            p.size.margin +
                            p.size.border * 4));
                    }
                }

                // Draw the ticks.
                if (!rects.empty())
                {
                    event.render->drawRects(
                        rects,
                        event.style->getColorRole(ftk::ColorRole::TextDisabled));
                }
            }
        }

        void TimelineItem::_drawCurrentTime(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();

            const ftk::Box2I& g = getGeometry();

            if (p.currentTime.has_value())
            {
                const ftk::V2I pos(
                    timeToPos(*p.currentTime),
                    p.size.scrollArea.min.y +
                    g.min.y);

                event.render->drawRect(
                    ftk::Box2I(
                        pos.x,
                        pos.y,
                        p.size.border * 2,
                        g.h()),
                    event.style->getColorRole(ftk::ColorRole::Red));

                const std::string label = _timeLabel(*p.currentTime);
                ftk::V2I labelPos(
                    pos.x + p.size.border * 2 + p.size.margin,
                    pos.y + p.size.margin);
                const ftk::Size2I labelSize = event.fontSystem->getSize(label, p.size.fontInfo);
                const ftk::Box2I g2(p.size.scrollArea.min + g.min, p.size.scrollArea.size());
                if (labelPos.x + labelSize.w > g2.max.x)
                {
                    const ftk::V2I labelPos2(
                        pos.x - p.size.border * 2 - p.size.margin - labelSize.w,
                        pos.y + p.size.margin);
                    if (labelPos2.x > g2.min.x)
                    {
                        labelPos = labelPos2;
                    }
                }
                event.render->drawText(
                    event.fontSystem->getGlyphs(label, p.size.fontInfo),
                    p.size.fontMetrics,
                    labelPos,
                    event.style->getColorRole(ftk::ColorRole::Text));
            }
        }

        void TimelineItem::_tracksUpdate()
        {
            FTK_P();
            for (const auto& track : p.tracks)
            {
                const bool visible = _isTrackVisible(track.index);
                track.label->setVisible(!_displayOptions.minimize && visible);
                track.durationLabel->setVisible(!_displayOptions.minimize && visible);
            }
        }

        void TimelineItem::_textUpdate()
        {
            FTK_P();
            for (auto& track : p.tracks)
            {
                const OTIO_NS::RationalTime duration = track.timeRange.duration();
                const bool khz =
                    TrackType::Audio == track.type ?
                    (duration.rate() >= 1000.0) :
                    false;
                const OTIO_NS::RationalTime rescaled = duration.rescaled_to(_data->speed);
                const std::string label = ftk::Format("{0}, {1}{2}").
                    arg(_data->timeUnitsModel->getLabel(rescaled)).
                    arg(khz ? (duration.rate() / 1000.0) : duration.rate()).
                    arg(khz ? "kHz" : "FPS");
                track.durationLabel->setText(label);

                for (auto& item : track.items)
                {
                    item.durationLabel = _getDurationLabel(item.timeRange.duration());
                }
            }
            p.size.init = true;
            setSizeUpdate();
            setDrawUpdate();
        }
    }
}

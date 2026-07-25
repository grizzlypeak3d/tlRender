// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/AudioClipItem.h>

#include <tlRender/UI/ThumbnailSystem.h>

#include <tlRender/Timeline/Util.h>

#include <ftk/UI/DrawUtil.h>
#include <ftk/Core/RenderUtil.h>

namespace tl
{
    namespace ui
    {
        struct AudioClipItem::Private
        {
            ftk::Path path;
            std::vector<ftk::MemFile> memRead;
            std::shared_ptr<ThumbnailSystem> thumbnailSystem;

            struct SizeData
            {
                int dragLength = 0;
                ftk::Box2I clipRect;
            };
            SizeData size;

            InfoRequest infoRequest;
            std::optional<IOInfo> ioInfo;
            std::map<OTIO_NS::RationalTime, WaveformRequest> waveformRequests;
            std::map< OTIO_NS::RationalTime, std::shared_ptr<ftk::TriMesh2F> > waveforms;
        };

        void AudioClipItem::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<Timeline>& timeline,
            const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip>& clip,
            double scale,
            const ItemOptions& options,
            const DisplayOptions& displayOptions,
            const std::shared_ptr<ItemData>& itemData,
            const std::shared_ptr<IWidget>& parent)
        {
            // Resolved through the timeline rather than taken from the clip,
            // so that the item follows the media reference key instead of the
            // reference the clip was authored with.
            const auto mediaReference = timeline->getMediaReference(clip.value);
            const auto path = getPath(
                mediaReference,
                itemData->dir,
                itemData->options.pathOptions);
            IBasicItem::_init(
                context,
                !clip->name().empty() ? clip->name() : path.getFileName(),
                getItemColor(clip.value, ftk::Color4F(.3F, .25F, .4F), displayOptions),
                "tl::ui::AudioClipItem",
                clip.value,
                scale,
                options,
                displayOptions,
                itemData,
                parent);
            FTK_P();

            p.path = path;
            p.memRead = timeline->getMem(mediaReference);
            p.thumbnailSystem = context->getSystem<ThumbnailSystem>();
        }

        AudioClipItem::AudioClipItem() :
            _p(new Private)
        {}

        AudioClipItem::~AudioClipItem()
        {
            FTK_P();
            _cancelRequests();
        }

        std::shared_ptr<AudioClipItem> AudioClipItem::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<Timeline>& timeline,
            const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip>& clip,
            double scale,
            const ItemOptions& options,
            const DisplayOptions& displayOptions,
            const std::shared_ptr<ItemData>& itemData,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<AudioClipItem>(new AudioClipItem);
            out->_init(
                context,
                timeline,
                clip,
                scale,
                options,
                displayOptions,
                itemData,
                parent);
            return out;
        }

        void AudioClipItem::setScale(double value)
        {
            const bool changed = value != _scale;
            IBasicItem::setScale(value);
            FTK_P();
            if (changed)
            {
                p.waveforms.clear();
                _cancelRequests();
                setDrawUpdate();
            }
        }

        void AudioClipItem::setDisplayOptions(const DisplayOptions& value)
        {
            const bool thumbnailsChanged =
                value.waveforms != _displayOptions.waveforms ||
                value.waveformWidth != _displayOptions.waveformWidth ||
                value.waveformHeight != _displayOptions.waveformHeight ||
                value.waveformPrim != _displayOptions.waveformPrim;
            IBasicItem::setDisplayOptions(value);
            FTK_P();
            if (thumbnailsChanged)
            {
                p.waveforms.clear();
                _cancelRequests();
                setDrawUpdate();
            }
        }
        
        ftk::Size2I AudioClipItem::getSizeHint() const
        {
            ftk::Size2I out = IBasicItem::getSizeHint();
            if (_displayOptions.waveforms)
            {
                out.h += _displayOptions.waveformHeight;
            }
            return out;
        }

        void AudioClipItem::setGeometry(const ftk::Box2I& value)
        {
            const ftk::Size2I size = getGeometry().size();
            IBasicItem::setGeometry(value);
            FTK_P();
            if (size != value.size())
            {
                p.waveforms.clear();
                _cancelRequests();
            }
        }

        void AudioClipItem::tickEvent(
            bool parentsVisible,
            bool parentsEnabled,
            const ftk::TickEvent& event)
        {
            IBasicItem::tickEvent(parentsVisible, parentsEnabled, event);
            FTK_P();

            // Check if the I/O information is finished.
            if (p.infoRequest.future.valid() &&
                p.infoRequest.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                p.ioInfo = p.infoRequest.future.get();
                setSizeUpdate();
                setDrawUpdate();
            }

            // Check if any audio waveforms are finished.
            auto i = p.waveformRequests.begin();
            while (i != p.waveformRequests.end())
            {
                if (i->second.future.valid() &&
                    i->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                {
                    auto mesh = i->second.future.get();
                    p.waveforms[i->second.timeRange.start_time()] = mesh;
                    i = p.waveformRequests.erase(i);
                    setDrawUpdate();
                }
                else
                {
                    ++i;
                }
            }
        }

        void AudioClipItem::sizeHintEvent(const ftk::SizeHintEvent& event)
        {
            IBasicItem::sizeHintEvent(event);
            FTK_P();
            p.size.dragLength = event.style->getSizeRole(ftk::SizeRole::DragLength, event.displayScale);
        }

        void AudioClipItem::clipEvent(const ftk::Box2I& clipRect, bool clipped)
        {
            IBasicItem::clipEvent(clipRect, clipped);
            FTK_P();
            if (clipRect == p.size.clipRect)
                return;
            p.size.clipRect = clipRect;
            if (clipped)
            {
                p.waveforms.clear();
                _cancelRequests();
                setDrawUpdate();
            }
        }

        void AudioClipItem::drawEvent(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            IBasicItem::drawEvent(drawRect, event);
            if (_displayOptions.waveforms)
            {
                _drawWaveforms(drawRect, event);
            }
        }

        void AudioClipItem::_drawWaveforms(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();

            const ftk::Box2I g = _getInsideGeometry();
            const int m = _getMargin();
            const int lineHeight = _getLineHeight();

            const ftk::Box2I g2(
                g.min.x,
                g.min.y +
                (!_displayOptions.minimize ? (lineHeight + m * 2) : 0),
                g.w(),
                _displayOptions.waveformHeight);
            event.render->drawRect(
                g2,
                ftk::Color4F(0.F, 0.F, 0.F));
            const ftk::ClipRectEnabledState clipRectEnabledState(event.render);
            const ftk::ClipRectState clipRectState(event.render);
            event.render->setClipRectEnabled(true);
            event.render->setClipRect(ftk::intersect(g2, clipRectState.getClipRect()));

            const ftk::Box2I clipRect = _getClipRect(
                drawRect,
                _displayOptions.clipRectScale);
            if (!p.ioInfo.has_value() &&
                !p.infoRequest.future.valid() &&
                ftk::intersects(g, clipRect))
            {
                p.infoRequest = p.thumbnailSystem->getInfo(
                    p.path,
                    p.memRead,
                    _data->options.ioOptions);
            }

            std::map< OTIO_NS::RationalTime, std::shared_ptr<ftk::TriMesh2F> > waveforms;
            if (_displayOptions.waveformWidth > 0 && p.ioInfo.has_value())
            {
                const int w = g.w();
                for (int x = 0; x < w; x += _displayOptions.waveformWidth)
                {
                    const ftk::Box2I box(
                        g.min.x +
                        x,
                        g.min.y +
                        (!_displayOptions.minimize ? (lineHeight + m * 2) : 0),
                        _displayOptions.waveformWidth,
                        _displayOptions.waveformHeight);
                    if (ftk::intersects(box, clipRect))
                    {
                        const OTIO_NS::RationalTime time = OTIO_NS::RationalTime(
                            _timeRange.start_time().value() +
                            (w > 0 ? (x / static_cast<double>(w)) : 0) *
                            _timeRange.duration().rescaled_to(_timeRange.start_time()).value(),
                            _timeRange.start_time().rate()).
                            round();
                        const OTIO_NS::RationalTime time2 = OTIO_NS::RationalTime(
                            _timeRange.start_time().value() +
                            (w > 0 ? ((x + _displayOptions.waveformWidth) / static_cast<double>(w)) : 0) *
                            _timeRange.duration().rescaled_to(_timeRange.start_time()).value(),
                            _timeRange.start_time().rate()).
                            round();
                        OTIO_NS::TimeRange trimmedRange = _trimmedRange;
                        if (_data->options.compat &&
                            trimmedRange.start_time() < p.ioInfo->audioTime.start_time())
                        {
                            //! \bug If the trimmed range is less than the media time,
                            //! assume the media time is wrong (e.g., ALab trailer) and
                            //! compensate for it.
                            trimmedRange = OTIO_NS::TimeRange(
                                p.ioInfo->audioTime.start_time() + trimmedRange.start_time(),
                                trimmedRange.duration());
                        }
                        const OTIO_NS::TimeRange mediaRange = toAudioMediaTime(
                            OTIO_NS::TimeRange::range_from_start_end_time(time, time2),
                            _timeRange,
                            trimmedRange,
                            p.ioInfo->audio.sampleRate);
                        std::shared_ptr<ftk::TriMesh2F> mesh;
                        const auto i = p.waveforms.find(mediaRange.start_time());
                        if (i != p.waveforms.end())
                        {
                            mesh = i->second;
                        }
                        else if (p.ioInfo->audio.isValid())
                        {
                            const auto j = p.waveformRequests.find(mediaRange.start_time());
                            if (j == p.waveformRequests.end())
                            {
                                auto request = p.thumbnailSystem->getWaveform(
                                    p.path,
                                    p.memRead,
                                    box.size(),
                                    mediaRange,
                                    _data->options.ioOptions);
                                if (request.future.valid() &&
                                    request.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                                {
                                    mesh = request.future.get();
                                }
                                else
                                {
                                    p.waveformRequests[mediaRange.start_time()] = std::move(request);
                                }
                            }
                        }
                        if (mesh)
                        {
                            event.render->drawMesh(
                                *mesh,
                                ftk::Color4F(1.F, 1.F, 1.F),
                                ftk::V2F(box.min.x, box.min.y));
                            waveforms[mediaRange.start_time()] = mesh;
                        }
                    }
                }
            }
            p.waveforms = waveforms;
        }

        void AudioClipItem::_cancelRequests()
        {
            FTK_P();
            std::vector<uint64_t> ids;
            if (p.infoRequest.future.valid())
            {
                ids.push_back(p.infoRequest.id);
                p.infoRequest = InfoRequest();
            }
            for (const auto& i : p.waveformRequests)
            {
                ids.push_back(i.second.id);
            }
            p.waveformRequests.clear();
            p.thumbnailSystem->cancelRequests(ids);
        }
    }
}

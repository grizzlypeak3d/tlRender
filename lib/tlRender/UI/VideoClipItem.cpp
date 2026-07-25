// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/VideoClipItem.h>

#include <tlRender/UI/ThumbnailSystem.h>

#include <tlRender/Timeline/IRender.h>
#include <tlRender/Timeline/Util.h>

#include <ftk/UI/DrawUtil.h>
#include <ftk/Core/RenderUtil.h>

namespace tl
{
    namespace ui
    {
        struct VideoClipItem::Private
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

            IOOptions ioOptions;
            InfoRequest infoRequest;
            std::optional<IOInfo> ioInfo;
            std::map<OTIO_NS::RationalTime, ThumbnailRequest> thumbnailRequests;
            std::map<OTIO_NS::RationalTime, std::shared_ptr<ftk::Image> > thumbnails;
        };

        void VideoClipItem::_init(
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
                getItemColor(clip.value, ftk::Color4F(.2F, .4F, .4F), displayOptions),
                "tl::ui::VideoClipItem",
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

            p.ioOptions = _data->options.ioOptions;
            p.ioOptions["USD/CameraName"] = clip->name();
        }

        VideoClipItem::VideoClipItem() :
            _p(new Private)
        {}

        VideoClipItem::~VideoClipItem()
        {
            FTK_P();
            _cancelRequests();
        }

        std::shared_ptr<VideoClipItem> VideoClipItem::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<Timeline>& timeline,
            const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip>& clip,
            double scale,
            const ItemOptions& options,
            const DisplayOptions& displayOptions,
            const std::shared_ptr<ItemData>& itemData,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<VideoClipItem>(new VideoClipItem);
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

        void VideoClipItem::setScale(double value)
        {
            const bool changed = value != _scale;
            IBasicItem::setScale(value);
            FTK_P();
            if (changed)
            {
                p.thumbnails.clear();
                _cancelRequests();
                setDrawUpdate();
            }
        }

        void VideoClipItem::setDisplayOptions(const DisplayOptions& value)
        {
            const bool thumbnailsChanged =
                value.thumbnails != _displayOptions.thumbnails ||
                value.thumbnailHeight != _displayOptions.thumbnailHeight;
            IBasicItem::setDisplayOptions(value);
            FTK_P();
            if (thumbnailsChanged)
            {
                p.thumbnails.clear();
                _cancelRequests();
                setDrawUpdate();
            }
        }
        
        ftk::Size2I VideoClipItem::getSizeHint() const
        {
            ftk::Size2I out = IBasicItem::getSizeHint();
            if (_displayOptions.thumbnails)
            {
                out.h += _displayOptions.thumbnailHeight;
            }
            return out;
        }

        void VideoClipItem::setGeometry(const ftk::Box2I& value)
        {
            const ftk::Size2I size = getGeometry().size();
            IBasicItem::setGeometry(value);
            FTK_P();
            if (size != value.size())
            {
                p.thumbnails.clear();
                _cancelRequests();
            }
        }

        void VideoClipItem::tickEvent(
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

            // Check if any thumbnails are finished.
            auto i = p.thumbnailRequests.begin();
            while (i != p.thumbnailRequests.end())
            {
                if (i->second.future.valid() &&
                    i->second.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                {
                    const auto image = i->second.future.get();
                    p.thumbnails[i->second.time] = image;
                    i = p.thumbnailRequests.erase(i);
                    setDrawUpdate();
                }
                else
                {
                    ++i;
                }
            }
        }

        void VideoClipItem::sizeHintEvent(const ftk::SizeHintEvent& event)
        {
            IBasicItem::sizeHintEvent(event);
            FTK_P();
            p.size.dragLength = event.style->getSizeRole(ftk::SizeRole::DragLength, event.displayScale);
        }

        void VideoClipItem::clipEvent(const ftk::Box2I& clipRect, bool clipped)
        {
            IBasicItem::clipEvent(clipRect, clipped);
            FTK_P();
            if (clipRect == p.size.clipRect)
                return;
            p.size.clipRect = clipRect;
            if (clipped)
            {
                p.thumbnails.clear();
                _cancelRequests();
                setDrawUpdate();
            }
        }

        void VideoClipItem::drawEvent(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            IBasicItem::drawEvent(drawRect, event);
            if (_displayOptions.thumbnails)
            {
                _drawThumbnails(drawRect, event);
            }
        }

        void VideoClipItem::_drawThumbnails(
            const ftk::Box2I& drawRect,
            const ftk::DrawEvent& event)
        {
            FTK_P();

            auto render = std::dynamic_pointer_cast<IRender>(event.render);
            const ftk::Box2I g = _getInsideGeometry();
            const int m = _getMargin();
            const int lineHeight = _getLineHeight();

            const ftk::Box2I g2(
                g.min.x,
                g.min.y +
                (!_displayOptions.minimize ? (lineHeight + m * 2) : 0),
                g.w(),
                _displayOptions.thumbnailHeight);
            render->drawRect(
                g2,
                ftk::Color4F(0.F, 0.F, 0.F));
            const ftk::ClipRectEnabledState clipRectEnabledState(render);
            const ftk::ClipRectState clipRectState(render);
            render->setClipRectEnabled(true);
            render->setClipRect(ftk::intersect(g2, clipRectState.getClipRect()));
            render->setOCIOOptions(_displayOptions.ocio);
            render->setLUTOptions(_displayOptions.lut);

            const ftk::Box2I clipRect = _getClipRect(
                drawRect,
                _displayOptions.clipRectScale);

            // Get thumbnail information.
            if (!p.ioInfo.has_value() &&
                !p.infoRequest.future.valid() &&
                ftk::intersects(g, clipRect))
            {
                p.infoRequest = p.thumbnailSystem->getInfo(
                    p.path,
                    p.memRead,
                    p.ioOptions);
            }

            std::map<OTIO_NS::RationalTime, std::shared_ptr<ftk::Image> > thumbnails;
            const int thumbnailWidth =
                (_displayOptions.thumbnails && p.ioInfo.has_value() && !p.ioInfo->video.empty()) ?
                static_cast<int>(_displayOptions.thumbnailHeight * ftk::aspectRatio(p.ioInfo->video[0].size)) :
                0;
            if (thumbnailWidth > 0)
            {
                const int w = g.w();
                for (int x = 0; x < w; x += thumbnailWidth)
                {
                    const ftk::Box2I box(
                        g.min.x +
                        x,
                        g.min.y +
                        (!_displayOptions.minimize ? (lineHeight + m * 2) : 0),
                        thumbnailWidth,
                        _displayOptions.thumbnailHeight);
                    if (ftk::intersects(box, clipRect))
                    {
                        const OTIO_NS::RationalTime time = OTIO_NS::RationalTime(
                            _timeRange.start_time().value() +
                            (w > 1 ? (x / static_cast<double>(w - 1)) : 0) *
                            _timeRange.duration().rescaled_to(_timeRange.start_time()).value(),
                            _timeRange.start_time().rate()).
                            floor();
                        OTIO_NS::TimeRange trimmedRange = _trimmedRange;
                        if (_data->options.compat &&
                            _availableRange.start_time() > p.ioInfo->videoTime.start_time())
                        {
                            //! \bug If the available range is greater than the media time,
                            //! assume the media time is wrong (e.g., Picchu) and
                            //! compensate for it.
                            trimmedRange = OTIO_NS::TimeRange(
                                trimmedRange.start_time() - _availableRange.start_time(),
                                trimmedRange.duration());
                        }
                        const OTIO_NS::RationalTime mediaTime = toVideoMediaTime(
                            time,
                            _timeRange,
                            trimmedRange,
                            p.ioInfo->videoTime.duration().rate());
                        std::shared_ptr<ftk::Image> image;
                        const auto i = p.thumbnails.find(mediaTime);
                        if (i != p.thumbnails.end())
                        {
                            image = i->second;
                        }
                        else if (!p.ioInfo->video.empty())
                        {
                            const auto k = p.thumbnailRequests.find(mediaTime);
                            if (k == p.thumbnailRequests.end())
                            {
                                auto request = p.thumbnailSystem->getThumbnail(
                                    p.path,
                                    p.memRead,
                                    _displayOptions.thumbnailHeight,
                                    mediaTime,
                                    p.ioOptions);
                                if (request.future.valid() &&
                                    request.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                                {
                                    image = request.future.get();
                                }
                                else
                                {
                                    p.thumbnailRequests[mediaTime] = std::move(request);
                                }
                            }
                        }
                        if (image)
                        {
                            ftk::ImageOptions imageOptions;
                            imageOptions.cache = false;
                            render->drawImage(
                                image,
                                box,
                                ftk::Color4F(1.F, 1.F, 1.F),
                                imageOptions);
                            thumbnails[mediaTime] = image;
                        }
                    }
                }
            }
            p.thumbnails = thumbnails;
        }

        void VideoClipItem::_cancelRequests()
        {
            FTK_P();
            std::vector<uint64_t> ids;
            if (p.infoRequest.future.valid())
            {
                ids.push_back(p.infoRequest.id);
                p.infoRequest = InfoRequest();
            }
            for (const auto& i : p.thumbnailRequests)
            {
                ids.push_back(i.second.id);
            }
            p.thumbnailRequests.clear();
            p.thumbnailSystem->cancelRequests(ids);
        }
    }
}

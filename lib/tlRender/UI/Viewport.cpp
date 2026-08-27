// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/Viewport.h>

#include <tlRender/Timeline/IRender.h>

#include <ftk/UI/DrawUtil.h>
#include <ftk/GL/GL.h>
#include <ftk/GL/OffscreenBuffer.h>
#include <ftk/GL/Util.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/RenderUtil.h>

#include <cmath>

namespace tl
{
    namespace ui
    {
        namespace
        {
            // A position inside a box, in the coordinates of what the box
            // holds. Clamped because a box and its contents are different
            // sizes whenever the image is scaled, and the far edge rounds up
            // to a pixel past the last one.
            ftk::V2I mapInto(
                const ftk::V2I& pos,
                const ftk::Box2I& box,
                const ftk::Size2I& size)
            {
                return ftk::V2I(
                    std::clamp<int>(
                        std::lround(
                            (pos.x - box.min.x) / static_cast<double>(box.w()) * size.w),
                        0,
                        size.w - 1),
                    std::clamp<int>(
                        std::lround(
                            (pos.y - box.min.y) / static_cast<double>(box.h()) * size.h),
                        0,
                        size.h - 1));
            }
        }

        struct Viewport::Private
        {
            std::shared_ptr<ftk::Observable<CompareOptions> > compareOptions;
            std::shared_ptr<ftk::Observable<OCIOOptions> > ocioOptions;
            std::function<std::string(
                const std::string&,
                const ftk::ImageTags&)> ocioInputResolver;
            std::shared_ptr<ftk::Observable<LUTOptions> > lutOptions;
            std::shared_ptr<ftk::ObservableList<ftk::ImageOptions> > imageOptions;
            std::shared_ptr<ftk::ObservableList<DisplayOptions> > displayOptions;
            std::shared_ptr<ftk::Observable<BackgroundOptions> > bgOptions;
            std::shared_ptr<ftk::Observable<ForegroundOptions> > fgOptions;
            std::shared_ptr<ftk::Observable<ftk::gl::TextureType> > colorBuffer;
            std::shared_ptr<Player> player;
            std::vector<VideoFrame> videoFrame;
            std::shared_ptr<ftk::Observable<ftk::V2I> > viewPos;
            std::shared_ptr<ftk::Observable<double> > zoom;
            ftk::RangeD zoomRange = ftk::RangeD(0.01, 512.0);
            std::shared_ptr<ftk::Observable<std::pair<ftk::V2I, double> > > viewPosZoom;
            std::shared_ptr<ftk::Observable<bool> > frameView;
            std::shared_ptr<ftk::Observable<bool> > framed;
            std::shared_ptr<ftk::Observable<double> > fps;
            struct FpsData
            {
                std::chrono::steady_clock::time_point timer;
                size_t frameCount = 0;
            };
            std::optional<FpsData> fpsData;
            std::shared_ptr<ftk::Observable<size_t> > droppedFrames;
            bool inputEnabled = true;
            std::pair<ftk::MouseButton, ftk::KeyModifier> panBinding =
                std::make_pair(ftk::MouseButton::Middle, ftk::KeyModifier::None);
            std::pair<ftk::MouseButton, ftk::KeyModifier> wipeBinding =
                std::make_pair(ftk::MouseButton::Left, ftk::KeyModifier::Alt);
            std::pair<ftk::MouseButton, ftk::KeyModifier> pickBinding =
                std::make_pair(ftk::MouseButton::None, ftk::KeyModifier::None);
            float mouseWheelScale = 1.1F;

            bool picked = false;
            enum class Resample { None, Wait, Read };
            Resample resample = Resample::None;
            bool resampleOnFrames = false;
            std::shared_ptr<ftk::Observable<ftk::V2I> > samplePos;
            std::shared_ptr<ftk::Observable<std::optional<ftk::V2I> > > pick;
            std::shared_ptr<ftk::Observable<std::optional<ftk::Color4F> > > colorSample;

            bool doRender = false;
            std::shared_ptr<ftk::gl::OffscreenBuffer> buffer;
            std::shared_ptr<ftk::gl::OffscreenBuffer> bgBuffer;
            std::shared_ptr<ftk::gl::OffscreenBuffer> fgBuffer;

            struct SizeData
            {
                float displayScale = 1.F;
                int sizeHint = 0;
            };
            SizeData size;

            enum class MouseMode
            {
                None,
                View,
                Wipe,
                Picker
            };
            struct MouseData
            {
                bool inside = false;
                ftk::V2I pos;
                ftk::V2I press;
                MouseMode mode = MouseMode::None;
                ftk::V2I viewPos;
            };
            MouseData mouse;

            std::shared_ptr<ftk::Observer<Playback> > playbackObserver;
            std::shared_ptr<ftk::ListObserver<VideoFrame> > videoFrameObserver;
            std::shared_ptr<ftk::Observer<size_t> > droppedFramesObserver;
        };

        void Viewport::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init(context, "tl::ui::Viewport", parent);
            FTK_P();

            setHStretch(ftk::Stretch::Expanding);
            setVStretch(ftk::Stretch::Expanding);

            p.compareOptions = ftk::Observable<CompareOptions>::create();
            p.ocioOptions = ftk::Observable<OCIOOptions>::create();
            p.lutOptions = ftk::Observable<LUTOptions>::create();
            p.imageOptions = ftk::ObservableList<ftk::ImageOptions>::create();
            p.displayOptions = ftk::ObservableList<DisplayOptions>::create();
            p.bgOptions = ftk::Observable<BackgroundOptions>::create();
            p.fgOptions = ftk::Observable<ForegroundOptions>::create();
            p.compareOptions = ftk::Observable<CompareOptions>::create();
            p.colorBuffer = ftk::Observable<ftk::gl::TextureType>::create(
                ftk::gl::offscreenColorDefault);
            p.viewPos = ftk::Observable<ftk::V2I>::create();
            p.zoom = ftk::Observable<double>::create(1.0);
            p.viewPosZoom = ftk::Observable<std::pair<ftk::V2I, double> >::create(
                std::make_pair(ftk::V2I(), 1.0));
            p.frameView = ftk::Observable<bool>::create(true);
            p.framed = ftk::Observable<bool>::create(false);
            p.fps = ftk::Observable<double>::create(0.0);
            p.droppedFrames = ftk::Observable<size_t>::create(0);
            p.samplePos = ftk::Observable<ftk::V2I>::create();
            p.pick = ftk::Observable<std::optional<ftk::V2I> >::create();
            p.colorSample = ftk::Observable<std::optional<ftk::Color4F> >::create();
        }

        Viewport::Viewport() :
            _p(new Private)
        {}

        Viewport::~Viewport()
        {}

        std::shared_ptr<Viewport> Viewport::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<Viewport>(new Viewport);
            out->_init(context, parent);
            return out;
        }

        const CompareOptions& Viewport::getCompareOptions() const
        {
            return _p->compareOptions->get();
        }

        std::shared_ptr<ftk::IObservable<CompareOptions> > Viewport::observeCompareOptions() const
        {
            return _p->compareOptions;
        }

        void Viewport::setCompareOptions(const CompareOptions& value)
        {
            FTK_P();
            // A comparison change can bring a different image under the
            // sample position, or none at all, so the sample is retaken
            // once the comparison's frames have arrived and been drawn.
            if (value.compare != p.compareOptions->get().compare && p.picked)
            {
                p.resample = Private::Resample::Wait;
                p.resampleOnFrames = true;
            }
            if (p.compareOptions->setIfChanged(value))
            {
                p.doRender = true;
                setDrawUpdate();
            }
        }

        const OCIOOptions& Viewport::getOCIOOptions() const
        {
            return _p->ocioOptions->get();
        }

        std::shared_ptr<ftk::IObservable<OCIOOptions> > Viewport::observeOCIOOptions() const
        {
            return _p->ocioOptions;
        }

        void Viewport::setOCIOOptions(const OCIOOptions& value)
        {
            FTK_P();
            if (p.ocioOptions->setIfChanged(value))
            {
                p.doRender = true;
                setDrawUpdate();
            }
        }

        void Viewport::setOCIOInputResolver(
            const std::function<std::string(
                const std::string& path,
                const ftk::ImageTags&)>& value)
        {
            FTK_P();
            p.ocioInputResolver = value;
            setDrawUpdate();
        }

        const LUTOptions& Viewport::getLUTOptions() const
        {
            return _p->lutOptions->get();
        }

        std::shared_ptr<ftk::IObservable<LUTOptions> > Viewport::observeLUTOptions() const
        {
            return _p->lutOptions;
        }

        void Viewport::setLUTOptions(const LUTOptions& value)
        {
            FTK_P();
            if (p.lutOptions->setIfChanged(value))
            {
                p.doRender = true;
                setDrawUpdate();
            }
        }

        const std::vector<ftk::ImageOptions>& Viewport::getImageOptions() const
        {
            return _p->imageOptions->get();
        }

        std::shared_ptr<ftk::IObservableList<ftk::ImageOptions> > Viewport::observeImageOptions() const
        {
            return _p->imageOptions;
        }

        void Viewport::setImageOptions(const std::vector<ftk::ImageOptions>& value)
        {
            FTK_P();
            if (p.imageOptions->setIfChanged(value))
            {
                p.doRender = true;
                setDrawUpdate();
            }
        }

        const std::vector<DisplayOptions>& Viewport::getDisplayOptions() const
        {
            return _p->displayOptions->get();
        }

        std::shared_ptr<ftk::IObservableList<DisplayOptions> > Viewport::observeDisplayOptions() const
        {
            return _p->displayOptions;
        }

        void Viewport::setDisplayOptions(const std::vector<DisplayOptions>& value)
        {
            FTK_P();
            if (p.displayOptions->setIfChanged(value))
            {
                p.doRender = true;
                setDrawUpdate();
            }
        }

        const BackgroundOptions& Viewport::getBackgroundOptions() const
        {
            return _p->bgOptions->get();
        }

        std::shared_ptr<ftk::IObservable<BackgroundOptions> > Viewport::observeBackgroundOptions() const
        {
            return _p->bgOptions;
        }

        void Viewport::setBackgroundOptions(const BackgroundOptions& value)
        {
            FTK_P();
            if (p.bgOptions->setIfChanged(value))
            {
                p.doRender = true;
                setDrawUpdate();
            }
        }

        const ForegroundOptions& Viewport::getForegroundOptions() const
        {
            return _p->fgOptions->get();
        }

        std::shared_ptr<ftk::IObservable<ForegroundOptions> > Viewport::observeForegroundOptions() const
        {
            return _p->fgOptions;
        }

        void Viewport::setForegroundOptions(const ForegroundOptions& value)
        {
            FTK_P();
            if (p.fgOptions->setIfChanged(value))
            {
                p.doRender = true;
                setDrawUpdate();
            }
        }

        ftk::gl::TextureType Viewport::getColorBuffer() const
        {
            return _p->colorBuffer->get();
        }

        std::shared_ptr<ftk::IObservable<ftk::gl::TextureType> > Viewport::observeColorBuffer() const
        {
            return _p->colorBuffer;
        }

        void Viewport::setColorBuffer(ftk::gl::TextureType value)
        {
            FTK_P();
            if (p.colorBuffer->setIfChanged(value))
            {
                p.doRender = true;
                setDrawUpdate();
            }
        }

        const std::shared_ptr<Player>& Viewport::getPlayer() const
        {
            return _p->player;
        }

        void Viewport::setPlayer(const std::shared_ptr<Player>& value)
        {
            FTK_P();

            p.fps->setIfChanged(0.0);
            p.fpsData.reset();
            p.droppedFrames->setIfChanged(0);
            p.playbackObserver.reset();
            p.videoFrameObserver.reset();
            p.droppedFramesObserver.reset();

            p.player = value;

            if (p.player)
            {
                p.playbackObserver = ftk::Observer<Playback>::create(
                    p.player->observePlayback(),
                    [this](Playback value)
                    {
                        FTK_P();
                        switch (value)
                        {
                        case Playback::Forward:
                        case Playback::Reverse:
                            p.fpsData = Private::FpsData();
                            p.fpsData->timer = std::chrono::steady_clock::now();
                            p.fpsData->frameCount = 0;
                            break;
                        default:
                            p.fps->setIfChanged(0.0);
                            p.fpsData.reset();
                            break;
                        }
                    });

                p.videoFrameObserver = ftk::ListObserver<VideoFrame>::create(
                    p.player->observeCurrentVideo(),
                    [this](const std::vector<VideoFrame>& value)
                    {
                        FTK_P();
                        p.videoFrame = value;

                        if (p.resampleOnFrames)
                        {
                            // The frames a comparison had to read to be
                            // shown, which arrive after the comparison
                            // itself changed.
                            p.resampleOnFrames = false;
                            p.resample = Private::Resample::Wait;
                        }

                        if (p.fpsData.has_value())
                        {
                            p.fpsData->frameCount = p.fpsData->frameCount + 1;
                            const auto now = std::chrono::steady_clock::now();
                            const std::chrono::duration<double> diff = now - p.fpsData->timer;
                            if (diff.count() > 1.0)
                            {
                                const double fps = p.fpsData->frameCount / diff.count();
                                p.fps->setIfChanged(fps);
                                p.fpsData->timer = now;
                                p.fpsData->frameCount = 0;
                            }
                        }

                        p.doRender = true;
                        setDrawUpdate();
                    });

                p.droppedFramesObserver = ftk::Observer<size_t>::create(
                    p.player->observeDroppedFrames(),
                    [this](size_t value)
                    {
                        _p->droppedFrames->setIfChanged(value);
                    });
            }
            else
            {
                // The position and zoom say where an image sat in the view,
                // so with the media gone they describe nothing, and a large
                // zoom left behind makes the next file opened look broken.
                // Set the observables rather than calling
                // setViewPosAndZoom(), which would take frame view off as a
                // side effect of closing a file and leave the next one
                // unframed.
                bool changed = false;
                const std::pair<ftk::V2I, double> reset(ftk::V2I(), 1.0);
                if (p.viewPosZoom->setIfChanged(reset))
                {
                    p.viewPos->setIfChanged(reset.first);
                    p.zoom->setIfChanged(reset.second);
                    changed = true;
                }
                if (!p.videoFrame.empty())
                {
                    p.videoFrame.clear();
                    changed = true;
                }
                if (changed)
                {
                    p.doRender = true;
                    setDrawUpdate();
                }
            }
        }

        ftk::V2I Viewport::toViewportPos(const ftk::V2I& windowPos) const
        {
            return windowPos - getGeometry().min;
        }

        ftk::V2I Viewport::toRenderPos(const ftk::V2I& viewportPos) const
        {
            FTK_P();
            const ftk::V2I& viewPos = p.viewPos->get();
            const double zoom = p.zoom->get();
            return ftk::V2I(
                static_cast<int>(std::floor((viewportPos.x - viewPos.x) / zoom)),
                static_cast<int>(std::floor((viewportPos.y - viewPos.y) / zoom)));
        }

        ftk::V2I Viewport::fromRenderPos(const ftk::V2I& renderPos) const
        {
            FTK_P();
            const ftk::V2I& viewPos = p.viewPos->get();
            const double zoom = p.zoom->get();
            return ftk::V2I(
                static_cast<int>(std::floor(viewPos.x + renderPos.x * zoom)),
                static_cast<int>(std::floor(viewPos.y + renderPos.y * zoom)));
        }

        const ftk::V2I& Viewport::getViewPos() const
        {
            return _p->viewPos->get();
        }

        std::shared_ptr<ftk::IObservable<ftk::V2I> > Viewport::observeViewPos() const
        {
            return _p->viewPos;
        }

        double Viewport::getZoom() const
        {
            return _p->zoom->get();
        }

        std::shared_ptr<ftk::IObservable<double> > Viewport::observeZoom() const
        {
            return _p->zoom;
        }

        std::pair<ftk::V2I, double> Viewport::getViewPosAndZoom() const
        {
            return _p->viewPosZoom->get();
        }

        std::shared_ptr<ftk::IObservable<std::pair<ftk::V2I, double> > > Viewport::observeViewPosAndZoom() const
        {
            return _p->viewPosZoom;
        }

        void Viewport::setViewPosAndZoom(const ftk::V2I& pos, double zoom)
        {
            FTK_P();
            const double zoomClamped = ftk::clamp(zoom, p.zoomRange.min(), p.zoomRange.max());
            const std::pair<ftk::V2I, double> pair(pos, zoomClamped);
            if (pair != p.viewPosZoom->get())
            {
                setFrameView(false);
            }
            if (p.viewPosZoom->setIfChanged(pair))
            {
                p.viewPos->setIfChanged(pos);
                p.zoom->setIfChanged(zoomClamped);
                p.doRender = true;
                setDrawUpdate();
            }
        }

        void Viewport::setZoom(double zoom, const ftk::V2I& focus)
        {
            FTK_P();
            ftk::V2I pos;
            const ftk::V2I& viewPos = p.viewPos->get();
            const double zoomPrev = p.zoom->get();
            const double zoomClamped = ftk::clamp(zoom, p.zoomRange.min(), p.zoomRange.max());
            pos.x = focus.x + (viewPos.x - focus.x) * (zoomClamped / zoomPrev);
            pos.y = focus.y + (viewPos.y - focus.y) * (zoomClamped / zoomPrev);
            setViewPosAndZoom(pos, zoomClamped);
        }

        bool Viewport::hasFrameView() const
        {
            return _p->frameView->get();
        }

        std::shared_ptr<ftk::IObservable<bool> > Viewport::observeFrameView() const
        {
            return _p->frameView;
        }

        std::shared_ptr<ftk::IObservable<bool> > Viewport::observeFramed() const
        {
            return _p->framed;
        }

        void Viewport::setFrameView(bool value)
        {
            FTK_P();
            if (p.frameView->setIfChanged(value))
            {
                if (value)
                {
                    p.framed->setAlways(true);
                }
                p.doRender = true;
                setDrawUpdate();
            }
        }

        void Viewport::center()
        {
            FTK_P();
            const ftk::Size2I renderSize = _getRenderSize();
            if (renderSize.isValid())
            {
                const ftk::Box2I& g = getGeometry();
                const ftk::Size2I viewportSize = g.size();
                const float zoom = p.zoom->get();
                const ftk::V2I pos = ftk::V2I(
                    viewportSize.w / 2.F - renderSize.w / 2 * zoom,
                    viewportSize.h / 2.F - renderSize.h / 2 * zoom);
                setViewPosAndZoom(pos, p.zoom->get());
            }
        }

        void Viewport::resetZoom()
        {
            FTK_P();
            setZoom(1.F, _getViewportCenter());
        }

        void Viewport::zoomIn()
        {
            FTK_P();
            setZoom(
                p.zoom->get() * 2.0,
                p.mouse.inside ? p.mouse.pos : _getViewportCenter());
        }

        void Viewport::zoomOut()
        {
            FTK_P();
            setZoom(p.zoom->get() / 2.0, _getViewportCenter());
        }

        const ftk::RangeD& Viewport::getZoomRange() const
        {
            return _p->zoomRange;
        }

        void Viewport::setZoomRange(const ftk::RangeD& value)
        {
            FTK_P();
            p.zoomRange = value;
            setZoom(p.zoom->get());
        }

        double Viewport::getFPS() const
        {
            return _p->fps->get();
        }

        std::shared_ptr<ftk::IObservable<double> > Viewport::observeFPS() const
        {
            return _p->fps;
        }

        size_t Viewport::getDroppedFrames() const
        {
            return _p->droppedFrames->get();
        }

        std::shared_ptr<ftk::IObservable<size_t> > Viewport::observeDroppedFrames() const
        {
            return _p->droppedFrames;
        }

        ftk::Color4F Viewport::getColorSample(const ftk::V2I& value)
        {
            FTK_P();
            ftk::Color4F out;
            if (p.buffer)
            {
                const ftk::Box2I& g = getGeometry();
                std::vector<float> sample(4);
                ftk::gl::OffscreenBufferBinding binding(p.buffer);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
#if defined(FTK_API_GL_4_1)
                glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
#endif // FTK_API_GL_4_1
                glReadPixels(
                    value.x,
                    g.h() - 1 - value.y,
                    1,
                    1,
                    GL_RGBA,
                    GL_FLOAT,
                    sample.data());
                out.r = std::isnan(sample[0]) || std::isinf(sample[0]) ? 0.F : sample[0];
                out.g = std::isnan(sample[1]) || std::isinf(sample[1]) ? 0.F : sample[1];
                out.b = std::isnan(sample[2]) || std::isinf(sample[2]) ? 0.F : sample[2];
                out.a = std::isnan(sample[3]) || std::isinf(sample[3]) ? 0.F : sample[3];
            }
            return out;
        }

        std::shared_ptr<ftk::IObservable<ftk::V2I> > Viewport::observeSamplePos() const
        {
            return _p->samplePos;
        }

        std::shared_ptr<ftk::IObservable<std::optional<ftk::V2I> > > Viewport::observePick() const
        {
            return _p->pick;
        }

        std::shared_ptr<ftk::IObservable<std::optional<ftk::Color4F> > > Viewport::observeColorSample() const
        {
            return _p->colorSample;
        }

        void Viewport::pick(const ftk::V2I& imagePos)
        {
            FTK_P();
            // Image pixel -> widget-local position, inverting the pick math
            // used by the mouse handlers (image = (widget - viewPos) / zoom).
            // Then sample exactly as a pick mouse action would. The incoming
            // position is a source pixel, so it goes through the canvas
            // first.
            const ftk::V2I pos = fromRenderPos(_fromSourcePixel(imagePos));
            p.samplePos->setIfChanged(pos);
            p.picked = true;
            _sampleUpdate();
            // The pixel asked for rather than the one that comes back from
            // converting it to a position and then converting it again,
            // which is a pixel out at some zooms.
            p.pick->setIfChanged(imagePos);
        }

        bool Viewport::_getSourceBox(ftk::Box2I& box, ftk::Size2I& size) const
        {
            FTK_P();
            // With OTIO spatial coordinates the render space is the timeline
            // canvas rather than the media, so the box the image is drawn
            // into is needed to relate the two. Returns false without
            // spatial coordinates, where render space is already the image.
            if (!p.videoFrame.empty() && p.videoFrame.front().canvasSize.isValid())
            {
                const auto& displayOptions = p.displayOptions->get();
                const AspectRatioOptions aspectRatio =
                    !displayOptions.empty() ?
                    displayOptions.front().aspectRatio :
                    AspectRatioOptions();
                for (const auto& layer : p.videoFrame.front().layers)
                {
                    const auto& image = layer.image ? layer.image : layer.imageB;
                    const auto& bounds = layer.image ? layer.bounds : layer.boundsB;
                    if (image && bounds.has_value())
                    {
                        // The image is fitted into its canvas box, so use
                        // the same box the renderer draws into.
                        box = getBox(
                            ftk::Box2I(
                                ftk::V2I(
                                    std::lround(bounds.value().min.x),
                                    std::lround(bounds.value().min.y)),
                                ftk::V2I(
                                    std::lround(bounds.value().max.x),
                                    std::lround(bounds.value().max.y))),
                            image->getInfo(),
                            aspectRatio);
                        size = image->getInfo().size;
                        return box.w() > 0 && box.h() > 0 && size.isValid();
                    }
                }
            }
            return false;
        }

        std::optional<ftk::V2I> Viewport::_toSourcePixel(const ftk::V2I& renderPos) const
        {
            FTK_P();
            // Which image the position is over, and where in it. Side by
            // side comparisons give every image its own box, so a position
            // has to be measured against the one it is over rather than
            // against the first -- otherwise the second image reports where
            // it sits in the first's coordinates, which is a pixel the file
            // does not have. Over no image there is no pixel to name, so
            // nothing is returned rather than a position carried on past
            // the edge.
            const auto& displayOptions = p.displayOptions->get();
            const AspectRatioOptions aspectRatio =
                !displayOptions.empty() ?
                displayOptions.front().aspectRatio :
                AspectRatioOptions();
            const auto boxes = getBoxes(
                p.compareOptions->get(), aspectRatio, p.videoFrame);
            for (size_t i = 0; i < boxes.size() && i < p.videoFrame.size(); ++i)
            {
                if (!ftk::contains(boxes[i], renderPos))
                    continue;
                if (p.videoFrame[i].canvasSize.isValid())
                {
                    // With OTIO spatial coordinates the box holds the
                    // timeline canvas rather than the media, so the position
                    // crosses into the canvas before it can find an image.
                    const ftk::V2I canvasPos = mapInto(
                        renderPos, boxes[i], p.videoFrame[i].canvasSize);
                    for (const auto& layer : p.videoFrame[i].layers)
                    {
                        const auto& image = layer.image ? layer.image : layer.imageB;
                        const auto& bounds = layer.image ? layer.bounds : layer.boundsB;
                        if (image && bounds.has_value())
                        {
                            const ftk::Box2I box = getBox(
                                ftk::Box2I(
                                    ftk::V2I(
                                        std::lround(bounds.value().min.x),
                                        std::lround(bounds.value().min.y)),
                                    ftk::V2I(
                                        std::lround(bounds.value().max.x),
                                        std::lround(bounds.value().max.y))),
                                image->getInfo(),
                                aspectRatio);
                            if (ftk::contains(box, canvasPos))
                            {
                                return mapInto(
                                    canvasPos, box, image->getInfo().size);
                            }
                        }
                    }
                    return std::nullopt;
                }
                for (const auto& layer : p.videoFrame[i].layers)
                {
                    const auto& image = layer.image ? layer.image : layer.imageB;
                    if (image)
                    {
                        return mapInto(
                            renderPos, boxes[i], image->getInfo().size);
                    }
                }
            }
            return std::nullopt;
        }

        ftk::V2I Viewport::_fromSourcePixel(const ftk::V2I& sourcePos) const
        {
            ftk::Box2I box;
            ftk::Size2I size;
            ftk::V2I out = sourcePos;
            if (_getSourceBox(box, size))
            {
                out = ftk::V2I(
                    std::lround(
                        box.min.x + sourcePos.x /
                        static_cast<double>(size.w) * box.w()),
                    std::lround(
                        box.min.y + sourcePos.y /
                        static_cast<double>(size.h) * box.h()));
            }
            return out;
        }

        void Viewport::_sampleUpdate()
        {
            FTK_P();
            const ftk::V2I& pos = p.samplePos->get();
            const auto pixel = _toSourcePixel(toRenderPos(pos));
            p.pick->setIfChanged(pixel);
            // The color is read back from what was rendered, so away from
            // the media it is the background rather than a value from
            // anything being reviewed.
            p.colorSample->setIfChanged(pixel.has_value() ?
                std::optional<ftk::Color4F>(getColorSample(pos)) :
                std::nullopt);
        }

        bool Viewport::isInputEnabled() const
        {
            return _p->inputEnabled;
        }

        void Viewport::setInputEnabled(bool value)
        {
            _p->inputEnabled = value;
        }

        void Viewport::setPanBinding(ftk::MouseButton button, ftk::KeyModifier modifier)
        {
            _p->panBinding = std::make_pair(button, modifier);
        }

        void Viewport::setWipeBinding(ftk::MouseButton button, ftk::KeyModifier modifier)
        {
            _p->wipeBinding = std::make_pair(button, modifier);
        }

        void Viewport::setPickBinding(ftk::MouseButton button, ftk::KeyModifier modifier)
        {
            _p->pickBinding = std::make_pair(button, modifier);
        }

        void Viewport::setMouseWheelScale(float value)
        {
            _p->mouseWheelScale = value;
        }
        
        ftk::Size2I Viewport::getSizeHint() const
        {
            FTK_P();
            return ftk::Size2I(p.size.sizeHint, p.size.sizeHint);
        }

        void Viewport::setGeometry(const ftk::Box2I& value)
        {
            const bool changed = value != getGeometry();
            IWidget::setGeometry(value);
            FTK_P();
            if (changed)
            {
                p.doRender = true;
            }
        }

        void Viewport::tickEvent(
            bool parentsVisible,
            bool parentsEnabled,
            const ftk::TickEvent& event)
        {
            IWidget::tickEvent(parentsVisible, parentsEnabled, event);
            FTK_P();
            if (Private::Resample::Read == p.resample)
            {
                p.resample = Private::Resample::None;
                // Through the same path as a pick: a comparison can bring a
                // different image under the position, or none at all.
                _sampleUpdate();
            }
        }

        void Viewport::sizeHintEvent(const ftk::SizeHintEvent& event)
        {
            FTK_P();
            p.size.displayScale = event.displayScale;
            p.size.sizeHint = event.style->getSizeRole(ftk::SizeRole::ScrollArea, event.displayScale);
        }

        void Viewport::drawEvent(const ftk::Box2I& drawRect, const ftk::DrawEvent& event)
        {
            IWidget::drawEvent(drawRect, event);
            FTK_P();

            if (p.frameView->get())
            {
                _frameView();
            }

            auto render = std::dynamic_pointer_cast<IRender>(event.render);
            const ftk::Box2I& g = getGeometry();
            render->drawRect(g, ftk::Color4F(0.F, 0.F, 0.F));

            const auto& bgOptions = p.bgOptions->get();
            if (p.doRender)
            {
                p.doRender = false;
                try
                {
                    // Create the background and foreground buffers.
                    const ftk::Size2I size = g.size();
                    const auto& fgOptions = p.fgOptions->get();
                    ftk::gl::OffscreenBufferOptions offscreenBufferOptions;
                    offscreenBufferOptions.colorFilters.minify = ftk::ImageFilter::Nearest;
                    offscreenBufferOptions.colorFilters.magnify = ftk::ImageFilter::Nearest;
                    if (bgOptions.type != tl::Background::Solid ||
                        bgOptions.outline.enabled)
                    {
                        if (ftk::gl::doCreate(
                            p.bgBuffer,
                            size,
                            ftk::gl::TextureType::RGBA_U8,
                            offscreenBufferOptions))
                        {
                            p.bgBuffer = ftk::gl::OffscreenBuffer::create(
                                size,
                                ftk::gl::TextureType::RGBA_U8,
                                offscreenBufferOptions);
                        }
                    }
                    else
                    {
                        p.bgBuffer.reset();
                    }
                    if (fgOptions.grid.enabled || fgOptions.centerMarker.enabled)
                    {
                        if (ftk::gl::doCreate(
                            p.fgBuffer,
                            size,
                            ftk::gl::TextureType::RGBA_U8,
                            offscreenBufferOptions))
                        {
                            p.fgBuffer = ftk::gl::OffscreenBuffer::create(
                                size,
                                ftk::gl::TextureType::RGBA_U8,
                                offscreenBufferOptions);
                        }
                    }
                    else
                    {
                        p.fgBuffer.reset();
                    }

                    // Create the main buffer.
                    offscreenBufferOptions.colorFilters.minify = ftk::ImageFilter::Linear;
                    offscreenBufferOptions.colorFilters.magnify = ftk::ImageFilter::Linear;
                    if (!p.imageOptions->isEmpty())
                    {
                        offscreenBufferOptions.colorFilters = p.imageOptions->getItem(0).imageFilters;
                    }
#if defined(FTK_API_GL_4_1)
                    offscreenBufferOptions.depth = ftk::gl::OffscreenDepth::_24;
                    offscreenBufferOptions.stencil = ftk::gl::OffscreenStencil::_8;
#elif defined(FTK_API_GLES_3)
                    offscreenBufferOptions.stencil = ftk::gl::OffscreenStencil::_8;
#endif // FTK_API_GL_4_1
                    if (ftk::gl::doCreate(
                        p.buffer,
                        size,
                        p.colorBuffer->get(),
                        offscreenBufferOptions))
                    {
                        p.buffer = ftk::gl::OffscreenBuffer::create(
                            size,
                            p.colorBuffer->get(),
                            offscreenBufferOptions);
                    }

                    // Setup the transforms.
                    const auto pm = ftk::ortho(
                        0.F,
                        static_cast<float>(g.w()),
                        static_cast<float>(g.h()),
                        0.F,
                        -1.F,
                        1.F);
                    const auto& compareOptions = p.compareOptions->get();
                    const auto& displayOptions = p.displayOptions->get();
                    const auto boxes = getBoxes(
                        compareOptions,
                        !displayOptions.empty() ? displayOptions.front().aspectRatio : AspectRatioOptions(),
                        p.videoFrame);
                    const ftk::V2I& viewPos = p.viewPos->get();
                    const double zoom = p.zoom->get();
                    const ftk::M44F vm =
                        ftk::translate(ftk::V3F(viewPos.x, viewPos.y, 0.F)) *
                        ftk::scale(ftk::V3F(zoom, zoom, 1.F));

                    // Setup the state.
                    const ftk::ViewportState viewportState(render);
                    const ftk::ClipRectEnabledState clipRectEnabledState(render);
                    const ftk::ClipRectState clipRectState(render);
                    const ftk::TransformState transformState(render);
                    const ftk::RenderSizeState renderSizeState(render);
                    render->setRenderSize(size);
                    render->setViewport(ftk::Box2I(0, 0, g.w(), g.h()));
                    render->setClipRectEnabled(false);

                    // Draw the main buffer.
                    if (p.buffer)
                    {
                        ftk::gl::OffscreenBufferBinding binding(p.buffer);
                        render->clearViewport(ftk::Color4F(0.F, 0.F, 0.F, 0.F));
                        render->setOCIOOptions(p.ocioOptions->get());
                        render->setOCIOInputResolver(p.ocioInputResolver);
                        render->setLUTOptions(p.lutOptions->get());
                        render->setTransform(pm * vm);
                        render->drawVideo(
                            p.videoFrame,
                            boxes,
                            p.imageOptions->get(),
                            p.displayOptions->get(),
                            compareOptions,
                            p.colorBuffer->get());
                    }

                    // Draw the background buffer.
                    if (p.bgBuffer)
                    {
                        ftk::gl::OffscreenBufferBinding binding(p.bgBuffer);
                        render->clearViewport(ftk::Color4F(0.F, 0.F, 0.F, 0.F));
                        render->setTransform(pm);
                        render->drawBackground(boxes, vm, bgOptions, compareOptions);
                    }

                    // Draw the foreground buffer.
                    if (p.fgBuffer)
                    {
                        ftk::gl::OffscreenBufferBinding binding(p.fgBuffer);
                        render->clearViewport(ftk::Color4F(0.F, 0.F, 0.F, 0.F));
                        render->setTransform(pm);
                        ForegroundOptions fgOptionsTmp = fgOptions;
                        fgOptionsTmp.grid.fontInfo.size *= p.size.displayScale;
                        fgOptionsTmp.grid.textMargin *= p.size.displayScale;
                        render->drawForeground(boxes, vm, fgOptionsTmp, compareOptions);
                    }
                }
                catch (const std::exception& e)
                {
                    if (auto context = getContext())
                    {
                        context->log("tl::ui::Viewport", e.what(), ftk::LogType::Error);
                    }
                }
            }

            if (p.bgBuffer)
            {
                render->drawTexture(p.bgBuffer->getColorID(), g, true);
            }
            else if (tl::Background::Solid == bgOptions.type)
            {
                // Optimize drawing solid backgrounds.
                render->drawRect(g, bgOptions.solidColor);
            }
            if (p.buffer)
            {
                ftk::AlphaBlend alphaBlend = ftk::AlphaBlend::Straight;
                if (!p.imageOptions->isEmpty() &&
                    p.imageOptions->getItem(0).alphaBlend != ftk::AlphaBlend::None)
                {
                    alphaBlend = p.imageOptions->getItem(0).alphaBlend;
                }
                render->drawTexture(
                    p.buffer->getColorID(),
                    g,
                    true,
                    ftk::Color4F(1.F, 1.F, 1.F),
                    alphaBlend);
            }
            if (p.fgBuffer)
            {
                render->drawTexture(p.fgBuffer->getColorID(), g, true);
            }

            _drawMissingIndicators(event);

            if (Private::Resample::Wait == p.resample)
            {
                // This drawing carries the new picture; the next tick
                // reads it.
                p.resample = Private::Resample::Read;
            }
        }

        void Viewport::_drawMissingIndicators(const ftk::DrawEvent& event)
        {
            FTK_P();
            const auto& indicator = p.fgOptions->get().missingIndicator;
            if (!indicator.enabled || p.videoFrame.empty())
            {
                return;
            }

            // Drawn here rather than into the foreground buffer so that it is
            // in the widget's own coordinates: clamping to the part of the
            // image that is on screen is what keeps it visible when the view
            // is zoomed inside the picture.
            const ftk::Box2I& g = getGeometry();
            const auto boxes = getBoxes(
                p.compareOptions->get(),
                !p.displayOptions->get().empty() ?
                    p.displayOptions->get().front().aspectRatio :
                    AspectRatioOptions(),
                p.videoFrame);
            const ftk::V2I& viewPos = p.viewPos->get();
            const double zoom = p.zoom->get();

            for (size_t i = 0; i < boxes.size() && i < p.videoFrame.size(); ++i)
            {
                // Any layer standing in makes the picture a stand-in: what is
                // on screen is not what was asked for either way.
                bool missing = false;
                for (const auto& layer : p.videoFrame[i].layers)
                {
                    missing |= layer.missing;
                }
                if (!missing)
                {
                    continue;
                }

                const ftk::Box2I& box = boxes[i];
                const ftk::Box2I image(
                    g.min.x + viewPos.x + static_cast<int>(box.min.x * zoom),
                    g.min.y + viewPos.y + static_cast<int>(box.min.y * zoom),
                    static_cast<int>(box.w() * zoom),
                    static_cast<int>(box.h() * zoom));
                const ftk::Box2I visible = ftk::intersect(image, g);
                if (!visible.isValid())
                {
                    continue;
                }

                // A cross over what can be seen of the picture. Drawn as
                // two quads rather than lines so the width is in pixels
                // whatever the shape of the box.
                const float w = std::max(1, indicator.width) / 2.F;
                const ftk::V2F corners[2][2] =
                {
                    {
                        ftk::V2F(visible.min.x, visible.min.y),
                        ftk::V2F(visible.max.x + 1, visible.max.y + 1)
                    },
                    {
                        ftk::V2F(visible.min.x, visible.max.y + 1),
                        ftk::V2F(visible.max.x + 1, visible.min.y)
                    }
                };
                ftk::TriMesh2F mesh;
                for (const auto& diagonal : corners)
                {
                    const ftk::V2F d(
                        diagonal[1].x - diagonal[0].x,
                        diagonal[1].y - diagonal[0].y);
                    const float length = std::sqrt(d.x * d.x + d.y * d.y);
                    if (length <= 0.F)
                    {
                        continue;
                    }
                    // Across the line, so the quad has the given thickness.
                    const ftk::V2F n(-d.y / length * w, d.x / length * w);
                    const size_t v = mesh.v.size();
                    mesh.v.push_back(ftk::V2F(
                        diagonal[0].x + n.x, diagonal[0].y + n.y));
                    mesh.v.push_back(ftk::V2F(
                        diagonal[1].x + n.x, diagonal[1].y + n.y));
                    mesh.v.push_back(ftk::V2F(
                        diagonal[1].x - n.x, diagonal[1].y - n.y));
                    mesh.v.push_back(ftk::V2F(
                        diagonal[0].x - n.x, diagonal[0].y - n.y));
                    // Vertex indices are one based.
                    mesh.triangles.push_back({
                        ftk::Vertex2(v + 1),
                        ftk::Vertex2(v + 2),
                        ftk::Vertex2(v + 3) });
                    mesh.triangles.push_back({
                        ftk::Vertex2(v + 1),
                        ftk::Vertex2(v + 3),
                        ftk::Vertex2(v + 4) });
                }
                if (!mesh.triangles.empty())
                {
                    event.render->drawMesh(mesh, indicator.color);
                }
            }
        }

        void Viewport::mouseEnterEvent(ftk::MouseEnterEvent& event)
        {
            FTK_P();
            if (p.inputEnabled)
            {
                event.accept = true;
                p.mouse.inside = true;
                p.mouse.pos = toViewportPos(event.pos);
            }
        }

        void Viewport::mouseLeaveEvent()
        {
            FTK_P();
            p.mouse.inside = false;
        }

        void Viewport::mouseMoveEvent(ftk::MouseMoveEvent& event)
        {
            FTK_P();
            if (p.inputEnabled)
            {
                event.accept = true;

                p.mouse.pos = toViewportPos(event.pos);

                switch (p.mouse.mode)
                {
                case Private::MouseMode::View:
                {
                    const ftk::V2I viewPos = p.mouse.viewPos + (p.mouse.pos - p.mouse.press);
                    const double zoom = p.zoom->get();
                    const std::pair<ftk::V2I, double> pair(viewPos, zoom);
                    if (pair != p.viewPosZoom->get())
                    {
                        setFrameView(false);
                    }
                    if (p.viewPosZoom->setIfChanged(std::make_pair(viewPos, zoom)))
                    {
                        p.viewPos->setIfChanged(viewPos);
                        p.zoom->setIfChanged(zoom);
                        p.doRender = true;
                        setDrawUpdate();
                    }
                    break;
                }
                case Private::MouseMode::Picker:
                    p.picked = true;
                    if (p.samplePos->setIfChanged(p.mouse.pos))
                    {
                        _sampleUpdate();
                    }
                    break;
                case Private::MouseMode::Wipe:
                {
                    if (p.player)
                    {
                        const IOInfo& ioInfo = p.player->getIOInfo();
                        if (!ioInfo.video.empty())
                        {
                            const ftk::V2I& viewPos = p.viewPos->get();
                            const double zoom = p.zoom->get();
                            const auto& imageInfo = ioInfo.video[0];
                            CompareOptions compareOptions = p.compareOptions->get();
                            compareOptions.wipeCenter.x = (p.mouse.pos.x - viewPos.x) / zoom /
                                static_cast<float>(imageInfo.size.w * imageInfo.pixelAspectRatio);
                            compareOptions.wipeCenter.y = (p.mouse.pos.y - viewPos.y) / zoom /
                                static_cast<float>(imageInfo.size.h);
                            if (p.compareOptions->setIfChanged(compareOptions))
                            {
                                p.doRender = true;
                                setDrawUpdate();
                            }
                        }
                    }
                    break;
                }
                default: break;
                }
            }
        }

        void Viewport::mousePressEvent(ftk::MouseClickEvent& event)
        {
            FTK_P();
            if (p.inputEnabled)
            {
                p.mouse.pos = toViewportPos(event.pos);
                p.mouse.press = p.mouse.pos;

                if (p.panBinding.first == event.button &&
                    ftk::checkKeyModifier(p.panBinding.second, event.modifiers))
                {
                    p.mouse.mode = Private::MouseMode::View;
                    p.mouse.viewPos = p.viewPos->get();
                }
                else if (p.wipeBinding.first == event.button &&
                    ftk::checkKeyModifier(p.wipeBinding.second, event.modifiers))
                {
                    p.mouse.mode = Private::MouseMode::Wipe;
                }
                else if (p.pickBinding.first == event.button &&
                    ftk::checkKeyModifier(p.pickBinding.second, event.modifiers))
                {
                    p.mouse.mode = Private::MouseMode::Picker;
                    p.picked = true;
                    if (p.samplePos->setIfChanged(p.mouse.pos))
                    {
                        _sampleUpdate();
                    }
                }
                else
                {
                    p.mouse.mode = Private::MouseMode::None;
                }

                // Only claim the button when it is bound to something.
                // Accepting a button we do nothing with would deny it to
                // everything else, including a context menu. Derived
                // classes with their own bindings accept in addition.
                if (p.mouse.mode != Private::MouseMode::None)
                {
                    event.accept = true;
                    takeKeyFocus();
                }
            }
        }

        void Viewport::mouseReleaseEvent(ftk::MouseClickEvent& event)
        {
            FTK_P();
            event.accept = true;
            p.mouse.mode = Private::MouseMode::None;
        }

        void Viewport::scrollEvent(ftk::ScrollEvent& event)
        {
            FTK_P();
            if (p.inputEnabled)
            {
                if (static_cast<int>(ftk::KeyModifier::None) == event.modifiers)
                {
                    event.accept = true;

                    p.mouse.pos = toViewportPos(event.pos);

                    const double zoom = p.zoom->get();
                    const double newZoom =
                        event.value.y > 0 ?
                        zoom * p.mouseWheelScale :
                        zoom / p.mouseWheelScale;
                    setZoom(newZoom, p.mouse.pos);
                }
                else if (event.modifiers & static_cast<int>(ftk::KeyModifier::Control))
                {
                    event.accept = true;

                    if (p.player)
                    {
                        const OTIO_NS::RationalTime t = p.player->getCurrentTime();
                        p.player->seek(t + OTIO_NS::RationalTime(event.value.y, t.rate()));
                    }
                }
            }
        }

        void Viewport::keyPressEvent(ftk::KeyEvent& event)
        {
            FTK_P();
            if (p.inputEnabled)
            {
                p.mouse.pos = toViewportPos(event.pos);

                if (0 == event.modifiers)
                {
                    switch (event.key)
                    {
                    case ftk::Key::_0:
                        event.accept = true;
                        setZoom(1.0, p.mouse.pos);
                        break;

                    case ftk::Key::Equals:
                        event.accept = true;
                        setZoom(p.zoom->get() * 2.0, p.mouse.pos);
                        break;

                    case ftk::Key::Minus:
                        event.accept = true;
                        setZoom(p.zoom->get() / 2.0, p.mouse.pos);
                        break;

                    case ftk::Key::Backspace:
                        event.accept = true;
                        setFrameView(true);
                        break;

                    default: break;
                    }
                }
            }
        }

        void Viewport::keyReleaseEvent(ftk::KeyEvent& event)
        {
            event.accept = true;
        }

        bool Viewport::_isMouseInside() const
        {
            return _p->mouse.inside;
        }

        const ftk::V2I& Viewport::_getMousePressPos() const
        {
            return _p->mouse.press;
        }

        ftk::Size2I Viewport::_getRenderSize() const
        {
            FTK_P();
            const auto& displayOptions = p.displayOptions->get();
            return getRenderSize(
                p.compareOptions->get(),
                !displayOptions.empty() ? displayOptions.front().aspectRatio : AspectRatioOptions(),
                p.videoFrame);
        }

        ftk::V2I Viewport::_getViewportCenter() const
        {
            const ftk::Box2I& g = getGeometry();
            return ftk::V2I(g.w() / 2, g.h() / 2);
        }

        void Viewport::_frameView()
        {
            FTK_P();
            ftk::V2I viewPos;
            double zoom = 1.0;
            const ftk::Size2I renderSize = _getRenderSize();
            if (renderSize.isValid())
            {
                const ftk::Box2I& g = getGeometry();
                const ftk::Size2I viewportSize = g.size();
                zoom = viewportSize.w / static_cast<double>(renderSize.w);
                if (zoom * renderSize.h > viewportSize.h)
                {
                    zoom = viewportSize.h / static_cast<double>(renderSize.h);
                }
                viewPos = ftk::V2I(
                    viewportSize.w / 2.F - renderSize.w / 2 * zoom,
                    viewportSize.h / 2.F - renderSize.h / 2 * zoom);
            }
            if (p.viewPosZoom->setIfChanged(std::make_pair(viewPos, zoom)))
            {
                p.viewPos->setIfChanged(viewPos);
                p.zoom->setIfChanged(zoom);
            }
        }

    }
}

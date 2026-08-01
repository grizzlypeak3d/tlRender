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
        struct Viewport::Private
        {
            std::shared_ptr<ftk::Observable<CompareOptions> > compareOptions;
            std::shared_ptr<ftk::Observable<OCIOOptions> > ocioOptions;
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
            float mouseWheelScale = 1.1F;

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
                Wipe
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
                ftk::gl::TextureType::RGBA_F32);
            p.viewPos = ftk::Observable<ftk::V2I>::create();
            p.zoom = ftk::Observable<double>::create(1.0);
            p.viewPosZoom = ftk::Observable<std::pair<ftk::V2I, double> >::create(
                std::make_pair(ftk::V2I(), 1.0));
            p.frameView = ftk::Observable<bool>::create(true);
            p.framed = ftk::Observable<bool>::create(false);
            p.fps = ftk::Observable<double>::create(0.0);
            p.droppedFrames = ftk::Observable<size_t>::create(0);
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

                        if (p.fpsData.has_value())
                        {
                            p.fpsData->frameCount = p.fpsData->frameCount + 1;
                            const auto now = std::chrono::steady_clock::now();
                            const std::chrono::duration<double> diff = now - p.fpsData->timer;
                            if (diff.count() > 1.0)
                            {
                                const double fps = p.fpsData->frameCount / diff.count();
                                //std::cout << "FPS: " << fps << std::endl;
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
            else if (!p.videoFrame.empty())
            {
                p.videoFrame.clear();
                p.doRender = true;
                setDrawUpdate();
            }
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
                    if (!p.displayOptions->isEmpty())
                    {
                        offscreenBufferOptions.colorFilters = p.displayOptions->getItem(0).imageFilters;
                    }
#if defined(FTK_API_GL_4_1)
                    offscreenBufferOptions.depth = ftk::gl::OffscreenDepth::_24;
                    offscreenBufferOptions.stencil = ftk::gl::OffscreenStencil::_8;
#elif defined(FTK_API_GLES_2)
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
                p.mouse.pos = event.pos - getGeometry().min;
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

                const ftk::Box2I& g = getGeometry();
                p.mouse.pos = event.pos - g.min;

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
                const ftk::Box2I& g = getGeometry();
                p.mouse.pos = event.pos - g.min;
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

                    const ftk::Box2I& g = getGeometry();
                    p.mouse.pos = event.pos - g.min;

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
                const ftk::Box2I& g = getGeometry();
                p.mouse.pos = event.pos - g.min;

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

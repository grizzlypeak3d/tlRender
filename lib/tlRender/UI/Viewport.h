// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/UI/Export.h>
#include <tlRender/Timeline/BackgroundOptions.h>
#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/DisplayOptions.h>
#include <tlRender/Timeline/ForegroundOptions.h>
#include <tlRender/Timeline/Player.h>

#include <ftk/UI/IWidget.h>
#include <ftk/GL/Texture.h>

namespace tl
{
    namespace ui
    {
        //! Timeline viewport.
        class TL_UI_API_TYPE Viewport : public ftk::IWidget
        {
            FTK_NON_COPYABLE(Viewport);

        protected:
            TL_UI_API void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent);

            TL_UI_API Viewport();

        public:
            TL_UI_API virtual ~Viewport();

            //! Create a new widget.
            TL_UI_API static std::shared_ptr<Viewport> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! \name Comparison Options
            ///@{

            TL_UI_API const CompareOptions& getCompareOptions() const;
            TL_UI_API std::shared_ptr<ftk::IObservable<CompareOptions> > observeCompareOptions() const;
            TL_UI_API void setCompareOptions(const CompareOptions&);

            ///@}

            //! \name OpenColorIO Options
            ///@{

            TL_UI_API const OCIOOptions& getOCIOOptions() const;
            TL_UI_API std::shared_ptr<ftk::IObservable<OCIOOptions> > observeOCIOOptions() const;
            TL_UI_API void setOCIOOptions(const OCIOOptions&);

            //! Set a resolver for per layer OCIO input color spaces; see
            //! IRender::setOCIOInputResolver().
            TL_UI_API void setOCIOInputResolver(
                const std::function<std::string(
                    const std::string& path,
                    const ftk::ImageTags&)>&);

            ///@}

            //! \name LUT Options
            ///@{

            TL_UI_API const LUTOptions& getLUTOptions() const;
            TL_UI_API std::shared_ptr<ftk::IObservable<LUTOptions> > observeLUTOptions() const;
            TL_UI_API void setLUTOptions(const LUTOptions&);

            ///@}

            //! \name Image Options
            ///@{

            TL_UI_API const std::vector<ftk::ImageOptions>& getImageOptions() const;
            TL_UI_API std::shared_ptr<ftk::IObservableList<ftk::ImageOptions> > observeImageOptions() const;
            TL_UI_API void setImageOptions(const std::vector<ftk::ImageOptions>&);

            ///@}

            //! \name Display Options
            ///@{

            TL_UI_API const std::vector<tl::DisplayOptions>& getDisplayOptions() const;
            TL_UI_API std::shared_ptr<ftk::IObservableList<tl::DisplayOptions> > observeDisplayOptions() const;
            TL_UI_API void setDisplayOptions(const std::vector<tl::DisplayOptions>&);

            ///@}

            //! \name Background Options
            ///@{

            TL_UI_API const BackgroundOptions& getBackgroundOptions() const;
            TL_UI_API std::shared_ptr<ftk::IObservable<BackgroundOptions> > observeBackgroundOptions() const;
            TL_UI_API void setBackgroundOptions(const BackgroundOptions&);

            //! \name Foreground Options
            ///@{

            TL_UI_API const ForegroundOptions& getForegroundOptions() const;
            TL_UI_API std::shared_ptr<ftk::IObservable<ForegroundOptions> > observeForegroundOptions() const;
            TL_UI_API void setForegroundOptions(const ForegroundOptions&);

            ///@}

            //! \name Color Buffer Type
            ///@{

            TL_UI_API ftk::gl::TextureType getColorBuffer() const;
            TL_UI_API std::shared_ptr<ftk::IObservable<ftk::gl::TextureType> > observeColorBuffer() const;
            TL_UI_API void setColorBuffer(ftk::gl::TextureType);

            ///@}

            //! \name Timeline Player
            ///@{

            TL_UI_API const std::shared_ptr<Player>& getPlayer() const;
            TL_UI_API virtual void setPlayer(const std::shared_ptr<Player>&);

            ///@}

            //! \name Coordinates
            ///@{
            //!
            //! Three spaces meet at the viewport, and mixing them up is the
            //! usual way a pixel readout goes wrong:
            //!
            //! - the window's, which is what an event carries;
            //! - the viewport's own, which is the window's less the position
            //!   the viewport was laid out at;
            //! - the render's, which is what the images are laid out in and
            //!   what a comparison arranges -- the view position and zoom are
            //!   the difference between it and the viewport's.
            //!
            //! Going to a coarser space rounds down rather than towards zero,
            //! so that a position just outside the image stays outside it
            //! rather than landing on the first row or column.

            //! Convert a window position to the viewport's own coordinates.
            TL_UI_API ftk::V2I toViewportPos(const ftk::V2I& windowPos) const;

            //! Convert a position in the viewport's own coordinates to the
            //! render coordinates the images are laid out in.
            TL_UI_API ftk::V2I toRenderPos(const ftk::V2I& viewportPos) const;

            //! Convert a render position back to the viewport's own
            //! coordinates.
            TL_UI_API ftk::V2I fromRenderPos(const ftk::V2I& renderPos) const;

            ///@}

            //! \name View
            ///@{

            //! Get the view position.
            TL_UI_API const ftk::V2I& getViewPos() const;

            //! Observe the view position.
            TL_UI_API std::shared_ptr<ftk::IObservable<ftk::V2I> > observeViewPos() const;

            //! Get the view zoom.
            TL_UI_API double getZoom() const;

            //! Observe the view zoom.
            TL_UI_API std::shared_ptr<ftk::IObservable<double> > observeZoom() const;

            //! Get the view position and zoom.
            TL_UI_API std::pair<ftk::V2I, double> getViewPosAndZoom() const;

            //! Observe the view position and zoom.
            TL_UI_API std::shared_ptr<ftk::IObservable<std::pair<ftk::V2I, double> > > observeViewPosAndZoom() const;

            //! Set the view position and zoom.
            TL_UI_API void setViewPosAndZoom(const ftk::V2I&, double);

            //! Set the view zoom.
            TL_UI_API void setZoom(double, const ftk::V2I& focus = ftk::V2I());

            //! Get the view zoom range.
            TL_UI_API const ftk::RangeD& getZoomRange() const;

            //! Set the view zoom range.
            TL_UI_API void setZoomRange(const ftk::RangeD&);

            //! Get whether the view is framed automatically.
            TL_UI_API bool hasFrameView() const;

            //! Observe whether the view is framed automatically.
            TL_UI_API std::shared_ptr<ftk::IObservable<bool> > observeFrameView() const;

            //! Observe when the view is framed.
            TL_UI_API std::shared_ptr<ftk::IObservable<bool> > observeFramed() const;

            //! Set whether the view is framed automatically.
            TL_UI_API void setFrameView(bool);

            //! Center the view.
            TL_UI_API void center();

            //! Reset the view zoom to 1:1.
            TL_UI_API void resetZoom();

            //! Zoom the view in.
            TL_UI_API void zoomIn();

            //! Zoom the view out.
            TL_UI_API void zoomOut();

            ///@}

            //! \name Frames
            ///@{

            //! Get the frames per second.
            TL_UI_API double getFPS() const;

            //! Observe the frames per second.
            TL_UI_API std::shared_ptr<ftk::IObservable<double> > observeFPS() const;

            //! Get the number of dropped frames during playback.
            TL_UI_API size_t getDroppedFrames() const;

            //! Observe the number of dropped frames during playback.
            TL_UI_API std::shared_ptr<ftk::IObservable<size_t> > observeDroppedFrames() const;
            
            ///@}

            //! \name Color Sample
            ///@{

            //! Sample a color from the viewport, at a position in the
            //! viewport's own coordinates.
            //!
            //! Two things about where the color comes from, both of which
            //! have been mistaken for bugs:
            //!
            //! - It is read back from the buffer the viewport drew into, so
            //!   it is the frame *before* the one being drawn. Sampling from
            //!   drawEvent() returns the picture that is being replaced;
            //!   something that changes the image has to be given a drawing
            //!   before the sample means anything.
            //! - The read is synchronous, and waits for the GPU. Sampling
            //!   every frame stalls the playback being sampled.
            TL_UI_API ftk::Color4F getColorSample(const ftk::V2I&);

            ///@}

            //! \name Input
            ///@{

            //! Get whether input is enabled.
            TL_UI_API bool isInputEnabled() const;

            //! Set whether input is enabled.
            TL_UI_API void setInputEnabled(bool);

            //! Set the pan binding.
            TL_UI_API void setPanBinding(ftk::MouseButton, ftk::KeyModifier);

            //! Set the wipe binding.
            TL_UI_API void setWipeBinding(ftk::MouseButton, ftk::KeyModifier);

            //! Set the mouse wheel scale.
            TL_UI_API void setMouseWheelScale(float);

            ///@}

            TL_UI_API ftk::Size2I getSizeHint() const override;
            TL_UI_API void setGeometry(const ftk::Box2I&) override;
            TL_UI_API void sizeHintEvent(const ftk::SizeHintEvent&) override;
            TL_UI_API void drawEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;
            TL_UI_API void mouseEnterEvent(ftk::MouseEnterEvent&) override;
            TL_UI_API void mouseLeaveEvent() override;
            TL_UI_API void mouseMoveEvent(ftk::MouseMoveEvent&) override;
            TL_UI_API void mousePressEvent(ftk::MouseClickEvent&) override;
            TL_UI_API void mouseReleaseEvent(ftk::MouseClickEvent&) override;
            TL_UI_API void scrollEvent(ftk::ScrollEvent&) override;
            TL_UI_API void keyPressEvent(ftk::KeyEvent&) override;
            TL_UI_API void keyReleaseEvent(ftk::KeyEvent&) override;

        protected:
            bool _isMouseInside() const;
            TL_UI_API const ftk::V2I& _getMousePressPos() const;

        private:
            ftk::Size2I _getRenderSize() const;
            ftk::V2I _getViewportCenter() const;
            void _frameView();
            void _drawMissingIndicators(const ftk::DrawEvent&);

            FTK_PRIVATE();
        };
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

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
        class TL_API_TYPE Viewport : public ftk::IWidget
        {
            FTK_NON_COPYABLE(Viewport);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent);

            Viewport();

        public:
            TL_API virtual ~Viewport();

            //! Create a new widget.
            TL_API static std::shared_ptr<Viewport> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            //! \name Comparison Options
            ///@{

            TL_API const CompareOptions& getCompareOptions() const;
            TL_API std::shared_ptr<ftk::IObservable<CompareOptions> > observeCompareOptions() const;
            TL_API void setCompareOptions(const CompareOptions&);

            ///@}

            //! \name OpenColorIO Options
            ///@{

            TL_API const OCIOOptions& getOCIOOptions() const;
            TL_API std::shared_ptr<ftk::IObservable<OCIOOptions> > observeOCIOOptions() const;
            TL_API void setOCIOOptions(const OCIOOptions&);

            ///@}

            //! \name LUT Options
            ///@{

            TL_API const LUTOptions& getLUTOptions() const;
            TL_API std::shared_ptr<ftk::IObservable<LUTOptions> > observeLUTOptions() const;
            TL_API void setLUTOptions(const LUTOptions&);

            ///@}

            //! \name Image Options
            ///@{

            TL_API const std::vector<ftk::ImageOptions>& getImageOptions() const;
            TL_API std::shared_ptr<ftk::IObservableList<ftk::ImageOptions> > observeImageOptions() const;
            TL_API void setImageOptions(const std::vector<ftk::ImageOptions>&);

            ///@}

            //! \name Display Options
            ///@{

            TL_API const std::vector<tl::DisplayOptions>& getDisplayOptions() const;
            TL_API std::shared_ptr<ftk::IObservableList<tl::DisplayOptions> > observeDisplayOptions() const;
            TL_API void setDisplayOptions(const std::vector<tl::DisplayOptions>&);

            ///@}

            //! \name Background Options
            ///@{

            TL_API const BackgroundOptions& getBackgroundOptions() const;
            TL_API std::shared_ptr<ftk::IObservable<BackgroundOptions> > observeBackgroundOptions() const;
            TL_API void setBackgroundOptions(const BackgroundOptions&);

            //! \name Foreground Options
            ///@{

            TL_API const ForegroundOptions& getForegroundOptions() const;
            TL_API std::shared_ptr<ftk::IObservable<ForegroundOptions> > observeForegroundOptions() const;
            TL_API void setForegroundOptions(const ForegroundOptions&);

            ///@}

            //! \name Color Buffer Type
            ///@{

            TL_API ftk::gl::TextureType getColorBuffer() const;
            TL_API std::shared_ptr<ftk::IObservable<ftk::gl::TextureType> > observeColorBuffer() const;
            TL_API void setColorBuffer(ftk::gl::TextureType);

            ///@}

            //! \name Timeline Player
            ///@{

            TL_API const std::shared_ptr<Player>& getPlayer() const;
            TL_API virtual void setPlayer(const std::shared_ptr<Player>&);

            ///@}

            //! \name View
            ///@{

            //! Get the view position.
            TL_API const ftk::V2I& getViewPos() const;

            //! Observe the view position.
            TL_API std::shared_ptr<ftk::IObservable<ftk::V2I> > observeViewPos() const;

            //! Get the view zoom.
            TL_API double getZoom() const;

            //! Observe the view zoom.
            TL_API std::shared_ptr<ftk::IObservable<double> > observeZoom() const;

            //! Get the view position and zoom.
            TL_API std::pair<ftk::V2I, double> getViewPosAndZoom() const;

            //! Observe the view position and zoom.
            TL_API std::shared_ptr<ftk::IObservable<std::pair<ftk::V2I, double> > > observeViewPosAndZoom() const;

            //! Set the view position and zoom.
            TL_API void setViewPosAndZoom(const ftk::V2I&, double);

            //! Set the view zoom.
            TL_API void setZoom(double, const ftk::V2I& focus = ftk::V2I());

            //! Get the zoom for the active tile.
            TL_API double getTileZoom() const;

            //! Observe the zoom for the active tile.
            TL_API std::shared_ptr<ftk::IObservable<double> > observeTileZoom() const;

            //! Set the zoom for the active tile, or all tiles when synchronized.
            TL_API void setTileZoom(double);

            //! Get whether tile zoom is synchronized.
            TL_API bool isTileZoomSync() const;

            //! Set whether tile zoom is synchronized.
            TL_API void setTileZoomSync(bool);

            //! Get the view zoom range.
            TL_API const ftk::RangeD& getZoomRange() const;

            //! Set the view zoom range.
            TL_API void setZoomRange(const ftk::RangeD&);

            //! Get whether the view is framed automatically.
            TL_API bool hasFrameView() const;

            //! Observe whether the view is framed automatically.
            TL_API std::shared_ptr<ftk::IObservable<bool> > observeFrameView() const;

            //! Observe when the view is framed.
            TL_API std::shared_ptr<ftk::IObservable<bool> > observeFramed() const;

            //! Set whether the view is framed automatically.
            TL_API void setFrameView(bool);

            //! Center the view.
            TL_API void center();

            //! Reset the view zoom to 1:1.
            TL_API void resetZoom();

            //! Zoom the view in.
            TL_API void zoomIn();

            //! Zoom the view out.
            TL_API void zoomOut();

            ///@}

            //! \name Frames
            ///@{

            //! Get the frames per second.
            TL_API double getFPS() const;

            //! Observe the frames per second.
            TL_API std::shared_ptr<ftk::IObservable<double> > observeFPS() const;

            //! Get the number of dropped frames during playback.
            TL_API size_t getDroppedFrames() const;

            //! Observe the number of dropped frames during playback.
            TL_API std::shared_ptr<ftk::IObservable<size_t> > observeDroppedFrames() const;
            
            ///@}

            //! \name Color Sample
            ///@{

            //! Sample a color from the viewport.
            TL_API ftk::Color4F getColorSample(const ftk::V2I&);

            ///@}

            //! \name Input
            ///@{

            //! Get whether input is enabled.
            bool isInputEnabled() const;

            //! Set whether input is enabled.
            void setInputEnabled(bool);

            //! Set the pan binding.
            TL_API void setPanBinding(ftk::MouseButton, ftk::KeyModifier);

            //! Set the wipe binding.
            TL_API void setWipeBinding(ftk::MouseButton, ftk::KeyModifier);

            //! Set the mouse wheel scale.
            TL_API void setMouseWheelScale(float);

            ///@}

            TL_API ftk::Size2I getSizeHint() const override;
            TL_API void setGeometry(const ftk::Box2I&) override;
            TL_API void sizeHintEvent(const ftk::SizeHintEvent&) override;
            TL_API void drawEvent(const ftk::Box2I&, const ftk::DrawEvent&) override;
            TL_API void mouseEnterEvent(ftk::MouseEnterEvent&) override;
            TL_API void mouseLeaveEvent() override;
            TL_API void mouseMoveEvent(ftk::MouseMoveEvent&) override;
            TL_API void mousePressEvent(ftk::MouseClickEvent&) override;
            TL_API void mouseReleaseEvent(ftk::MouseClickEvent&) override;
            TL_API void scrollEvent(ftk::ScrollEvent&) override;
            TL_API void keyPressEvent(ftk::KeyEvent&) override;
            TL_API void keyReleaseEvent(ftk::KeyEvent&) override;

        protected:
            bool _isMouseInside() const;
            const ftk::V2I& _getMousePressPos() const;

        private:
            ftk::Size2I _getRenderSize() const;
            ftk::V2I _getViewportCenter() const;
            void _frameView();
            void _updateActiveTile(const ftk::V2I&);

            FTK_PRIVATE();
        };
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/GPU/Export.h>

#include <tlRender/Timeline/IRender.h>

#include <ftk/GPU/Render.h>

#include <optional>

namespace tl
{
    //! Timeline rendering with SDL's GPU API.
    //!
    //! A spike, beside the OpenGL renderer and drawing what it draws. Not
    //! here yet: the two pass reduction (a reduced picture is sampled
    //! linearly).
    namespace gpu
    {
#if defined(TLRENDER_OCIO)
        struct OCIOData;
#endif // TLRENDER_OCIO

        //! Timeline GPU renderer.
        class TL_GPU_API_TYPE Render : public IRender, public ftk::gpu::IGPURender
        {
            FTK_NON_COPYABLE(Render);

        protected:
            void _init(
                const std::shared_ptr<ftk::gpu::System>&,
                const std::shared_ptr<ftk::LogSystem>&,
                const std::shared_ptr<ftk::FontSystem>&);

            Render();

        public:
            TL_GPU_API virtual ~Render();

            //! Create a new renderer.
            TL_GPU_API static std::shared_ptr<Render> create(
                const std::shared_ptr<ftk::gpu::System>&,
                const std::shared_ptr<ftk::LogSystem>&,
                const std::shared_ptr<ftk::FontSystem>&);

            //! Get the renderer this one draws with, which is where the
            //! target is set and changed.
            TL_GPU_API const std::shared_ptr<ftk::gpu::Render>& getBaseRender() const;

            TL_GPU_API std::shared_ptr<ftk::gpu::Render> getGPURender() override;

            TL_GPU_API void setOCIOOptions(const OCIOOptions&) override;
            TL_GPU_API void setOCIOInputResolver(
                const std::function<std::string(
                    const std::string& path,
                    const ftk::ImageTags&)>&) override;
            TL_GPU_API void setLUTOptions(const LUTOptions&) override;

            TL_GPU_API void drawBackground(
                const std::vector<ftk::Box2I>&,
                const ftk::M44F& vm,
                const BackgroundOptions&,
                const CompareOptions&) override;
            TL_GPU_API void drawVideo(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>& = {},
                const std::vector<DisplayOptions>& = {},
                const CompareOptions& = CompareOptions(),
                ftk::gl::TextureType colorBuffer = ftk::gl::getOffscreenColorDefault()) override;
            TL_GPU_API void drawForeground(
                const std::vector<ftk::Box2I>&,
                const ftk::M44F& vm,
                const ForegroundOptions&,
                const CompareOptions&) override;
            TL_GPU_API void drawClippingWarning(
                unsigned int,
                const ftk::Box2I& rect,
                bool flipV,
                const std::vector<ftk::Box2I>& boxes,
                const ftk::M44F& vm,
                const ClippingWarning&) override;

            TL_GPU_API void begin(
                const ftk::Size2I&,
                const ftk::RenderOptions& = ftk::RenderOptions()) override;
            TL_GPU_API void end() override;
            TL_GPU_API ftk::Size2I getRenderSize() const override;
            TL_GPU_API void setRenderSize(const ftk::Size2I&) override;
            TL_GPU_API ftk::RenderOptions getRenderOptions() const override;
            TL_GPU_API ftk::Box2I getViewport() const override;
            TL_GPU_API void setViewport(const ftk::Box2I&) override;
            TL_GPU_API void clearViewport(const ftk::Color4F&) override;
            TL_GPU_API bool getClipRectEnabled() const override;
            TL_GPU_API void setClipRectEnabled(bool) override;
            TL_GPU_API ftk::Box2I getClipRect() const override;
            TL_GPU_API void setClipRect(const ftk::Box2I&) override;
            TL_GPU_API ftk::M44F getTransform() const override;
            TL_GPU_API void setTransform(const ftk::M44F&) override;
            using ftk::IRender::drawRect;
            TL_GPU_API void drawRect(
                const ftk::Box2F&,
                const ftk::Color4F&) override;
            using ftk::IRender::drawRects;
            TL_GPU_API void drawRects(
                const std::vector<ftk::Box2F>&,
                const ftk::Color4F&) override;
            using ftk::IRender::drawLine;
            TL_GPU_API void drawLine(
                const ftk::V2F&,
                const ftk::V2F&,
                const ftk::Color4F&,
                const ftk::LineOptions& = ftk::LineOptions()) override;
            using ftk::IRender::drawLines;
            TL_GPU_API void drawLines(
                const std::vector<std::pair<ftk::V2F, ftk::V2F> >&,
                const ftk::Color4F&,
                const ftk::LineOptions& = ftk::LineOptions()) override;
            TL_GPU_API void drawMesh(
                const ftk::TriMesh2F&,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F),
                const ftk::V2F& pos = ftk::V2F()) override;
            TL_GPU_API void drawColorMesh(
                const ftk::TriMesh2F&,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F),
                const ftk::V2F& pos = ftk::V2F()) override;
            TL_GPU_API void drawTexture(
                unsigned int,
                const ftk::Box2I&,
                bool flipV = false,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F),
                ftk::AlphaBlend = ftk::AlphaBlend::Straight) override;
            using ftk::IRender::drawText;
            TL_GPU_API void drawText(
                const std::vector<std::shared_ptr<ftk::Glyph> >&,
                const ftk::FontMetrics&,
                const ftk::V2F& position,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F)) override;
            using ftk::IRender::drawImage;
            TL_GPU_API void drawImage(
                const std::shared_ptr<ftk::Image>&,
                const ftk::TriMesh2F&,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F),
                const ftk::ImageOptions& = ftk::ImageOptions()) override;
            TL_GPU_API void drawImage(
                const std::shared_ptr<ftk::Image>&,
                const ftk::Box2F&,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F),
                const ftk::ImageOptions& = ftk::ImageOptions()) override;
            TL_GPU_API ftk::RenderDiag getDiag() const override;

        private:
            //! A half plane, for the wipe: what is on the side of the line
            //! from a to b that c is on.
            struct Clip
            {
                ftk::V2F a;
                ftk::V2F b;
                ftk::V2F c;
            };

            std::string _displayShader(const std::string& ocioInput = std::string());
            std::string _toLinearShader(const std::string& input);
            void _displayShadersReset();
#if defined(TLRENDER_OCIO)
            std::shared_ptr<OCIOData> _ocioData(const std::string& input);
            void _ocioErase(const std::string& ocioKey);
            void _ocioError(const std::string&);
#endif // TLRENDER_OCIO
            std::string _layerOCIOInput(
                const std::string& layerInput,
                const std::string& path,
                const std::shared_ptr<ftk::Image>&);

            std::shared_ptr<ftk::gpu::OffscreenBuffer> _buffer(
                const std::string& name,
                const ftk::Size2I&,
                ftk::gl::TextureType);

            bool _drawVideoPair(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>&,
                const std::vector<DisplayOptions>&,
                ftk::gl::TextureType colorBuffer);
            void _drawVideo(
                const VideoFrame&,
                const ftk::Box2I&,
                const std::shared_ptr<ftk::ImageOptions>&,
                const DisplayOptions&,
                ftk::gl::TextureType colorBuffer,
                const ftk::M44F& mvp,
                const std::optional<Clip>& = std::nullopt);

            FTK_PRIVATE();
        };

        //! Timeline GPU render factory.
        class TL_GPU_API_TYPE RenderFactory : public ftk::IRenderFactory
        {
        public:
            TL_GPU_API RenderFactory(const std::shared_ptr<ftk::gpu::System>&);

            TL_GPU_API std::shared_ptr<ftk::IRender> createRender(
                const std::shared_ptr<ftk::LogSystem>&,
                const std::shared_ptr<ftk::FontSystem>&) override;

        private:
            std::weak_ptr<ftk::gpu::System> _system;
        };
    }
}

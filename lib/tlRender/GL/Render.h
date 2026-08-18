// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/IRender.h>

#include <ftk/GL/Render.h>
#include <ftk/Core/LRUCache.h>

namespace tl
{
    //! Timeline OpenGL support
    namespace gl
    {
        //! Timeline OpenGL renderer.
        class TL_API_TYPE Render : public IRender
        {
            FTK_NON_COPYABLE(Render);

        protected:
            void _init(
                const std::shared_ptr<ftk::LogSystem>&,
                const std::shared_ptr<ftk::FontSystem>&);

            Render();

        public:
            TL_API virtual ~Render();

            //! Create a new renderer.
            TL_API static std::shared_ptr<Render> create(
                const std::shared_ptr<ftk::LogSystem>&,
                const std::shared_ptr<ftk::FontSystem>&);

            TL_API void setOCIOOptions(const OCIOOptions&) override;
            TL_API void setLUTOptions(const LUTOptions&) override;

            TL_API void drawBackground(
                const std::vector<ftk::Box2I>&,
                const ftk::M44F& vm,
                const BackgroundOptions&,
                const CompareOptions&) override;
            TL_API void drawVideo(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>& = {},
                const std::vector<DisplayOptions>& = {},
                const CompareOptions& = CompareOptions(),
                ftk::gl::TextureType colorBuffer = ftk::gl::offscreenColorDefault) override;
            TL_API void drawForeground(
                const std::vector<ftk::Box2I>&,
                const ftk::M44F& vm,
                const ForegroundOptions&,
                const CompareOptions&) override;

            TL_API void begin(
                const ftk::Size2I&,
                const ftk::RenderOptions& = ftk::RenderOptions()) override;
            TL_API void end() override;
            TL_API ftk::Size2I getRenderSize() const override;
            TL_API void setRenderSize(const ftk::Size2I&) override;
            TL_API ftk::RenderOptions getRenderOptions() const override;
            TL_API ftk::Box2I getViewport() const override;
            TL_API void setViewport(const ftk::Box2I&) override;
            TL_API void clearViewport(const ftk::Color4F&) override;
            TL_API bool getClipRectEnabled() const override;
            TL_API void setClipRectEnabled(bool) override;
            TL_API ftk::Box2I getClipRect() const override;
            TL_API void setClipRect(const ftk::Box2I&) override;
            TL_API ftk::M44F getTransform() const override;
            TL_API void setTransform(const ftk::M44F&) override;
            TL_API void drawRect(
                const ftk::Box2F&,
                const ftk::Color4F&) override;
            TL_API void drawRects(
                const std::vector<ftk::Box2F>&,
                const ftk::Color4F&) override;
            TL_API void drawLine(
                const ftk::V2F&,
                const ftk::V2F&,
                const ftk::Color4F&,
                const ftk::LineOptions& = ftk::LineOptions()) override;
            TL_API void drawLines(
                const std::vector<std::pair<ftk::V2F, ftk::V2F> >&,
                const ftk::Color4F&,
                const ftk::LineOptions& = ftk::LineOptions()) override;
            TL_API void drawMesh(
                const ftk::TriMesh2F&,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F),
                const ftk::V2F& pos = ftk::V2F()) override;
            TL_API void drawColorMesh(
                const ftk::TriMesh2F&,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F),
                const ftk::V2F& pos = ftk::V2F()) override;
            TL_API void drawTexture(
                unsigned int,
                const ftk::Box2I&,
                bool flipV = false,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F),
                ftk::AlphaBlend = ftk::AlphaBlend::Straight) override;
            TL_API void drawText(
                const std::vector<std::shared_ptr<ftk::Glyph> >&,
                const ftk::FontMetrics&,
                const ftk::V2F& position,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F)) override;
            TL_API void drawImage(
                const std::shared_ptr<ftk::Image>&,
                const ftk::TriMesh2F&,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F),
                const ftk::ImageOptions& = ftk::ImageOptions()) override;
            TL_API void drawImage(
                const std::shared_ptr<ftk::Image>&,
                const ftk::Box2F&,
                const ftk::Color4F& = ftk::Color4F(1.F, 1.F, 1.F, 1.F),
                const ftk::ImageOptions& = ftk::ImageOptions()) override;
            FTK_API ftk::RenderDiag getDiag() const override;

        private:
            std::shared_ptr<ftk::gl::Shader> _displayShader(
                const std::string& ocioInput = std::string());
            void _displayShadersReset();

            void _drawVideoA(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>&,
                const std::vector<DisplayOptions>&,
                const CompareOptions&,
                ftk::gl::TextureType colorBuffer);
            void _drawVideoB(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>&,
                const std::vector<DisplayOptions>&,
                const CompareOptions&,
                ftk::gl::TextureType colorBuffer);
            void _drawVideoWipe(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>&,
                const std::vector<DisplayOptions>&,
                const CompareOptions&,
                ftk::gl::TextureType colorBuffer);
            void _drawVideoOverlay(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>&,
                const std::vector<DisplayOptions>&,
                const CompareOptions&,
                ftk::gl::TextureType colorBuffer);
            void _drawVideoButterfly(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>&,
                const std::vector<DisplayOptions>&,
                const CompareOptions&,
                ftk::gl::TextureType colorBuffer);
            void _drawVideoDifference(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>&,
                const std::vector<DisplayOptions>&,
                const CompareOptions&,
                ftk::gl::TextureType colorBuffer);
            //! Draw both files into a pair of offscreen buffers, for the
            //! comparisons that combine them a pixel at a time. Answers
            //! whether there are two to combine.
            bool _drawVideoPair(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>&,
                const std::vector<DisplayOptions>&,
                ftk::gl::TextureType colorBuffer);
            //! Draw the pair through a shader that samples both.
            void _drawVideoPairShader(
                const std::string& shader,
                const ftk::Box2I&);
            void _drawVideoTile(
                const std::vector<VideoFrame>&,
                const std::vector<ftk::Box2I>&,
                const std::vector<ftk::ImageOptions>&,
                const std::vector<DisplayOptions>&,
                const CompareOptions&,
                ftk::gl::TextureType colorBuffer);
            void _drawVideo(
                const VideoFrame&,
                const ftk::Box2I&,
                const std::shared_ptr<ftk::ImageOptions>&,
                const DisplayOptions&,
                ftk::gl::TextureType colorBuffer,
                const ftk::M44F& mvp);

            FTK_PRIVATE();
        };

        //! Timeline OpenGL render factory.
        class TL_API_TYPE RenderFactory : public ftk::IRenderFactory
        {
        public:
            TL_API std::shared_ptr<ftk::IRender> createRender(
                const std::shared_ptr<ftk::LogSystem>&,
                const std::shared_ptr<ftk::FontSystem>&) override;
        };
    }
}

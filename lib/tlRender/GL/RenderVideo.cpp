// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GL/RenderPrivate.h>

#include <ftk/GL/GL.h>
#include <ftk/GL/Mesh.h>
#include <ftk/GL/Util.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Math.h>

namespace tl
{
    namespace gl
    {
        namespace
        {
            ftk::Box2I xform(const ftk::Box2I& box, const ftk::M44F& vm)
            {
                ftk::Box2I out;
                const ftk::V3F v0 = vm * ftk::V3F(box.min.x, box.min.y, 0.F);
                const ftk::V3F v1 = vm * ftk::V3F(box.max.x + 1, box.max.y + 1, 0.F);
                out.min.x = std::round(v0.x);
                out.min.y = std::round(v0.y);
                out.max.x = std::round(v1.x - 1);
                out.max.y = std::round(v1.y - 1);
                return out;
            }
        }

        void Render::drawBackground(
            const std::vector<ftk::Box2I>& boxes,
            const ftk::M44F& vm,
            const BackgroundOptions& options,
            const CompareOptions& compareOptions)
        {
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

            // Draw the background.
            const ftk::Box2I rect(ftk::V2I(0, 0), getRenderSize());
            switch (options.type)
            {
            case Background::Solid:
                IRender::drawRect(rect, options.solidColor);
                break;
            case Background::Checkers:
                drawColorMesh(
                    ftk::checkers(
                        rect,
                        options.checkersColor.first,
                        options.checkersColor.second,
                        options.checkersSize),
                    ftk::Color4F(1.F, 1.F, 1.F));
                break;
            case Background::Gradient:
            {
                ftk::TriMesh2F mesh;
                mesh.v.push_back(ftk::V2F(rect.min.x, rect.min.y));
                mesh.v.push_back(ftk::V2F(rect.max.x, rect.min.y));
                mesh.v.push_back(ftk::V2F(rect.max.x, rect.max.y));
                mesh.v.push_back(ftk::V2F(rect.min.x, rect.max.y));
                mesh.c.push_back(ftk::V4F(
                    options.gradientColor.first.r,
                    options.gradientColor.first.g,
                    options.gradientColor.first.b,
                    options.gradientColor.first.a));
                mesh.c.push_back(ftk::V4F(
                    options.gradientColor.second.r,
                    options.gradientColor.second.g,
                    options.gradientColor.second.b,
                    options.gradientColor.second.a));
                mesh.triangles.push_back({
                    ftk::Vertex2(1, 0, 1),
                    ftk::Vertex2(3, 0, 2),
                    ftk::Vertex2(2, 0, 1), });
                mesh.triangles.push_back({
                    ftk::Vertex2(1, 0, 1),
                    ftk::Vertex2(4, 0, 2),
                    ftk::Vertex2(3, 0, 2), });
                drawColorMesh(
                    mesh,
                    ftk::Color4F(1.F, 1.F, 1.F));
                break;
            }
            default: break;
            }

            // Draw the outline.
            if (options.outline.enabled && !boxes.empty())
            {
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

                size_t start = 0;
                size_t end = boxes.size();
                switch (compareOptions.compare)
                {
                    case Compare::None:
                        if (!boxes.empty())
                        {
                            end = 1;
                        }
                        break;
                    case Compare::B:
                        if (boxes.size() > 1)
                        {
                            start = 1;
                            end = 2;
                        }
                        break;
                    default: break;
                }
                for (size_t i = start; i < end; ++i)
                {
                    const ftk::Box2I box = xform(boxes[i], vm);

                    ftk::TriMesh2F mesh;
                    mesh.v.push_back(ftk::V2F(box.min.x, box.min.y));
                    mesh.v.push_back(ftk::V2F(box.max.x + 1, box.min.y));
                    mesh.v.push_back(ftk::V2F(box.max.x + 1, box.max.y + 1));
                    mesh.v.push_back(ftk::V2F(box.min.x, box.max.y + 1));
                    const int w = options.outline.width;
                    mesh.v.push_back(ftk::V2F(box.min.x - w, box.min.y - w));
                    mesh.v.push_back(ftk::V2F(box.max.x + 1 + w, box.min.y - w));
                    mesh.v.push_back(ftk::V2F(box.max.x + 1 + w, box.max.y + 1 + w));
                    mesh.v.push_back(ftk::V2F(box.min.x - w, box.max.y + 1 + w));

                    mesh.triangles.push_back({ ftk::Vertex2(1), ftk::Vertex2(2), ftk::Vertex2(5) });
                    mesh.triangles.push_back({ ftk::Vertex2(2), ftk::Vertex2(6), ftk::Vertex2(5) });
                    mesh.triangles.push_back({ ftk::Vertex2(2), ftk::Vertex2(3), ftk::Vertex2(6) });
                    mesh.triangles.push_back({ ftk::Vertex2(3), ftk::Vertex2(7), ftk::Vertex2(6) });
                    mesh.triangles.push_back({ ftk::Vertex2(3), ftk::Vertex2(4), ftk::Vertex2(7) });
                    mesh.triangles.push_back({ ftk::Vertex2(4), ftk::Vertex2(8), ftk::Vertex2(7) });
                    mesh.triangles.push_back({ ftk::Vertex2(4), ftk::Vertex2(1), ftk::Vertex2(8) });
                    mesh.triangles.push_back({ ftk::Vertex2(1), ftk::Vertex2(5), ftk::Vertex2(8) });

                    drawMesh(mesh, options.outline.color);
                }
            }
        }

        namespace
        {
            // The filters for one of the images being drawn: how a picture
            // is sampled is a fact about the picture, not about the colours
            // it is shown in.
            ftk::ImageFilters imageFilters(
                const std::vector<ftk::ImageOptions>& imageOptions,
                size_t index)
            {
                return index < imageOptions.size() ?
                    imageOptions[index].imageFilters :
                    ftk::ImageFilters();
            }
        }

        void Render::drawVideo(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            const CompareOptions& compareOptions,
            ftk::gl::TextureType colorBuffer)
        {
            switch (compareOptions.compare)
            {
            case Compare::None:
                _drawVideoA(
                    videoFrame,
                    boxes,
                    imageOptions,
                    displayOptions,
                    compareOptions,
                    colorBuffer);
                break;
            case Compare::B:
                _drawVideoB(
                    videoFrame,
                    boxes,
                    imageOptions,
                    displayOptions,
                    compareOptions,
                    colorBuffer);
                break;
            case Compare::Wipe:
                _drawVideoWipe(
                    videoFrame,
                    boxes,
                    imageOptions,
                    displayOptions,
                    compareOptions,
                    colorBuffer);
                break;
            case Compare::Butterfly:
                if (videoFrame.size() > 1)
                {
                    _drawVideoButterfly(
                        videoFrame,
                        boxes,
                        imageOptions,
                        displayOptions,
                        compareOptions,
                        colorBuffer);
                }
                else
                {
                    _drawVideoA(
                        videoFrame,
                        boxes,
                        imageOptions,
                        displayOptions,
                        compareOptions,
                        colorBuffer);
                }
                break;
            case Compare::Overlay:
                _drawVideoOverlay(
                    videoFrame,
                    boxes,
                    imageOptions,
                    displayOptions,
                    compareOptions,
                    colorBuffer);
                break;
            case Compare::Difference:
                if (videoFrame.size() > 1)
                {
                    _drawVideoDifference(
                        videoFrame,
                        boxes,
                        imageOptions,
                        displayOptions,
                        compareOptions,
                        colorBuffer);
                }
                else
                {
                    _drawVideoA(
                        videoFrame,
                        boxes,
                        imageOptions,
                        displayOptions,
                        compareOptions,
                        colorBuffer);
                }
                break;
            case Compare::Horizontal:
            case Compare::Vertical:
            case Compare::Tile:
                _drawVideoTile(
                    videoFrame,
                    boxes,
                    imageOptions,
                    displayOptions,
                    compareOptions,
                    colorBuffer);
                break;
            default: break;
            }
        }

        void Render::_drawVideoA(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            const CompareOptions& compareOptions,
            ftk::gl::TextureType colorBuffer)
        {
            if (!videoFrame.empty() && !boxes.empty())
            {
                _drawVideo(
                    videoFrame[0],
                    boxes[0],
                    !imageOptions.empty() ? std::make_shared<ftk::ImageOptions>(imageOptions[0]) : nullptr,
                    !displayOptions.empty() ? displayOptions[0] : DisplayOptions(),
                    colorBuffer);
            }
        }

        void Render::_drawVideoB(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            const CompareOptions& compareOptions,
            ftk::gl::TextureType colorBuffer)
        {
            if (videoFrame.size() > 1 && boxes.size() > 1)
            {
                _drawVideo(
                    videoFrame[1],
                    boxes[1],
                    imageOptions.size() > 1 ? std::make_shared<ftk::ImageOptions>(imageOptions[1]) : nullptr,
                    displayOptions.size() > 1 ? displayOptions[1] : DisplayOptions(),
                    colorBuffer);
            }
        }

        void Render::_drawVideoWipe(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            const CompareOptions& compareOptions,
            ftk::gl::TextureType colorBuffer)
        {
            FTK_P();

            float radius = 0.F;
            float x = 0.F;
            float y = 0.F;
            if (!boxes.empty())
            {
                radius = std::max(boxes[0].w(), boxes[0].h()) * 2.5F;
                x = boxes[0].w() * compareOptions.wipeCenter.x;
                y = boxes[0].h() * compareOptions.wipeCenter.y;
            }
            const float rotation = compareOptions.wipeRotation;
            ftk::V2F pts[4];
            for (size_t i = 0; i < 4; ++i)
            {
                float rad = ftk::deg2rad(rotation + 90.F * i + 90.F);
                pts[i].x = cos(rad) * radius + x;
                pts[i].y = sin(rad) * radius + y;
            }

            ftk::gl::SetAndRestore stencilTest(GL_STENCIL_TEST, GL_TRUE);

            const ftk::Size2I renderSize = getRenderSize();
            const ftk::Box2I viewport = getViewport();
            glViewport(
                viewport.x(),
                renderSize.h - viewport.h() - viewport.y(),
                viewport.w(),
                viewport.h());
            glClear(GL_STENCIL_BUFFER_BIT);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            p.shaders["wipe"]->bind();
            p.shaders["wipe"]->setUniform("color", ftk::Color4F(1.F, 0.F, 0.F));
            {
                if (p.vbos["wipe"])
                {
                    ftk::TriMesh2F mesh;
                    mesh.v.push_back(pts[0]);
                    mesh.v.push_back(pts[1]);
                    mesh.v.push_back(pts[2]);
                    ftk::Triangle2 tri;
                    tri.v[0] = 1;
                    tri.v[1] = 3;
                    tri.v[2] = 2;
                    mesh.triangles.push_back(tri);
                    p.vbos["wipe"]->copy(convert(mesh, p.vbos["wipe"]->getType()));
                }
                if (p.vaos["wipe"])
                {
                    p.vaos["wipe"]->bind();
                    p.vaos["wipe"]->draw(GL_TRIANGLES, 0, p.vbos["wipe"]->getSize());
                }
            }
            glStencilFunc(GL_EQUAL, 1, 0xFF);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            if (!videoFrame.empty() && !boxes.empty())
            {
                _drawVideo(
                    videoFrame[0],
                    boxes[0],
                    !imageOptions.empty() ? std::make_shared<ftk::ImageOptions>(imageOptions[0]) : nullptr,
                    !displayOptions.empty() ? displayOptions[0] : DisplayOptions(),
                    colorBuffer);
            }

            glViewport(
                viewport.x(),
                renderSize.h - viewport.h() - viewport.y(),
                viewport.w(),
                viewport.h());
            glClear(GL_STENCIL_BUFFER_BIT);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            p.shaders["wipe"]->bind();
            p.shaders["wipe"]->setUniform("color", ftk::Color4F(0.F, 1.F, 0.F));
            {
                if (p.vbos["wipe"])
                {
                    ftk::TriMesh2F mesh;
                    mesh.v.push_back(pts[2]);
                    mesh.v.push_back(pts[3]);
                    mesh.v.push_back(pts[0]);
                    ftk::Triangle2 tri;
                    tri.v[0] = 1;
                    tri.v[1] = 3;
                    tri.v[2] = 2;
                    mesh.triangles.push_back(tri);
                    p.vbos["wipe"]->copy(convert(mesh, p.vbos["wipe"]->getType()));
                }
                if (p.vaos["wipe"])
                {
                    p.vaos["wipe"]->bind();
                    p.vaos["wipe"]->draw(GL_TRIANGLES, 0, p.vbos["wipe"]->getSize());
                }
            }
            glStencilFunc(GL_EQUAL, 1, 0xFF);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            if (videoFrame.size() > 1 && boxes.size() > 1)
            {
                _drawVideo(
                    videoFrame[1],
                    boxes[1],
                    imageOptions.size() > 1 ? std::make_shared<ftk::ImageOptions>(imageOptions[1]) : nullptr,
                    displayOptions.size() > 1 ? displayOptions[1] : DisplayOptions(),
                    colorBuffer);
            }
        }

        void Render::_drawVideoOverlay(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            const CompareOptions& compareOptions,
            ftk::gl::TextureType colorBuffer)
        {
            FTK_P();

            if (videoFrame.size() > 1 && boxes.size() > 1)
            {
                _drawVideo(
                    videoFrame[1],
                    boxes[1],
                    imageOptions.size() > 1 ? std::make_shared<ftk::ImageOptions>(imageOptions[1]) : nullptr,
                    displayOptions.size() > 1 ? displayOptions[1] : DisplayOptions(),
                    colorBuffer);
            }
            if (!videoFrame.empty() && !boxes.empty())
            {
                const ftk::Size2I offscreenBufferSize(
                    boxes[0].w(),
                    boxes[0].h());
                ftk::gl::OffscreenBufferOptions offscreenBufferOptions;
                if (!displayOptions.empty())
                {
                    offscreenBufferOptions.colorFilters = imageFilters(imageOptions, 0);
                }
                if (doCreate(
                    p.buffers["overlay"],
                    offscreenBufferSize,
                    colorBuffer,
                    offscreenBufferOptions))
                {
                    p.buffers["overlay"] = ftk::gl::OffscreenBuffer::create(
                        offscreenBufferSize,
                        colorBuffer,
                        offscreenBufferOptions);
                }

                if (p.buffers["overlay"])
                {
                    const ftk::gl::SetAndRestore scissorTest(GL_SCISSOR_TEST, GL_FALSE);

                    ftk::gl::OffscreenBufferBinding binding(p.buffers["overlay"]);
                    glViewport(
                        0,
                        0,
                        offscreenBufferSize.w,
                        offscreenBufferSize.h);
                    glClearColor(0.F, 0.F, 0.F, 0.F);
                    glClear(GL_COLOR_BUFFER_BIT);

                    p.shaders["display"]->bind();
                    p.shaders["display"]->setUniform(
                        "transform.mvp",
                        ftk::ortho(
                            0.F,
                            static_cast<float>(offscreenBufferSize.w),
                            static_cast<float>(offscreenBufferSize.h),
                            0.F,
                            -1.F,
                            1.F));

                    _drawVideo(
                        videoFrame[0],
                        ftk::Box2I(ftk::V2I(), offscreenBufferSize),
                        !imageOptions.empty() ? std::make_shared<ftk::ImageOptions>(imageOptions[0]) : nullptr,
                        !displayOptions.empty() ? displayOptions[0] : DisplayOptions(),
                        colorBuffer);

                    p.shaders["display"]->bind();
                    p.shaders["display"]->setUniform("transform.mvp", getTransform());
                }

                if (p.buffers["overlay"])
                {
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                    const ftk::Size2I renderSize = getRenderSize();
                    const ftk::Box2I viewport = getViewport();
                    glViewport(
                        viewport.x(),
                        renderSize.h - viewport.h() - viewport.y(),
                        viewport.w(),
                        viewport.h());

                    p.shaders["overlay"]->bind();
                    p.shaders["overlay"]->setUniform("color", ftk::Color4F(1.F, 1.F, 1.F, compareOptions.overlay));
                    p.shaders["overlay"]->setUniform("textureSampler", 0);

                    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0));
                    glBindTexture(GL_TEXTURE_2D, p.buffers["overlay"]->getColorID());

                    if (p.vbos["video"])
                    {
                        p.vbos["video"]->copy(convert(
                            ftk::mesh(boxes[0], true),
                            p.vbos["video"]->getType()));
                    }
                    if (p.vaos["video"])
                    {
                        p.vaos["video"]->bind();
                        p.vaos["video"]->draw(GL_TRIANGLES, 0, p.vbos["video"]->getSize());
                    }
                }
            }
        }

        bool Render::_drawVideoPair(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            ftk::gl::TextureType colorBuffer)
        {
            FTK_P();
            const ftk::Size2I offscreenBufferSize(boxes[0].w(), boxes[0].h());

            // Each file into a buffer of its own, drawn through the whole
            // display pipeline so that what is combined is what would have
            // been shown.
            for (size_t i = 0; i < 2; ++i)
            {
                const std::string name = ftk::Format("compare{0}").arg(i);
                if (i > 0 && videoFrame.size() <= i)
                {
                    p.buffers[name].reset();
                    continue;
                }
                ftk::gl::OffscreenBufferOptions offscreenBufferOptions;
                if (displayOptions.size() > i)
                {
                    offscreenBufferOptions.colorFilters = imageFilters(imageOptions, i);
                }
                if (doCreate(
                    p.buffers[name],
                    offscreenBufferSize,
                    colorBuffer,
                    offscreenBufferOptions))
                {
                    p.buffers[name] = ftk::gl::OffscreenBuffer::create(
                        offscreenBufferSize,
                        colorBuffer,
                        offscreenBufferOptions);
                }
                if (!p.buffers[name])
                {
                    continue;
                }

                const ftk::gl::SetAndRestore scissorTest(GL_SCISSOR_TEST, GL_FALSE);
                ftk::gl::OffscreenBufferBinding binding(p.buffers[name]);
                glViewport(
                    0,
                    0,
                    offscreenBufferSize.w,
                    offscreenBufferSize.h);
                glClearColor(0.F, 0.F, 0.F, 0.F);
                glClear(GL_COLOR_BUFFER_BIT);

                p.shaders["display"]->bind();
                p.shaders["display"]->setUniform(
                    "transform.mvp",
                    ftk::ortho(
                        0.F,
                        static_cast<float>(offscreenBufferSize.w),
                        static_cast<float>(offscreenBufferSize.h),
                        0.F,
                        -1.F,
                        1.F));

                _drawVideo(
                    videoFrame[i],
                    boxes[i],
                    imageOptions.size() > i ?
                        std::make_shared<ftk::ImageOptions>(imageOptions[i]) :
                        nullptr,
                    displayOptions.size() > i ?
                        displayOptions[i] :
                        DisplayOptions(),
                    colorBuffer);

                // Restored because the buffer above replaced it, and the
                // caller draws with the transform it came in with.
                p.shaders["display"]->bind();
                p.shaders["display"]->setUniform("transform.mvp", getTransform());
            }

            return p.buffers["compare0"] && p.buffers["compare1"];
        }

        void Render::_drawVideoPairShader(
            const std::string& shader,
            const ftk::Box2I& box)
        {
            FTK_P();
            glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

            const ftk::Size2I renderSize = getRenderSize();
            const ftk::Box2I viewport = getViewport();
            glViewport(
                viewport.x(),
                renderSize.h - viewport.h() - viewport.y(),
                viewport.w(),
                viewport.h());

            p.shaders[shader]->bind();
            p.shaders[shader]->setUniform("textureSampler", 0);
            p.shaders[shader]->setUniform("textureSamplerB", 1);

            glActiveTexture(static_cast<GLenum>(GL_TEXTURE0));
            glBindTexture(GL_TEXTURE_2D, p.buffers["compare0"]->getColorID());

            glActiveTexture(static_cast<GLenum>(GL_TEXTURE1));
            glBindTexture(GL_TEXTURE_2D, p.buffers["compare1"]->getColorID());

            if (p.vbos["video"])
            {
                p.vbos["video"]->copy(convert(
                    ftk::mesh(box, true),
                    p.vbos["video"]->getType()));
            }
            if (p.vaos["video"])
            {
                p.vaos["video"]->bind();
                p.vaos["video"]->draw(GL_TRIANGLES, 0, p.vbos["video"]->getSize());
            }
        }

        void Render::_drawVideoButterfly(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            const CompareOptions& compareOptions,
            ftk::gl::TextureType colorBuffer)
        {
            FTK_P();
            if (videoFrame.empty() || boxes.empty())
                return;
            if (_drawVideoPair(
                videoFrame, boxes, imageOptions, displayOptions, colorBuffer))
            {
                _drawVideoPairShader("butterfly", boxes[0]);
            }
        }

        void Render::_drawVideoDifference(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            const CompareOptions& compareOptions,
            ftk::gl::TextureType colorBuffer)
        {
            FTK_P();
            if (videoFrame.empty() || boxes.empty())
                return;
            if (_drawVideoPair(
                videoFrame, boxes, imageOptions, displayOptions, colorBuffer))
            {
                p.shaders["difference"]->bind();
                p.shaders["difference"]->setUniform(
                    "gain",
                    compareOptions.differenceGain);
                _drawVideoPairShader("difference", boxes[0]);
            }
        }

        void Render::_drawVideoTile(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            const CompareOptions& compareOptions,
            ftk::gl::TextureType colorBuffer)
        {
            for (size_t i = 0; i < videoFrame.size() && i < boxes.size(); ++i)
            {
                _drawVideo(
                    videoFrame[i],
                    boxes[i],
                    i < imageOptions.size() ? std::make_shared<ftk::ImageOptions>(imageOptions[i]) : nullptr,
                    i < displayOptions.size() ? displayOptions[i] : DisplayOptions(),
                    colorBuffer);
            }
        }
        
        namespace
        {
            float knee(float x, float f)
            {
                return logf(x * f + 1.F) / f;
            }

            float knee2(float x, float y)
            {
                float f0 = 0.F;
                float f1 = 1.F;
                while (knee(x, f1) > y)
                {
                    f0 = f1;
                    f1 = f1 * 2.F;
                }
                for (size_t i = 0; i < 30; ++i)
                {
                    const float f2 = (f0 + f1) / 2.F;
                    if (knee(x, f2) < y)
                    {
                        f1 = f2;
                    }
                    else
                    {
                        f0 = f2;
                    }
                }
                return (f0 + f1) / 2.F;
            }
        }

        void Render::_drawVideo(
            const VideoFrame& videoFrame,
            const ftk::Box2I& box,
            const std::shared_ptr<ftk::ImageOptions>& imageOptions,
            const DisplayOptions& displayOptions,
            ftk::gl::TextureType colorBuffer)
        {
            FTK_P();
            
            GLint viewportPrev[4] = { 0, 0, 0, 0 };
            glGetIntegerv(GL_VIEWPORT, viewportPrev);

            // Through setTransform() rather than by setting the uniform on
            // the image shader: anything the draws below call has no other way
            // to find out what space it is drawing in, and a resample that
            // believes it is drawing to the view rather than to this buffer
            // puts the picture in a corner of it.
            const ftk::M44F previousTransform = p.baseRender->getTransform();
            const auto transform = ftk::ortho(
                0.F,
                static_cast<float>(box.w()),
                static_cast<float>(box.h()),
                0.F,
                -1.F,
                1.F);
            p.baseRender->setTransform(transform);

            const ftk::Size2I& offscreenBufferSize = box.size();

            // The box a layer occupies within the offscreen buffer. Without
            // OTIO spatial coordinates a layer fills the buffer; with them
            // it keeps its place in the timeline canvas, scaled to the
            // buffer.
            const auto layerBox = [&videoFrame, &offscreenBufferSize](
                const std::optional<ftk::Box2F>& bounds)
            {
                ftk::Box2I out(ftk::V2I(), offscreenBufferSize);
                if (bounds.has_value() && videoFrame.canvasSize.isValid())
                {
                    const float sx = offscreenBufferSize.w /
                        static_cast<float>(videoFrame.canvasSize.w);
                    const float sy = offscreenBufferSize.h /
                        static_cast<float>(videoFrame.canvasSize.h);
                    out = ftk::Box2I(
                        ftk::V2I(
                            std::lround(bounds.value().min.x * sx),
                            std::lround(bounds.value().min.y * sy)),
                        ftk::V2I(
                            std::lround(bounds.value().max.x * sx),
                            std::lround(bounds.value().max.y * sy)));
                }
                return out;
            };

            ftk::gl::OffscreenBufferOptions offscreenBufferOptions;
            const ftk::ImageFilters filters = imageOptions.get() ?
                imageOptions->imageFilters :
                ftk::ImageFilters();
            offscreenBufferOptions.colorFilters = filters;
            if (doCreate(
                p.buffers["video"],
                offscreenBufferSize,
                colorBuffer,
                offscreenBufferOptions))
            {
                p.buffers["video"] = ftk::gl::OffscreenBuffer::create(
                    offscreenBufferSize,
                    colorBuffer,
                    offscreenBufferOptions);
            }

            if (p.buffers["video"])
            {
                const ftk::gl::SetAndRestore scissorTest(GL_SCISSOR_TEST, GL_FALSE);

                ftk::gl::OffscreenBufferBinding binding(p.buffers["video"]);
                glViewport(0, 0, offscreenBufferSize.w, offscreenBufferSize.h);
                glClearColor(0.F, 0.F, 0.F, 0.F);
                glClear(GL_COLOR_BUFFER_BIT);

                for (const auto& layer : videoFrame.layers)
                {
                    switch (layer.transition)
                    {
                        case Transition::Dissolve:
                        {
                            if (layer.image && layer.imageB)
                            {
                                if (doCreate(
                                    p.buffers["dissolve"],
                                    offscreenBufferSize,
                                    colorBuffer,
                                    offscreenBufferOptions))
                                {
                                    p.buffers["dissolve"] = ftk::gl::OffscreenBuffer::create(
                                        offscreenBufferSize,
                                        colorBuffer,
                                        offscreenBufferOptions);
                                }
                                if (doCreate(
                                    p.buffers["dissolve2"],
                                    offscreenBufferSize,
                                    colorBuffer,
                                    offscreenBufferOptions))
                                {
                                    p.buffers["dissolve2"] = ftk::gl::OffscreenBuffer::create(
                                        offscreenBufferSize,
                                        colorBuffer,
                                        offscreenBufferOptions);
                                }
                                if (p.buffers["dissolve"])
                                {
                                    ftk::gl::OffscreenBufferBinding binding(p.buffers["dissolve"]);
                                    glViewport(0, 0, offscreenBufferSize.w, offscreenBufferSize.h);
                                    glClearColor(0.F, 0.F, 0.F, 0.F);
                                    glClear(GL_COLOR_BUFFER_BIT);
                                    glDisable(GL_BLEND);
                                    auto imageOptionsTmp = imageOptions.get() ? *imageOptions : layer.imageOptions;
                                    imageOptionsTmp.cache = false;
                                    IRender::drawImage(
                                        layer.image,
                                        getBox(
                                            layerBox(layer.bounds),
                                            layer.image->getInfo(),
                                            displayOptions.aspectRatio),
                                        ftk::Color4F(1.F, 1.F, 1.F),
                                        imageOptionsTmp);
                                    glEnable(GL_BLEND);
                                }
                                if (p.buffers["dissolve2"])
                                {
                                    ftk::gl::OffscreenBufferBinding binding(p.buffers["dissolve2"]);
                                    glViewport(0, 0, offscreenBufferSize.w, offscreenBufferSize.h);
                                    glClearColor(0.F, 0.F, 0.F, 0.F);
                                    glClear(GL_COLOR_BUFFER_BIT);
                                    glDisable(GL_BLEND);
                                    auto imageOptionsTmp = imageOptions.get() ? *imageOptions : layer.imageOptionsB;
                                    imageOptionsTmp.cache = false;
                                    IRender::drawImage(
                                        layer.imageB,
                                        getBox(
                                            layerBox(layer.boundsB),
                                            layer.imageB->getInfo(),
                                            displayOptions.aspectRatio),
                                        ftk::Color4F(1.F, 1.F, 1.F),
                                        imageOptionsTmp);
                                    glEnable(GL_BLEND);
                                }
                                if (p.buffers["dissolve"] && p.buffers["dissolve2"])
                                {
                                    p.shaders["dissolve"]->bind();
                                    p.shaders["dissolve"]->setUniform("transform.mvp", transform);
                                    p.shaders["dissolve"]->setUniform("dissolve", layer.transitionValue);
                                    p.shaders["dissolve"]->setUniform("textureSampler", 0);
                                    p.shaders["dissolve"]->setUniform("textureSampler2", 1);

                                    ftk::gl::setAlphaBlend(ftk::AlphaBlend::Straight);

                                    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0));
                                    glBindTexture(GL_TEXTURE_2D, p.buffers["dissolve"]->getColorID());
                                    glActiveTexture(static_cast<GLenum>(GL_TEXTURE1));
                                    glBindTexture(GL_TEXTURE_2D, p.buffers["dissolve2"]->getColorID());
                                    if (p.vbos["video"])
                                    {
                                        p.vbos["video"]->copy(convert(
                                            ftk::mesh(ftk::Box2I(ftk::V2I(), offscreenBufferSize), true),
                                            p.vbos["video"]->getType()));
                                    }
                                    if (p.vaos["video"])
                                    {
                                        p.vaos["video"]->bind();
                                        p.vaos["video"]->draw(GL_TRIANGLES, 0, p.vbos["video"]->getSize());
                                    }
                                }
                            }
                            else if (layer.image)
                            {
                                auto imageOptionsTmp = imageOptions.get() ? *imageOptions : layer.imageOptions;
                                imageOptionsTmp.cache = false;
                                IRender::drawImage(
                                    layer.image,
                                    getBox(
                                        layerBox(layer.bounds),
                                        layer.image->getInfo(),
                                        displayOptions.aspectRatio),
                                    ftk::Color4F(1.F, 1.F, 1.F, 1.F - layer.transitionValue),
                                    imageOptionsTmp);
                            }
                            else if (layer.imageB)
                            {
                                auto imageOptionsTmp = imageOptions.get() ? *imageOptions : layer.imageOptionsB;
                                imageOptionsTmp.cache = false;
                                IRender::drawImage(
                                    layer.imageB,
                                    getBox(
                                        layerBox(layer.boundsB),
                                        layer.imageB->getInfo(),
                                        displayOptions.aspectRatio),
                                    ftk::Color4F(1.F, 1.F, 1.F, layer.transitionValue),
                                    imageOptionsTmp);
                            }
                            break;
                        }
                    default:
                        if (layer.image)
                        {
                            auto imageOptionsTmp = imageOptions.get() ? *imageOptions : layer.imageOptions;
                            imageOptionsTmp.cache = false;
                            IRender::drawImage(
                                layer.image,
                                getBox(
                                    layerBox(layer.bounds),
                                    layer.image->getInfo(),
                                    displayOptions.aspectRatio),
                                ftk::Color4F(1.F, 1.F, 1.F),
                                imageOptionsTmp);
                        }
                        break;
                    }
                }
            }

            p.baseRender->setTransform(previousTransform);

            unsigned int videoID = p.buffers["video"] ? p.buffers["video"]->getColorID() : 0;
            unsigned int videoScaledID = 0;
            ftk::Size2I scaledBufferSize;

#if !defined(FTK_API_GLES_2)
            // The picture has been drawn at its own size; the view's zoom is
            // applied by the draw below. When that is a large reduction the
            // four texels a linear fetch reads miss most of it, so reduce the
            // buffer first, with the filter the View tool asks for.
            //
            // Before the display transform rather than after: this way the
            // resample weighs the working values rather than display referred
            // ones, and the transform then runs over the smaller picture.
            if (p.buffers["video"] &&
                ftk::ImageFilter::HighQuality == filters.minify)
            {
                // What the box comes to on screen, which is the render
                // transform applied to its corners.
                const ftk::M44F& m = p.baseRender->getTransform();
                const ftk::Size2I renderSize = p.baseRender->getRenderSize();
                const auto toPixels = [&m, &renderSize](float x, float y)
                {
                    const ftk::V4F v = m * ftk::V4F(x, y, 0.F, 1.F);
                    return ftk::V2F(
                        (v.x + 1.F) * .5F * renderSize.w,
                        (v.y + 1.F) * .5F * renderSize.h);
                };
                const ftk::V2F a = toPixels(box.min.x, box.min.y);
                const ftk::V2F b = toPixels(box.max.x + 1, box.max.y + 1);
                const ftk::Size2I onScreen(
                    static_cast<int>(std::round(std::fabs(b.x - a.x))),
                    static_cast<int>(std::round(std::fabs(b.y - a.y))));
                if (onScreen.isValid() &&
                    (onScreen.w < offscreenBufferSize.w ||
                     onScreen.h < offscreenBufferSize.h))
                {
                    const ftk::Size2I scaledSize(
                        std::min(onScreen.w, offscreenBufferSize.w),
                        std::min(onScreen.h, offscreenBufferSize.h));
                    ftk::gl::OffscreenBufferOptions scaledOptions;
                    if (doCreate(p.buffers["videoScaled"], scaledSize, colorBuffer, scaledOptions))
                    {
                        p.buffers["videoScaled"] = ftk::gl::OffscreenBuffer::create(
                            scaledSize, colorBuffer, scaledOptions);
                    }
                    if (p.buffers["videoScaled"])
                    {
                        const ftk::gl::SetAndRestore scissorTest(GL_SCISSOR_TEST, GL_FALSE);
                        ftk::gl::OffscreenBufferBinding binding(p.buffers["videoScaled"]);
                        const ftk::M44F saved = p.baseRender->getTransform();
                        glViewport(0, 0, scaledSize.w, scaledSize.h);
                        glClearColor(0.F, 0.F, 0.F, 0.F);
                        glClear(GL_COLOR_BUFFER_BIT);
                        p.baseRender->setTransform(ftk::ortho(
                            0.F, static_cast<float>(scaledSize.w),
                            static_cast<float>(scaledSize.h), 0.F, -1.F, 1.F));
                        p.baseRender->drawTextureScaled(
                            videoID,
                            offscreenBufferSize,
                            ftk::Box2I(0, 0, scaledSize.w, scaledSize.h));
                        p.baseRender->setTransform(saved);
                        videoID = p.buffers["videoScaled"]->getColorID();
                        videoScaledID = videoID;
                        scaledBufferSize = scaledSize;
                    }
                }
            }

#endif // FTK_API_GLES_2

            if (p.buffers["video"])
            {
                glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                glViewport(
                    viewportPrev[0],
                    viewportPrev[1],
                    viewportPrev[2],
                    viewportPrev[3]);

                p.shaders["display"]->bind();
                p.shaders["display"]->setUniform("textureSampler", 0);
                // Enlarging is sampled here rather than resampled into a
                // buffer first, so this is where the view's magnify setting
                // is answered.
#if !defined(FTK_API_GLES_2)
                // The GLES 2 display shader has no kernel to answer this with;
                // see ftk's drawTextureScaled().
                const ftk::Size2I displaySize = videoID == videoScaledID ?
                    scaledBufferSize :
                    offscreenBufferSize;
                p.shaders["display"]->setUniform(
                    "magnifyHighQuality",
                    ftk::ImageFilter::HighQuality == filters.magnify);
                p.shaders["display"]->setUniform(
                    "textureSize",
                    ftk::V2F(displaySize.w, displaySize.h));
#endif // FTK_API_GLES_2
                p.shaders["display"]->setUniform("channels", static_cast<int>(displayOptions.channels));
                p.shaders["display"]->setUniform("negative", displayOptions.negative);
                p.shaders["display"]->setUniform("mirrorX", displayOptions.mirror.x);
                p.shaders["display"]->setUniform("mirrorY", displayOptions.mirror.y);
                const bool colorMatrixEnabled =
                    displayOptions.color != Color() &&
                    displayOptions.color.enabled;
                p.shaders["display"]->setUniform("colorEnabled", colorMatrixEnabled);
                p.shaders["display"]->setUniform("colorAdd", displayOptions.color.add);
                if (colorMatrixEnabled)
                {
                    p.shaders["display"]->setUniform("colorMatrix", color(displayOptions.color));
                }
                p.shaders["display"]->setUniform("levelsEnabled", displayOptions.levels.enabled);
                p.shaders["display"]->setUniform("levels.inLow", displayOptions.levels.inLow);
                p.shaders["display"]->setUniform("levels.inHigh", displayOptions.levels.inHigh);
                p.shaders["display"]->setUniform(
                    "levels.gamma",
                    displayOptions.levels.gamma > 0.F ? (1.F / displayOptions.levels.gamma) : 1000000.F);
                p.shaders["display"]->setUniform("levels.outLow", displayOptions.levels.outLow);
                p.shaders["display"]->setUniform("levels.outHigh", displayOptions.levels.outHigh);
                p.shaders["display"]->setUniform("exposureEnabled", displayOptions.exposure.enabled);
                if (displayOptions.exposure.enabled)
                {
                    const float v = powf(2.F, displayOptions.exposure.exposure + 2.47393F);
                    const float d = displayOptions.exposure.defog;
                    const float k = powf(2.F, displayOptions.exposure.kneeLow);
                    const float f = knee2(
                        powf(2.F, displayOptions.exposure.kneeHigh) - k,
                        powf(2.F, 3.5F) - k);
                    p.shaders["display"]->setUniform("exposure.v", v);
                    p.shaders["display"]->setUniform("exposure.d", d);
                    p.shaders["display"]->setUniform("exposure.k", k);
                    p.shaders["display"]->setUniform("exposure.f", f);
                    const float gamma =
                        displayOptions.exposure.gamma > 0.F ?
                        (1.F / displayOptions.exposure.gamma) :
                        1000000.F;
                    p.shaders["display"]->setUniform("exposure.g", gamma);
                }
                p.shaders["display"]->setUniform(
                    "softClip",
                    displayOptions.softClip.enabled ? displayOptions.softClip.value : 0.F);

                glActiveTexture(static_cast<GLenum>(GL_TEXTURE0));
                glBindTexture(GL_TEXTURE_2D, videoID);
                size_t texturesOffset = 1;
#if defined(TLRENDER_OCIO)
                if (p.ocioData)
                {
                    for (size_t i = 0; i < p.ocioData->textures.size(); ++i)
                    {
                        glActiveTexture(GL_TEXTURE0 + texturesOffset + i);
                        glBindTexture(
                            p.ocioData->textures[i].type,
                            p.ocioData->textures[i].id);
                    }
                    texturesOffset += p.ocioData->textures.size();
                }
                if (p.lutData)
                {
                    for (size_t i = 0; i < p.lutData->textures.size(); ++i)
                    {
                        glActiveTexture(GL_TEXTURE0 + texturesOffset + i);
                        glBindTexture(
                            p.lutData->textures[i].type,
                            p.lutData->textures[i].id);
                    }
                    texturesOffset += p.lutData->textures.size();
                }
#endif // TLRENDER_OCIO

                if (p.vbos["video"])
                {
                    p.vbos["video"]->copy(convert(
                        ftk::mesh(box, true),
                        p.vbos["video"]->getType()));
                }
                if (p.vaos["video"])
                {
                    p.vaos["video"]->bind();
                    p.vaos["video"]->draw(GL_TRIANGLES, 0, p.vbos["video"]->getSize());
                }
            }

            p.baseRender->setTransform(previousTransform);
        }

        void Render::drawForeground(
            const std::vector<ftk::Box2I>& boxes,
            const ftk::M44F& vm,
            const ForegroundOptions& options,
            const CompareOptions& compareOptions)
        {
            size_t start = 0;
            size_t end = boxes.size();
            switch (compareOptions.compare)
            {
                case Compare::None:
                case Compare::Wipe:
                case Compare::Butterfly:
                case Compare::Overlay:
                case Compare::Difference:
                    if (!boxes.empty())
                    {
                        end = 1;
                    }
                    break;
                case Compare::B:
                    if (boxes.size() > 1)
                    {
                        start = 1;
                        end = 2;
                    }
                    break;
                default: break;
            }
            for (size_t i = start; i < end; ++i)
            {
                const ftk::Box2I& box = boxes[i];
                const ftk::Box2I boxT = xform(box, vm);

                if (options.grid.enabled &&
                    GridCellMode::CellSize == options.grid.cellMode)
                {
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

                    const ftk::Size2I cellSizeT(
                        ftk::length(
                            vm * ftk::V3F(0.F, 0.F, 0.F) -
                            vm * ftk::V3F(options.grid.cellSize, 0.F, 0.F)),
                        ftk::length(
                            vm * ftk::V3F(0.F, 0.F, 0.F) -
                            vm * ftk::V3F(0.F, options.grid.cellSize, 0.F)));

                    if (cellSizeT.w > options.grid.lineWidth + 10.F &&
                        cellSizeT.h > options.grid.lineWidth + 10.F)
                    {
                        const ftk::Size2I& renderSize = getRenderSize();
                        ftk::M44F mi;
                        ftk::invert(vm, mi);
                        const ftk::V3F v0 = mi * ftk::V3F(0.F, 0.F, 0.F);
                        const ftk::V3F v1 = mi * ftk::V3F(renderSize.w, renderSize.h, 0.F);
                        const ftk::V2F v2(
                            std::max(static_cast<int>(v0.x) / options.grid.cellSize * options.grid.cellSize, box.min.x),
                            std::max(static_cast<int>(v0.y) / options.grid.cellSize * options.grid.cellSize, box.min.y));
                        const ftk::V2F v3(
                            std::min(static_cast<int>(v1.x) / options.grid.cellSize * options.grid.cellSize, box.max.x),
                            std::min(static_cast<int>(v1.y) / options.grid.cellSize * options.grid.cellSize, box.max.y));

                        if (options.grid.labels != GridLabels::None)
                        {
                            auto fontSystem = _fontSystem.lock();
                            const ftk::FontMetrics fontMetrics = fontSystem->getMetrics(options.grid.fontInfo);
                            std::string text = getLabel(
                                options.grid.labels,
                                GridLabels::Pixels == options.grid.labels ? v3.x : v3.x / options.grid.cellSize,
                                GridLabels::Pixels == options.grid.labels ? v3.y : v3.y / options.grid.cellSize);
                            ftk::Size2I size =
                                fontSystem->getSize(text, options.grid.fontInfo) +
                                options.grid.textMargin * 2;
                            if (size.w <= cellSizeT.w - options.grid.lineWidth &&
                                size.h <= cellSizeT.h - options.grid.lineWidth)
                            {
                                for (int y = v2.y, i = v2.y / options.grid.cellSize;
                                    y <= v3.y + 1;
                                    y += options.grid.cellSize, ++i)
                                {
                                    for (int x = v2.x, j = v2.x / options.grid.cellSize;
                                        x <= v3.x + 1;
                                        x += options.grid.cellSize, ++j)
                                    {
                                        text = getLabel(
                                            options.grid.labels,
                                            GridLabels::Pixels == options.grid.labels ? (x - box.min.x) : j,
                                            GridLabels::Pixels == options.grid.labels ? (y - box.min.y) : i);
                                        size =
                                            fontSystem->getSize(text, options.grid.fontInfo) +
                                            options.grid.textMargin * 2;
                                        const ftk::V3F v4 = vm * ftk::V3F(x, y, 0.F);
                                        const ftk::V2F v5(v4.x, v4.y);
                                        if (v5.x + options.grid.lineWidth / 2.F + size.w <= boxT.max.x &&
                                            v5.y + options.grid.lineWidth / 2.F + size.h <= boxT.max.y)
                                        {
                                            drawRect(
                                                ftk::Box2F(
                                                    v5.x + options.grid.lineWidth / 2.F,
                                                    v5.y + options.grid.lineWidth / 2.F,
                                                    size.w,
                                                    size.h),
                                                options.grid.overlayColor);
                                            drawText(
                                                fontSystem->getGlyphs(text, options.grid.fontInfo),
                                                fontMetrics,
                                                ftk::V2F(
                                                    v5.x + options.grid.lineWidth / 2.F + options.grid.textMargin,
                                                    v5.y + options.grid.lineWidth / 2.F + options.grid.textMargin),
                                                options.grid.textColor);
                                        }
                                    }
                                }
                            }
                        }

                        std::vector<ftk::Box2F> rects;
                        for (int y = v2.y, i = v2.y / options.grid.cellSize;
                            y <= v3.y + 1;
                            y += options.grid.cellSize, ++i)
                        {
                            const ftk::V3F v0 = vm * ftk::V3F(box.min.x, y, 0.F);
                            const ftk::V3F v1 = vm * ftk::V3F(box.max.x + 1, y, 0.F);
                            const ftk::V2I v2(
                                ftk::clamp(static_cast<int>(v0.x), boxT.min.x, boxT.max.x),
                                ftk::clamp(static_cast<int>(v0.y), boxT.min.y, boxT.max.y));
                            const ftk::V2I v3(
                                ftk::clamp(static_cast<int>(v1.x), boxT.min.x, boxT.max.x),
                                ftk::clamp(static_cast<int>(v1.y), boxT.min.y, boxT.max.y));
                            rects.push_back(ftk::Box2F(
                                v2.x,
                                v2.y - options.grid.lineWidth / 2,
                                v3.x - v2.x + 1,
                                options.grid.lineWidth));
                        }
                        for (int x = v2.x, j = v2.x / options.grid.cellSize;
                            x <= v3.x + 1;
                            x += options.grid.cellSize, ++j)
                        {
                            const ftk::V3F v0 = vm * ftk::V3F(x, box.min.y, 0.F);
                            const ftk::V3F v1 = vm * ftk::V3F(x, box.max.y + 1, 0.F);
                            const ftk::V2I v2(
                                ftk::clamp(static_cast<int>(v0.x), boxT.min.x, boxT.max.x),
                                ftk::clamp(static_cast<int>(v0.y), boxT.min.y, boxT.max.y));
                            const ftk::V2I v3(
                                ftk::clamp(static_cast<int>(v1.x), boxT.min.x, boxT.max.x),
                                ftk::clamp(static_cast<int>(v1.y), boxT.min.y, boxT.max.y));
                            rects.push_back(ftk::Box2F(
                                v2.x - options.grid.lineWidth / 2,
                                v2.y,
                                options.grid.lineWidth,
                                v3.y - v2.y + 1));
                        }
                        drawRects(rects, options.grid.color);
                    }
                }

                if (options.grid.enabled &&
                    GridCellMode::CellCount == options.grid.cellMode)
                {
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);

                    const ftk::V2I cellCount(
                        std::max(1, options.grid.cellCount.x),
                        std::max(1, options.grid.cellCount.y));

                    const ftk::Size2I cellSize(
                        box.w() / static_cast<float>(cellCount.x),
                        box.h() / static_cast<float>(cellCount.y));
                    const ftk::Size2I cellSizeT(
                        ftk::length(
                            vm * ftk::V3F(0.F, 0.F, 0.F) -
                            vm * ftk::V3F(cellSize.w, 0.F, 0.F)),
                        ftk::length(
                            vm * ftk::V3F(0.F, 0.F, 0.F) -
                            vm * ftk::V3F(0.F, cellSize.h, 0.F)));

                    if (cellSizeT.w > options.grid.lineWidth + 10.F &&
                        cellSizeT.h > options.grid.lineWidth + 10.F)
                    {
                        if (options.grid.labels != GridLabels::None)
                        {
                            auto fontSystem = _fontSystem.lock();
                            const ftk::FontMetrics fontMetrics = fontSystem->getMetrics(options.grid.fontInfo);
                            const ftk::V3F v1 = vm * ftk::V3F(box.w(), box.h(), 0.F);
                            std::string text = getLabel(
                                options.grid.labels,
                                GridLabels::Pixels == options.grid.labels ? v1.x : v1.x / cellSize.w,
                                GridLabels::Pixels == options.grid.labels ? v1.y : v1.y / cellSize.h);
                            ftk::Size2I size =
                                fontSystem->getSize(text, options.grid.fontInfo) +
                                options.grid.textMargin * 2;
                            if (size.w <= cellSizeT.w - options.grid.lineWidth &&
                                size.h <= cellSizeT.h - options.grid.lineWidth)
                            {
                                for (int i = 0; i < options.grid.cellCount.y; ++i)
                                {
                                    const int y = box.min.y + i / static_cast<float>(options.grid.cellCount.y) * box.h();
                                    for (int j = 0; j < options.grid.cellCount.x; ++j)
                                    {
                                        const int x = box.min.x + j / static_cast<float>(options.grid.cellCount.x) * box.w();
                                        text = getLabel(
                                            options.grid.labels,
                                            GridLabels::Pixels == options.grid.labels ? (x - box.min.x) : j,
                                            GridLabels::Pixels == options.grid.labels ? (y - box.min.y) : i);
                                        size =
                                            fontSystem->getSize(text, options.grid.fontInfo) +
                                            options.grid.textMargin * 2;
                                        const ftk::V3F v2 = vm * ftk::V3F(x, y, 0.F);
                                        const ftk::V2F v3(
                                            ftk::clamp(static_cast<int>(v2.x), boxT.min.x, boxT.max.x),
                                            ftk::clamp(static_cast<int>(v2.y), boxT.min.y, boxT.max.y));
                                        drawRect(
                                            ftk::Box2F(
                                                v3.x + options.grid.lineWidth / 2,
                                                v3.y + options.grid.lineWidth / 2,
                                                size.w,
                                                size.h),
                                            options.grid.overlayColor);
                                        drawText(
                                            fontSystem->getGlyphs(text, options.grid.fontInfo),
                                            fontMetrics,
                                            ftk::V2F(
                                                v3.x + options.grid.lineWidth / 2 + options.grid.textMargin,
                                                v3.y + options.grid.lineWidth / 2 + options.grid.textMargin),
                                            options.grid.textColor);
                                    }
                                }
                            }
                        }

                        std::vector<ftk::Box2F> rects;
                        for (int i = 0; i <= cellCount.y; ++i)
                        {
                            const float y = box.min.y + i / static_cast<float>(options.grid.cellCount.y) * box.h();
                            const ftk::V3F v0 = vm * ftk::V3F(box.min.x, y, 0.F);
                            const ftk::V3F v1 = vm * ftk::V3F(box.max.x + 1, y, 0.F);
                            const ftk::V2I v2(
                                ftk::clamp(static_cast<int>(v0.x), boxT.min.x, boxT.max.x),
                                ftk::clamp(static_cast<int>(v0.y), boxT.min.y, boxT.max.y));
                            const ftk::V2I v3(
                                ftk::clamp(static_cast<int>(v1.x), boxT.min.x, boxT.max.x),
                                ftk::clamp(static_cast<int>(v1.y), boxT.min.y, boxT.max.y));
                            rects.push_back(ftk::Box2F(
                                v2.x,
                                v2.y - options.grid.lineWidth / 2,
                                boxT.w(),
                                options.grid.lineWidth));
                        }
                        for (int i = 0; i <= cellCount.x; ++i)
                        {
                            const float x = box.min.x + i / static_cast<float>(options.grid.cellCount.x) * box.w();
                            const ftk::V3F v0 = vm * ftk::V3F(x, box.min.y, 0.F);
                            const ftk::V3F v1 = vm * ftk::V3F(x, box.max.y + 1, 0.F);
                            const ftk::V2I v2(
                                ftk::clamp(static_cast<int>(v0.x), boxT.min.x, boxT.max.x),
                                ftk::clamp(static_cast<int>(v0.y), boxT.min.y, boxT.max.y));
                            const ftk::V2I v3(
                                ftk::clamp(static_cast<int>(v1.x), boxT.min.x, boxT.max.x),
                                ftk::clamp(static_cast<int>(v1.y), boxT.min.y, boxT.max.y));
                            rects.push_back(ftk::Box2F(
                                v2.x - options.grid.lineWidth / 2,
                                v2.y,
                                options.grid.lineWidth,
                                boxT.h()));
                        }
                        drawRects(rects, options.grid.color);
                    }
                }

                if (options.centerMarker.enabled)
                {
                    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
                    std::vector<ftk::Box2F> centerMarker;
                    const ftk::V2F c(
                        box.x() + box.w() / 2.F,
                        box.y() + box.h() / 2.F);
                    const ftk::V3F v = vm * ftk::V3F(c.x, c.y, 0.F);
                    const float a = 1.F / 3.F;
                    const float b = 2.F / 3.F;
                    centerMarker.push_back(ftk::Box2F(
                        v.x - options.centerMarker.width / 2,
                        v.y - options.centerMarker.size,
                        options.centerMarker.width,
                        options.centerMarker.size * b));
                    centerMarker.push_back(ftk::Box2F(
                        v.x - options.centerMarker.width / 2,
                        v.y + options.centerMarker.size * a,
                        options.centerMarker.width,
                        options.centerMarker.size * b));
                    centerMarker.push_back(ftk::Box2F(
                        v.x - options.centerMarker.size,
                        v.y - options.centerMarker.width / 2,
                        options.centerMarker.size * b,
                        options.centerMarker.width));
                    centerMarker.push_back(ftk::Box2F(
                        v.x + options.centerMarker.size * a,
                        v.y - options.centerMarker.width / 2,
                        options.centerMarker.size * b,
                        options.centerMarker.width));
                    drawRects(centerMarker, options.centerMarker.color);
                }
            }
        }
    }
}

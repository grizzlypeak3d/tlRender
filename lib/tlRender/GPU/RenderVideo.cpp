// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GPU/RenderPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/Math.h>

#include <cmath>

namespace tl
{
    namespace gpu
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

        namespace
        {
            // Draws into a buffer for as long as it lives, and back into
            // what was being drawn into when it goes.
            class TargetScope
            {
            public:
                TargetScope(
                    const std::shared_ptr<ftk::gpu::Render>& render,
                    const std::shared_ptr<ftk::gpu::OffscreenBuffer>& buffer) :
                    _render(render)
                {
                    _render->pushTarget(buffer);
                }

                ~TargetScope()
                {
                    _render->popTarget();
                }

            private:
                std::shared_ptr<ftk::gpu::Render> _render;
            };

            ftk::Box2I bufferBox(const ftk::Size2I& size)
            {
                return ftk::Box2I(ftk::V2I(), size);
            }

            ftk::M44F bufferTransform(const ftk::Size2I& size)
            {
                return ftk::ortho(
                    0.F,
                    static_cast<float>(size.w),
                    static_cast<float>(size.h),
                    0.F,
                    -1.F,
                    1.F);
            }

            // The mesh a box is drawn with, cut down to the part of it on
            // one side of a line. The OpenGL renderer wipes with the
            // stencil buffer; a rectangle cut by a line needs none.
            ftk::TriMesh2F boxMesh(
                const ftk::Box2I& box,
                const ftk::V2F* a = nullptr,
                const ftk::V2F* b = nullptr,
                const ftk::V2F* c = nullptr)
            {
                if (!a || !b || !c)
                {
                    return ftk::mesh(box);
                }
                struct Vertex
                {
                    ftk::V2F p;
                    ftk::V2F t;
                };
                const std::vector<Vertex> polygon =
                {
                    { ftk::V2F(box.min.x, box.min.y), ftk::V2F(0.F, 0.F) },
                    { ftk::V2F(box.max.x + 1, box.min.y), ftk::V2F(1.F, 0.F) },
                    { ftk::V2F(box.max.x + 1, box.max.y + 1), ftk::V2F(1.F, 1.F) },
                    { ftk::V2F(box.min.x, box.max.y + 1), ftk::V2F(0.F, 1.F) }
                };
                const auto side = [a, b](const ftk::V2F& q)
                {
                    return (b->x - a->x) * (q.y - a->y) - (b->y - a->y) * (q.x - a->x);
                };
                const float sign = side(*c) >= 0.F ? 1.F : -1.F;
                std::vector<Vertex> clipped;
                for (size_t i = 0; i < polygon.size(); ++i)
                {
                    const Vertex& v0 = polygon[i];
                    const Vertex& v1 = polygon[(i + 1) % polygon.size()];
                    const float d0 = side(v0.p) * sign;
                    const float d1 = side(v1.p) * sign;
                    if (d0 >= 0.F)
                    {
                        clipped.push_back(v0);
                    }
                    if ((d0 >= 0.F) != (d1 >= 0.F))
                    {
                        const float t = d0 / (d0 - d1);
                        Vertex v;
                        v.p = v0.p + (v1.p - v0.p) * t;
                        v.t = v0.t + (v1.t - v0.t) * t;
                        clipped.push_back(v);
                    }
                }
                ftk::TriMesh2F out;
                for (const auto& v : clipped)
                {
                    out.v.push_back(v.p);
                    out.t.push_back(v.t);
                }
                // A fan, wound the way ftk::mesh() winds a box.
                for (size_t i = 1; i + 1 < clipped.size(); ++i)
                {
                    ftk::Triangle2 triangle;
                    triangle.v[0] = ftk::Vertex2(1, 1);
                    triangle.v[1] = ftk::Vertex2(i + 2, i + 2);
                    triangle.v[2] = ftk::Vertex2(i + 1, i + 1);
                    out.triangles.push_back(triangle);
                }
                return out;
            }
        }

        void Render::drawBackground(
            const std::vector<ftk::Box2I>& boxes,
            const ftk::M44F& vm,
            const BackgroundOptions& options,
            const CompareOptions& compareOptions)
        {

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

                for (size_t i = 0; i < boxes.size(); ++i)
                {
                    if (!isShown(compareOptions.compare, i))
                    {
                        continue;
                    }
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

        void Render::drawVideo(
            const std::vector<VideoFrame>& videoFrame,
            const std::vector<ftk::Box2I>& boxes,
            const std::vector<ftk::ImageOptions>& imageOptions,
            const std::vector<DisplayOptions>& displayOptions,
            const CompareOptions& compareOptions,
            ftk::gl::TextureType colorBuffer)
        {
            FTK_P();
            const auto draw = [&](size_t i, const std::optional<Clip>& clip = std::nullopt)
            {
                if (i < videoFrame.size() && i < boxes.size())
                {
                    _drawVideo(
                        videoFrame[i],
                        boxes[i],
                        i < imageOptions.size() ?
                            std::make_shared<ftk::ImageOptions>(imageOptions[i]) :
                            nullptr,
                        i < displayOptions.size() ? displayOptions[i] : DisplayOptions(),
                        colorBuffer,
                        getTransform(),
                        clip);
                }
            };
            const bool pair = videoFrame.size() > 1 && !boxes.empty();
            Compare compare = compareOptions.compare;
            if (!pair &&
                (Compare::Butterfly == compare || Compare::Difference == compare))
            {
                compare = Compare::None;
            }
            switch (compare)
            {
            case Compare::None:
                draw(0);
                break;
            case Compare::B:
                draw(1);
                break;
            case Compare::Wipe:
            {
                // The line the wipe is along, and a point either side of
                // it: the first picture is drawn on one side and the
                // second on the other.
                float radius = 0.F;
                float x = 0.F;
                float y = 0.F;
                if (!boxes.empty())
                {
                    radius = std::max(boxes[0].w(), boxes[0].h()) * 2.5F;
                    x = boxes[0].w() * compareOptions.wipeCenter.x;
                    y = boxes[0].h() * compareOptions.wipeCenter.y;
                }
                ftk::V2F pts[4];
                for (size_t i = 0; i < 4; ++i)
                {
                    const float rad = ftk::deg2rad(compareOptions.wipeRotation + 90.F * i + 90.F);
                    pts[i].x = std::cos(rad) * radius + x;
                    pts[i].y = std::sin(rad) * radius + y;
                }
                draw(0, Clip{ pts[0], pts[2], pts[1] });
                draw(1, Clip{ pts[0], pts[2], pts[3] });
                break;
            }
            case Compare::Overlay:
            {
                draw(1);
                if (!videoFrame.empty() && !boxes.empty())
                {
                    const ftk::Size2I size(boxes[0].w(), boxes[0].h());
                    if (auto buffer = _buffer("overlay", size, colorBuffer))
                    {
                        {
                            TargetScope scope(p.baseRender, buffer);
                            _drawVideo(
                                videoFrame[0],
                                bufferBox(size),
                                !imageOptions.empty() ?
                                    std::make_shared<ftk::ImageOptions>(imageOptions[0]) :
                                    nullptr,
                                !displayOptions.empty() ? displayOptions[0] : DisplayOptions(),
                                colorBuffer,
                                bufferTransform(size));
                        }
                        ColorUniforms uniforms;
                        uniforms.color[3] = compareOptions.overlay;
                        p.baseRender->drawShader(
                            "tl:overlay",
                            ftk::gpu::Blend::Straight,
                            boxMesh(boxes[0]),
                            getTransform(),
                            &uniforms,
                            sizeof(uniforms),
                            { { buffer->getTexture(), p.baseRender->getSampler(ftk::ImageFilter::Linear) } });
                    }
                }
                break;
            }
            case Compare::Butterfly:
            case Compare::Difference:
                if (_drawVideoPair(videoFrame, boxes, imageOptions, displayOptions, colorBuffer))
                {
                    DifferenceUniforms uniforms;
                    uniforms.gain = compareOptions.differenceGain;
                    SDL_GPUSampler* sampler = p.baseRender->getSampler(ftk::ImageFilter::Linear);
                    p.baseRender->drawShader(
                        Compare::Butterfly == compare ? "tl:butterfly" : "tl:difference",
                        ftk::gpu::Blend::PremultipliedAddAlpha,
                        boxMesh(boxes[0]),
                        getTransform(),
                        &uniforms,
                        sizeof(uniforms),
                        {
                            { p.buffers["compare0"]->getTexture(), sampler },
                            { p.buffers["compare1"]->getTexture(), sampler }
                        });
                }
                break;
            case Compare::Horizontal:
            case Compare::Vertical:
            case Compare::Tile:
                for (size_t i = 0; i < videoFrame.size() && i < boxes.size(); ++i)
                {
                    draw(i);
                }
                break;
            default: break;
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
            const ftk::Size2I size(boxes[0].w(), boxes[0].h());
            for (size_t i = 0; i < 2; ++i)
            {
                const std::string name = ftk::Format("compare{0}").arg(i);
                if (videoFrame.size() <= i || boxes.size() <= i)
                {
                    p.buffers[name].reset();
                    continue;
                }
                if (auto buffer = _buffer(name, size, colorBuffer))
                {
                    TargetScope scope(p.baseRender, buffer);
                    _drawVideo(
                        videoFrame[i],
                        boxes[i],
                        imageOptions.size() > i ?
                            std::make_shared<ftk::ImageOptions>(imageOptions[i]) :
                            nullptr,
                        displayOptions.size() > i ? displayOptions[i] : DisplayOptions(),
                        colorBuffer,
                        bufferTransform(size));
                }
            }
            return p.buffers["compare0"] && p.buffers["compare1"];
        }

        void Render::_drawVideo(
            const VideoFrame& videoFrame,
            const ftk::Box2I& box,
            const std::shared_ptr<ftk::ImageOptions>& imageOptions,
            const DisplayOptions& displayOptions,
            ftk::gl::TextureType colorBuffer,
            const ftk::M44F& mvp,
            const std::optional<Clip>& clip)
        {
            FTK_P();
            const ftk::Size2I offscreenBufferSize = box.size();
            if (!offscreenBufferSize.isValid())
            {
                return;
            }
            const ftk::M44F previousTransform = p.baseRender->getTransform();
            const ftk::M44F transform = bufferTransform(offscreenBufferSize);

            // The box a layer occupies within the offscreen buffer; see
            // tl::gl::Render.
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

            const ftk::ImageFilters filters = imageOptions.get() ?
                imageOptions->imageFilters :
                ftk::ImageFilters();

            // Whether the layers composite in linear: each one goes through
            // its own input color space ahead of the blend, and the display
            // transform picks up from the scene linear role; see
            // tl::gl::Render.
            bool perLayer = false;
            std::vector<std::pair<std::string, std::string> > layerInputs;
#if defined(TLRENDER_OCIO)
            if (p.ocioOptions.enabled &&
                !p.ocioOptions.display.empty() &&
                !p.ocioOptions.view.empty())
            {
                const std::string itemInput = !displayOptions.ocioInput.empty() ?
                    displayOptions.ocioInput :
                    p.ocioOptions.input;
                for (const auto& layer : videoFrame.layers)
                {
                    std::string in = _layerOCIOInput(layer.ocioInput, layer.path, layer.image);
                    if (in.empty())
                    {
                        in = itemInput;
                    }
                    std::string inB = _layerOCIOInput(layer.ocioInputB, layer.pathB, layer.imageB);
                    if (inB.empty())
                    {
                        inB = itemInput;
                    }
                    layerInputs.push_back(std::make_pair(in, inB));
                }
                std::string probe;
                for (const auto& i : layerInputs)
                {
                    if (!i.first.empty())
                    {
                        probe = i.first;
                        break;
                    }
                    if (!i.second.empty())
                    {
                        probe = i.second;
                        break;
                    }
                }
                if (!probe.empty())
                {
                    const auto data = _ocioData(probe);
                    perLayer = data && data->toLinear.shaderDesc;
                }
            }
#endif // TLRENDER_OCIO

            // Draw a layer image, through an input color space to scene
            // linear when one is given: the image is drawn into a buffer of
            // its own, and that is drawn through the transform into what
            // was being drawn into, blended as the image would have been.
            const auto drawLayerImage = [this, &offscreenBufferSize, &transform, colorBuffer](
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Box2I& box,
                const ftk::Color4F& color,
                const ftk::ImageOptions& imageOptions,
                const std::string& input,
                bool blend)
            {
                FTK_P();
                std::string shader;
                std::shared_ptr<ftk::gpu::OffscreenBuffer> layer;
#if defined(TLRENDER_OCIO)
                if (!input.empty())
                {
                    shader = _toLinearShader(input);
                }
#endif // TLRENDER_OCIO
                if (!shader.empty())
                {
                    layer = _buffer("layer", offscreenBufferSize, colorBuffer);
                }
                if (shader.empty() || !layer)
                {
                    p.baseRender->setBlendEnabled(blend);
                    p.baseRender->drawImage(image, box, color, imageOptions);
                    p.baseRender->setBlendEnabled(true);
                    return;
                }
                {
                    TargetScope scope(p.baseRender, layer);
                    p.baseRender->setBlendEnabled(false);
                    p.baseRender->drawImage(image, box, color, imageOptions);
                }
                p.baseRender->setBlendEnabled(blend);
                ftk::gpu::Blend copyBlend = ftk::gpu::Blend::Default;
                switch (imageOptions.alphaBlend)
                {
                case ftk::AlphaBlend::Straight: copyBlend = ftk::gpu::Blend::Straight; break;
                case ftk::AlphaBlend::Premultiplied: copyBlend = ftk::gpu::Blend::Premultiplied; break;
                default: break;
                }
                std::vector<ftk::gpu::TextureBinding> textures;
                textures.push_back({
                    layer->getTexture(),
                    p.baseRender->getSampler(ftk::ImageFilter::Nearest) });
                {
                    const auto& stageTextures = p.displayShaders[shader];
                    textures.insert(textures.end(), stageTextures.begin(), stageTextures.end());
                }
                ColorUniforms uniforms;
                p.baseRender->drawShader(
                    shader,
                    copyBlend,
                    boxMesh(bufferBox(offscreenBufferSize)),
                    transform,
                    &uniforms,
                    sizeof(uniforms),
                    textures);
                p.baseRender->setBlendEnabled(true);
            };

            // The layers, into a buffer the size of the picture.
            auto video = _buffer("video", offscreenBufferSize, colorBuffer);
            if (!video)
            {
                return;
            }
            {
                TargetScope scope(p.baseRender, video);
                p.baseRender->setTransform(transform);
                for (size_t layerIndex = 0; layerIndex < videoFrame.layers.size(); ++layerIndex)
                {
                    const auto& layer = videoFrame.layers[layerIndex];
                    const std::string layerInput =
                        perLayer && layerIndex < layerInputs.size() ?
                        layerInputs[layerIndex].first :
                        std::string();
                    const std::string layerInputB =
                        perLayer && layerIndex < layerInputs.size() ?
                        layerInputs[layerIndex].second :
                        std::string();
                    const auto options = [&imageOptions](const ftk::ImageOptions& layerOptions)
                    {
                        ftk::ImageOptions out = imageOptions.get() ? *imageOptions : layerOptions;
                        out.cache = false;
                        return out;
                    };
                    const auto drawA = [&](const ftk::Color4F& color, bool blend = true)
                    {
                        drawLayerImage(
                            layer.image,
                            getBox(
                                layerBox(layer.bounds),
                                layer.image->getInfo(),
                                displayOptions.aspectRatio),
                            color,
                            options(layer.imageOptions),
                            layerInput,
                            blend);
                    };
                    const auto drawB = [&](const ftk::Color4F& color, bool blend = true)
                    {
                        drawLayerImage(
                            layer.imageB,
                            getBox(
                                layerBox(layer.boundsB),
                                layer.imageB->getInfo(),
                                displayOptions.aspectRatio),
                            color,
                            options(layer.imageOptionsB),
                            layerInputB,
                            blend);
                    };
                    if (Transition::Dissolve == layer.transition)
                    {
                        if (layer.image && layer.imageB)
                        {
                            auto dissolve = _buffer("dissolve", offscreenBufferSize, colorBuffer);
                            auto dissolve2 = _buffer("dissolve2", offscreenBufferSize, colorBuffer);
                            if (dissolve && dissolve2)
                            {
                                {
                                    TargetScope scope(p.baseRender, dissolve);
                                    drawA(ftk::Color4F(1.F, 1.F, 1.F), false);
                                }
                                {
                                    TargetScope scope(p.baseRender, dissolve2);
                                    drawB(ftk::Color4F(1.F, 1.F, 1.F), false);
                                }
                                DissolveUniforms uniforms;
                                uniforms.dissolve = layer.transitionValue;
                                SDL_GPUSampler* sampler = p.baseRender->getSampler(ftk::ImageFilter::Linear);
                                p.baseRender->drawShader(
                                    "tl:dissolve",
                                    ftk::gpu::Blend::Straight,
                                    boxMesh(bufferBox(offscreenBufferSize)),
                                    transform,
                                    &uniforms,
                                    sizeof(uniforms),
                                    {
                                        { dissolve->getTexture(), sampler },
                                        { dissolve2->getTexture(), sampler }
                                    });
                            }
                        }
                        else if (layer.image)
                        {
                            drawA(ftk::Color4F(1.F, 1.F, 1.F, 1.F - layer.transitionValue));
                        }
                        else if (layer.imageB)
                        {
                            drawB(ftk::Color4F(1.F, 1.F, 1.F, layer.transitionValue));
                        }
                    }
                    else if (layer.image)
                    {
                        drawA(ftk::Color4F(1.F, 1.F, 1.F));
                    }
                }
            }
            p.baseRender->setTransform(previousTransform);

            // What the box comes to on screen, which is the render
            // transform applied to its corners.
            ftk::Size2I onScreen;
            {
                const ftk::M44F& m = previousTransform;
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
                onScreen = ftk::Size2I(
                    static_cast<int>(std::round(std::fabs(b.x - a.x))),
                    static_cast<int>(std::round(std::fabs(b.y - a.y))));
            }

            // The picture has been drawn at its own size. When the view
            // reduces it a long way the four texels a linear fetch reads
            // miss most of it, so the buffer is reduced first, with the
            // two pass resample, ahead of the display transform; see
            // tl::gl::Render.
            auto sampled = video;
            if (ftk::ImageFilter::HighQuality == filters.minify &&
                onScreen.isValid() &&
                (onScreen.w < offscreenBufferSize.w ||
                    onScreen.h < offscreenBufferSize.h))
            {
                const ftk::Size2I scaledSize(
                    std::min(onScreen.w, offscreenBufferSize.w),
                    std::min(onScreen.h, offscreenBufferSize.h));
                if (auto scaled = _buffer("videoScaled", scaledSize, colorBuffer))
                {
                    {
                        TargetScope scope(p.baseRender, scaled);
                        p.baseRender->setTransform(bufferTransform(scaledSize));
                        p.baseRender->drawTextureScaled(
                            video->getID(),
                            offscreenBufferSize,
                            bufferBox(scaledSize));
                    }
                    p.baseRender->setTransform(previousTransform);
                    sampled = scaled;
                }
            }
            const ftk::Size2I displaySize = sampled->getSize();

            // The buffer through the display shader, into what was being
            // drawn into.
            // When the layers were put together in linear the display picks
            // up from the scene linear role.
            const std::string shader = _displayShader(
                perLayer ? std::string("scene_linear") : displayOptions.ocioInput);
            DisplayUniforms uniforms;
            const bool magnify = ftk::ImageFilter::HighQuality == filters.magnify;
            uniforms.magnifyAxes[0] = magnify && onScreen.w > displaySize.w ? 1.F : 0.F;
            uniforms.magnifyAxes[1] = magnify && onScreen.h > displaySize.h ? 1.F : 0.F;
            uniforms.sourceSize[0] = displaySize.w;
            uniforms.sourceSize[1] = displaySize.h;
            uniforms.channels = static_cast<int32_t>(displayOptions.channels);
            uniforms.negative = displayOptions.negative;
            uniforms.mirrorX = displayOptions.mirror.x;
            uniforms.mirrorY = displayOptions.mirror.y;
            const bool colorMatrixEnabled =
                displayOptions.color != Color() &&
                displayOptions.color.enabled;
            uniforms.colorEnabled = colorMatrixEnabled;
            uniforms.colorAdd[0] = displayOptions.color.add.x;
            uniforms.colorAdd[1] = displayOptions.color.add.y;
            uniforms.colorAdd[2] = displayOptions.color.add.z;
            const ftk::M44F colorMatrix = colorMatrixEnabled ? color(displayOptions.color) : ftk::M44F();
            for (int row = 0; row < 4; ++row)
            {
                for (int column = 0; column < 4; ++column)
                {
                    uniforms.colorMatrix[column * 4 + row] = colorMatrix.get(row, column);
                }
            }
            uniforms.levelsEnabled = displayOptions.levels.enabled;
            uniforms.levelsInLow = displayOptions.levels.inLow;
            uniforms.levelsInHigh = displayOptions.levels.inHigh;
            uniforms.levelsGamma = displayOptions.levels.gamma > 0.F ?
                (1.F / displayOptions.levels.gamma) :
                1000000.F;
            uniforms.levelsOutLow = displayOptions.levels.outLow;
            uniforms.levelsOutHigh = displayOptions.levels.outHigh;
            uniforms.exposureEnabled = displayOptions.exposure.enabled;
            uniforms.exposure = displayOptions.exposure.enabled ?
                std::pow(2.F, displayOptions.exposure.exposure) :
                1.F;
            uniforms.softClip = displayOptions.softClip.enabled ? displayOptions.softClip.value : 0.F;

            // At the one to one size nothing needs filtering, so nothing
            // is; see tl::gl::Render.
            const bool nearest =
                onScreen == displaySize ||
                (ftk::ImageFilter::Nearest == filters.minify &&
                    ftk::ImageFilter::Nearest == filters.magnify);
            std::vector<ftk::gpu::TextureBinding> textures;
            textures.push_back({
                sampled->getTexture(),
                p.baseRender->getSampler(nearest ? ftk::ImageFilter::Nearest : ftk::ImageFilter::Linear) });
            {
                const auto& stageTextures = p.displayShaders[shader];
                textures.insert(textures.end(), stageTextures.begin(), stageTextures.end());
            }
            p.baseRender->drawShader(
                shader,
                ftk::gpu::Blend::Premultiplied,
                clip.has_value() ?
                    boxMesh(box, &clip->a, &clip->b, &clip->c) :
                    boxMesh(box),
                mvp,
                &uniforms,
                sizeof(uniforms),
                textures);
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

        void Render::drawClippingWarning(
            unsigned int id,
            const ftk::Box2I& rect,
            bool flipV,
            const std::vector<ftk::Box2I>& boxes,
            const ftk::M44F& vm,
            const ClippingWarning& options)
        {
            FTK_P();
            const ftk::Size2I size = rect.size();
            SDL_GPUTexture* texture = p.system->getTexture(id);
            if (!size.isValid() || !texture)
            {
                return;
            }
            ClippingWarningUniforms uniforms;
            uniforms.mode = static_cast<int32_t>(options.mode);
            uniforms.low = options.low;
            uniforms.high = options.high;
            for (const auto& box : boxes)
            {
                const ftk::Box2I boxT = ftk::intersect(
                    xform(box, vm),
                    ftk::Box2I(0, 0, size.w, size.h));
                if (boxT.w() <= 0 || boxT.h() <= 0)
                {
                    continue;
                }
                const float x0 = rect.min.x + boxT.min.x;
                const float y0 = rect.min.y + boxT.min.y;
                const float x1 = rect.min.x + boxT.max.x + 1;
                const float y1 = rect.min.y + boxT.max.y + 1;
                const float u0 = boxT.min.x / static_cast<float>(size.w);
                const float u1 = (boxT.max.x + 1) / static_cast<float>(size.w);
                float v0 = boxT.min.y / static_cast<float>(size.h);
                float v1 = (boxT.max.y + 1) / static_cast<float>(size.h);
                // The callers say whether to turn the texture over with
                // OpenGL's buffers in mind, whose first row is the bottom
                // one; here it is the top one.
                if (!flipV)
                {
                    v0 = 1.F - v0;
                    v1 = 1.F - v1;
                }
                ftk::TriMesh2F mesh;
                mesh.v = { ftk::V2F(x0, y0), ftk::V2F(x1, y0), ftk::V2F(x1, y1), ftk::V2F(x0, y1) };
                mesh.t = { ftk::V2F(u0, v0), ftk::V2F(u1, v0), ftk::V2F(u1, v1), ftk::V2F(u0, v1) };
                mesh.triangles.push_back({ ftk::Vertex2(1, 1), ftk::Vertex2(3, 3), ftk::Vertex2(2, 2) });
                mesh.triangles.push_back({ ftk::Vertex2(3, 3), ftk::Vertex2(1, 1), ftk::Vertex2(4, 4) });
                p.baseRender->drawShader(
                    "tl:clippingWarning",
                    ftk::gpu::Blend::Straight,
                    mesh,
                    getTransform(),
                    &uniforms,
                    sizeof(uniforms),
                    { { texture, p.baseRender->getSampler(ftk::ImageFilter::Linear) } });
            }
        }
    }
}

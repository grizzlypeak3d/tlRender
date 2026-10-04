// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GPU/RenderPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <stdexcept>

namespace tl
{
    namespace gpu
    {
#if defined(TLRENDER_OCIO)
        OCIOStage::~OCIOStage()
        {
            if (device)
            {
                for (const auto& i : textures)
                {
                    SDL_ReleaseGPUTexture(device, i.texture);
                    SDL_ReleaseGPUSampler(device, i.sampler);
                }
            }
        }

        namespace
        {
            SDL_GPUSampler* createSampler(SDL_GPUDevice* device, OCIO::Interpolation interpolation)
            {
                SDL_GPUSamplerCreateInfo info = {};
                const SDL_GPUFilter filter = OCIO::INTERP_NEAREST == interpolation ?
                    SDL_GPU_FILTER_NEAREST :
                    SDL_GPU_FILTER_LINEAR;
                info.min_filter = filter;
                info.mag_filter = filter;
                info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                return SDL_CreateGPUSampler(device, &info);
            }

            // A texture of floats, sent in a command buffer of its own.
            // There are no three channel textures, so the RGB tables are
            // given a fourth channel.
            SDL_GPUTexture* createTexture(
                SDL_GPUDevice* device,
                SDL_GPUTextureType type,
                uint32_t w,
                uint32_t h,
                uint32_t d,
                const float* values,
                bool rgb)
            {
                SDL_GPUTextureCreateInfo info = {};
                info.type = type;
                info.format = rgb ?
                    SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT :
                    SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
                info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
                info.width = w;
                info.height = h;
                info.layer_count_or_depth = d;
                info.num_levels = 1;
                SDL_GPUTexture* out = SDL_CreateGPUTexture(device, &info);
                if (!out)
                {
                    throw std::runtime_error(ftk::Format("Cannot create a texture: {0}").arg(SDL_GetError()));
                }
                const size_t count = static_cast<size_t>(w) * h * d;
                const size_t byteCount = count * (rgb ? 4 : 1) * sizeof(float);
                SDL_GPUTransferBufferCreateInfo transferInfo = {};
                transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                transferInfo.size = static_cast<Uint32>(byteCount);
                SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
                float* p = static_cast<float*>(SDL_MapGPUTransferBuffer(device, transfer, false));
                if (rgb)
                {
                    for (size_t i = 0; i < count; ++i)
                    {
                        p[i * 4 + 0] = values[i * 3 + 0];
                        p[i * 4 + 1] = values[i * 3 + 1];
                        p[i * 4 + 2] = values[i * 3 + 2];
                        p[i * 4 + 3] = 1.F;
                    }
                }
                else
                {
                    std::memcpy(p, values, byteCount);
                }
                SDL_UnmapGPUTransferBuffer(device, transfer);
                SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
                SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmd);
                SDL_GPUTextureTransferInfo source = {};
                source.transfer_buffer = transfer;
                SDL_GPUTextureRegion region = {};
                region.texture = out;
                region.w = w;
                region.h = h;
                region.d = d;
                SDL_UploadToGPUTexture(pass, &source, &region, false);
                SDL_EndGPUCopyPass(pass);
                SDL_SubmitGPUCommandBuffer(cmd);
                SDL_ReleaseGPUTransferBuffer(device, transfer);
                return out;
            }

            // Compile a processor for the GPU: the shader function under
            // the given names, and the textures it samples. The device is
            // the stage's.
            void ocioStageInit(
                OCIOStage& stage,
                const char* functionName,
                const char* resourcePrefix)
            {
                stage.functionName = functionName;
                stage.resourcePrefix = resourcePrefix;
                stage.gpuProcessor = stage.processor->getDefaultGPUProcessor();
                if (!stage.gpuProcessor)
                {
                    throw std::runtime_error("Cannot get OCIO GPU processor");
                }
                stage.shaderDesc = OCIO::GpuShaderDesc::CreateShaderDesc();
                if (!stage.shaderDesc)
                {
                    throw std::runtime_error("Cannot create OCIO shader description");
                }
                // The textures are the same whichever language the shader
                // is written in; the text is asked for again, in the
                // language wanted, when a shader is built with it.
                stage.shaderDesc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_VK_4_6);
                stage.shaderDesc->setFunctionName(functionName);
                stage.shaderDesc->setResourcePrefix(resourcePrefix);
                // There are no one dimensional textures: a table is a
                // texture one texel high.
                stage.shaderDesc->setAllowTexture1D(false);
                stage.gpuProcessor->extractGpuShaderInfo(stage.shaderDesc);

                const unsigned num3DTextures = stage.shaderDesc->getNum3DTextures();
                for (unsigned i = 0; i < num3DTextures; ++i)
                {
                    const char* textureName = nullptr;
                    const char* samplerName = nullptr;
                    unsigned edgelen = 0;
                    OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                    stage.shaderDesc->get3DTexture(i, textureName, samplerName, edgelen, interpolation);
                    const float* values = nullptr;
                    stage.shaderDesc->get3DTextureValues(i, values);
                    if (!textureName || !*textureName || !samplerName || !*samplerName || 0 == edgelen || !values)
                    {
                        throw std::runtime_error("The OCIO texture data is corrupted");
                    }
                    OCIOTexture texture;
                    texture.name = textureName;
                    texture.samplerName = samplerName;
                    texture.texture = createTexture(
                        stage.device, SDL_GPU_TEXTURETYPE_3D, edgelen, edgelen, edgelen, values, true);
                    texture.sampler = createSampler(stage.device, interpolation);
                    stage.textures.push_back(texture);
                }

                const unsigned numTextures = stage.shaderDesc->getNumTextures();
                for (unsigned i = 0; i < numTextures; ++i)
                {
                    const char* textureName = nullptr;
                    const char* samplerName = nullptr;
                    unsigned width = 0;
                    unsigned height = 0;
                    OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
                    OCIO::GpuShaderDesc::TextureDimensions dimensions = OCIO::GpuShaderDesc::TEXTURE_2D;
                    OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                    stage.shaderDesc->getTexture(
                        i, textureName, samplerName, width, height, channel, dimensions, interpolation);
                    const float* values = nullptr;
                    stage.shaderDesc->getTextureValues(i, values);
                    if (!textureName || !*textureName || !samplerName || !*samplerName || 0 == width || !values)
                    {
                        throw std::runtime_error("The OCIO texture data is corrupted");
                    }
                    OCIOTexture texture;
                    texture.name = textureName;
                    texture.samplerName = samplerName;
                    texture.texture = createTexture(
                        stage.device,
                        SDL_GPU_TEXTURETYPE_2D,
                        width,
                        std::max(height, 1U),
                        1,
                        values,
                        OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL == channel);
                    texture.sampler = createSampler(stage.device, interpolation);
                    stage.textures.push_back(texture);
                }
            }

            // The stage's shader text in a language, with its textures
            // numbered from a slot.
            OCIO::GpuShaderDescRcPtr shaderDesc(
                const OCIOStage& stage,
                OCIO::GpuLanguage language,
                size_t slot)
            {
                auto out = OCIO::GpuShaderDesc::CreateShaderDesc();
                out->setLanguage(language);
                out->setFunctionName(stage.functionName.c_str());
                out->setResourcePrefix(stage.resourcePrefix.c_str());
                out->setAllowTexture1D(false);
                if (OCIO::GPU_LANGUAGE_GLSL_VK_4_6 == language)
                {
                    // The set a fragment stage's samplers are in, and the
                    // first of this stage's bindings.
                    out->setDescriptorSetIndex(2, static_cast<unsigned>(slot));
                }
                stage.gpuProcessor->extractGpuShaderInfo(out);
                return out;
            }

            // What a stage adds to a shader, with its textures starting at
            // a slot. Both languages are written where the GLSL is wanted
            // or is being checked; the textures are put in the order of
            // the language the device takes.
            ShaderStage shaderStage(const OCIOStage& stage, size_t slot)
            {
                ShaderStage out;
                if (!stage.shaderDesc)
                {
                    return out;
                }
                const bool msl = SDL_GetGPUShaderFormats(stage.device) & SDL_GPU_SHADERFORMAT_MSL;
                const auto find = [&stage](const std::string& name, bool sampler)
                {
                    size_t index = stage.textures.size();
                    for (size_t k = 0; k < stage.textures.size(); ++k)
                    {
                        if ((sampler ? stage.textures[k].samplerName : stage.textures[k].name) == name)
                        {
                            index = k;
                            break;
                        }
                    }
                    if (index >= stage.textures.size())
                    {
                        throw std::runtime_error(
                            ftk::Format("The OCIO shader wants \"{0}\", which is not a texture").
                            arg(name));
                    }
                    return index;
                };

                if (msl)
                {
                    // The function OCIO writes takes its textures and
                    // samplers as arguments, so the entry point is given
                    // them, each at the slot its texture is bound to, and
                    // passes them on.
                    const auto desc = shaderDesc(stage, OCIO::GPU_LANGUAGE_MSL_2_0, slot);
                    out.mslDef = desc->getShaderText();
                    const std::string begin = "float4 " + stage.functionName + "(";
                    const size_t i = out.mslDef.rfind(begin);
                    const size_t j = std::string::npos == i ? i : out.mslDef.find(')', i);
                    if (std::string::npos == i || std::string::npos == j)
                    {
                        throw std::runtime_error("Cannot find the OCIO shader function");
                    }
                    std::string params = out.mslDef.substr(i + begin.size(), j - (i + begin.size()));
                    std::replace(params.begin(), params.end(), ',', ' ');
                    std::stringstream ss(params);
                    std::string type;
                    std::string name;
                    std::string call;
                    while (ss >> type >> name)
                    {
                        if ("inPixel" == name)
                        {
                            continue;
                        }
                        const bool sampler = "sampler" == type;
                        out.mslArgs += ftk::Format(",\n    {0} {1} [[{2}({3})]]").
                            arg(type).
                            arg(name).
                            arg(sampler ? "sampler" : "texture").
                            arg(slot + find(name, sampler));
                        call += name + ", ";
                    }
                    out.mslCall = "outColor = " + stage.functionName + "(" + call + "outColor);";
                    for (const auto& i : stage.textures)
                    {
                        out.textures.push_back({ i.texture, i.sampler });
                    }
                }
                if (!msl || (ftk::gpu::validateGLSL() && ftk::gpu::hasGLSLCompiler()))
                {
                    // The GLSL declares its samplers itself, at the
                    // bindings the description says it gave them.
                    const auto desc = shaderDesc(stage, OCIO::GPU_LANGUAGE_GLSL_VK_4_6, slot);
                    out.glslDef = desc->getShaderText();
                    out.glslCall = "outColor = " + stage.functionName + "(outColor);";
                    if (!msl)
                    {
                        out.textures.resize(stage.textures.size());
                        const auto place = [&](const char* samplerName, unsigned binding)
                        {
                            const size_t index = find(samplerName, true);
                            if (binding < slot || binding - slot >= out.textures.size())
                            {
                                throw std::runtime_error("The OCIO shader binds a texture out of range");
                            }
                            out.textures[binding - slot] =
                                { stage.textures[index].texture, stage.textures[index].sampler };
                        };
                        for (unsigned i = 0; i < desc->getNum3DTextures(); ++i)
                        {
                            const char* textureName = nullptr;
                            const char* samplerName = nullptr;
                            unsigned edgelen = 0;
                            OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                            desc->get3DTexture(i, textureName, samplerName, edgelen, interpolation);
                            place(samplerName, desc->get3DTextureShaderBindingIndex(i));
                        }
                        for (unsigned i = 0; i < desc->getNumTextures(); ++i)
                        {
                            const char* textureName = nullptr;
                            const char* samplerName = nullptr;
                            unsigned width = 0;
                            unsigned height = 0;
                            OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
                            OCIO::GpuShaderDesc::TextureDimensions dimensions = OCIO::GpuShaderDesc::TEXTURE_2D;
                            OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                            desc->getTexture(
                                i, textureName, samplerName, width, height, channel, dimensions, interpolation);
                            place(samplerName, desc->getTextureShaderBindingIndex(i));
                        }
                    }
                }
                return out;
            }

            // Build the two stages from the configuration's processors.
            void ocioDataInit(
                OCIOData& data,
                const OCIOOptions& options,
                SDL_GPUDevice* device)
            {
                data.toLinear.device = device;
                data.display.device = device;
                // The processors are the same whatever draws with them; see
                // getOCIOProcessors(). What is made of them here is a
                // shader function and its textures.
                const OCIOProcessors processors = getOCIOProcessors(options);
                data.config = processors.config;
                data.transform = processors.transform;
                data.lvp = processors.lvp;
                if (processors.toLinear)
                {
                    data.toLinear.processor = processors.toLinear;
                    ocioStageInit(
                        data.toLinear,
                        "ocioToLinearFunc",
                        "ocioToLinear");
                }
                data.display.processor = processors.display;
                ocioStageInit(data.display, "ocioFunc", "ocio");
            }
        }
#endif // TLRENDER_OCIO

        void Render::_init(
            const std::shared_ptr<ftk::gpu::System>& system,
            const std::shared_ptr<ftk::LogSystem>& logSystem,
            const std::shared_ptr<ftk::FontSystem>& fontSystem)
        {
            IRender::_init(logSystem, fontSystem);
            FTK_P();
            p.system = system;
            p.baseRender = ftk::gpu::Render::create(system, logSystem, fontSystem);
            p.baseRender->setShader("tl:overlay", textureFragmentSource(), 1);
            p.baseRender->setShader("tl:dissolve", dissolveFragmentSource(), 2);
            p.baseRender->setShader("tl:clippingWarning", clippingWarningFragmentSource(), 1);
            p.baseRender->setShader("tl:butterfly", butterflyFragmentSource(), 2);
            p.baseRender->setShader("tl:difference", differenceFragmentSource(), 2);
            p.baseRender->setShader("tl:hdr", hdrFragmentSource(), 1);
#if defined(TLRENDER_OCIO)
            p.ocioKey = getOCIOOptionsKey(p.ocioOptions);
            p.ocioKeys.push_front(p.ocioKey);
#endif // TLRENDER_OCIO
        }

        Render::Render() :
            _p(new Private)
        {}

        Render::~Render()
        {}

        std::shared_ptr<Render> Render::create(
            const std::shared_ptr<ftk::gpu::System>& system,
            const std::shared_ptr<ftk::LogSystem>& logSystem,
            const std::shared_ptr<ftk::FontSystem>& fontSystem)
        {
            auto out = std::shared_ptr<Render>(new Render);
            out->_init(system, logSystem, fontSystem);
            return out;
        }

        const std::shared_ptr<ftk::gpu::Render>& Render::getBaseRender() const
        {
            return _p->baseRender;
        }

        std::shared_ptr<ftk::gpu::Render> Render::getGPURender()
        {
            return _p->baseRender;
        }

        void Render::begin(
            const ftk::Size2I& renderSize,
            const ftk::RenderOptions& renderOptions)
        {
            _p->baseRender->begin(renderSize, renderOptions);
        }

        void Render::end()
        {
            _p->baseRender->end();
        }

        void Render::setOCIOOptions(const OCIOOptions& value)
        {
            FTK_P();
            if (value == p.ocioOptions)
                return;

            p.ocioOptions = value;

#if defined(TLRENDER_OCIO)
            p.ocioDataBound.reset();

            p.ocioKey = getOCIOOptionsKey(p.ocioOptions);
            auto i = std::find(p.ocioKeys.begin(), p.ocioKeys.end(), p.ocioKey);
            if (i != p.ocioKeys.end())
            {
                p.ocioKeys.erase(i);
            }
            p.ocioKeys.push_front(p.ocioKey);
            while (p.ocioKeys.size() > 4)
            {
                _ocioErase(p.ocioKeys.back());
                p.ocioKeys.pop_back();
            }

            if (p.ocioOptions.enabled &&
                !p.ocioOptions.input.empty() &&
                !p.ocioOptions.display.empty() &&
                !p.ocioOptions.view.empty())
            {
                _ocioData(p.ocioOptions.input);
            }
#endif // TLRENDER_OCIO
        }

        void Render::setOCIOInputResolver(
            const std::function<std::string(
                const std::string& path,
                const ftk::ImageTags&)>& value)
        {
            FTK_P();
#if defined(TLRENDER_OCIO)
            p.ocioInputResolver = value;
            p.ocioInputCache.clear();
#endif // TLRENDER_OCIO
        }

        void Render::setLUTOptions(const LUTOptions& value)
        {
            FTK_P();
            if (value == p.lutOptions)
                return;

#if defined(TLRENDER_OCIO)
            p.lutData.reset();
#endif // TLRENDER_OCIO

            p.lutOptions = value;

#if defined(TLRENDER_OCIO)
            // A LUT that cannot be read leaves the picture as it is; see
            // tl::gl::Render.
            if (p.lutOptions.enabled && !p.lutOptions.fileName.empty())
            {
                try
                {
                    auto lutData = std::make_unique<OCIOLUTData>();
                    const OCIOLUTProcessor processor = getOCIOLUTProcessor(p.lutOptions);
                    lutData->config = processor.config;
                    lutData->transform = processor.transform;
                    lutData->stage.device = p.system->getDevice();
                    lutData->stage.processor = processor.processor;
                    ocioStageInit(lutData->stage, "lutFunc", "lut");
                    p.lutData = std::move(lutData);
                }
                catch (const std::exception& e)
                {
                    if (auto logSystem = _logSystem.lock())
                    {
                        logSystem->print(
                            "tl::gpu::Render",
                            ftk::Format("Cannot use the LUT \"{0}\": {1}").
                                arg(p.lutOptions.fileName).
                                arg(e.what()),
                            ftk::LogType::Error);
                    }
                }
            }
#endif // TLRENDER_OCIO

            _displayShadersReset();
        }

        ftk::Size2I Render::getRenderSize() const
        {
            return _p->baseRender->getRenderSize();
        }

        void Render::setRenderSize(const ftk::Size2I& value)
        {
            _p->baseRender->setRenderSize(value);
        }

        ftk::RenderOptions Render::getRenderOptions() const
        {
            return _p->baseRender->getRenderOptions();
        }

        ftk::Box2I Render::getViewport() const
        {
            return _p->baseRender->getViewport();
        }

        void Render::setViewport(const ftk::Box2I& value)
        {
            _p->baseRender->setViewport(value);
        }

        void Render::clearViewport(const ftk::Color4F& value)
        {
            _p->baseRender->clearViewport(value);
        }

        bool Render::getClipRectEnabled() const
        {
            return _p->baseRender->getClipRectEnabled();
        }

        void Render::setClipRectEnabled(bool value)
        {
            _p->baseRender->setClipRectEnabled(value);
        }

        ftk::Box2I Render::getClipRect() const
        {
            return _p->baseRender->getClipRect();
        }

        void Render::setClipRect(const ftk::Box2I& value)
        {
            _p->baseRender->setClipRect(value);
        }

        ftk::M44F Render::getTransform() const
        {
            return _p->baseRender->getTransform();
        }

        void Render::setTransform(const ftk::M44F& value)
        {
            _p->baseRender->setTransform(value);
        }

        ftk::RenderDiag Render::getDiag() const
        {
            return _p->baseRender->getDiag();
        }

        void Render::drawRect(const ftk::Box2F& rect, const ftk::Color4F& color)
        {
            _p->baseRender->drawRect(rect, color);
        }

        void Render::drawRects(const std::vector<ftk::Box2F>& rects, const ftk::Color4F& color)
        {
            _p->baseRender->drawRects(rects, color);
        }

        void Render::drawLine(
            const ftk::V2F& v0,
            const ftk::V2F& v1,
            const ftk::Color4F& color,
            const ftk::LineOptions& options)
        {
            _p->baseRender->drawLine(v0, v1, color, options);
        }

        void Render::drawLines(
            const std::vector<std::pair<ftk::V2F, ftk::V2F> >& lines,
            const ftk::Color4F& color,
            const ftk::LineOptions& options)
        {
            _p->baseRender->drawLines(lines, color, options);
        }

        void Render::drawMesh(
            const ftk::TriMesh2F& mesh,
            const ftk::Color4F& color,
            const ftk::V2F& pos)
        {
            _p->baseRender->drawMesh(mesh, color, pos);
        }

        void Render::drawColorMesh(
            const ftk::TriMesh2F& mesh,
            const ftk::Color4F& color,
            const ftk::V2F& pos)
        {
            _p->baseRender->drawColorMesh(mesh, color, pos);
        }

        void Render::drawTexture(
            unsigned int id,
            const ftk::Box2I& rect,
            bool flipV,
            const ftk::Color4F& color,
            ftk::AlphaBlend alphaBlend)
        {
            _p->baseRender->drawTexture(id, rect, flipV, color, alphaBlend);
        }

        void Render::drawText(
            const std::vector<std::shared_ptr<ftk::Glyph> >& glyphs,
            const ftk::FontMetrics& fontMetrics,
            const ftk::V2F& position,
            const ftk::Color4F& color)
        {
            _p->baseRender->drawText(glyphs, fontMetrics, position, color);
        }

        void Render::drawImage(
            const std::shared_ptr<ftk::Image>& image,
            const ftk::TriMesh2F& mesh,
            const ftk::Color4F& color,
            const ftk::ImageOptions& imageOptions)
        {
            _p->baseRender->drawImage(image, mesh, color, imageOptions);
        }

        void Render::drawImage(
            const std::shared_ptr<ftk::Image>& image,
            const ftk::Box2F& box,
            const ftk::Color4F& color,
            const ftk::ImageOptions& imageOptions)
        {
            _p->baseRender->drawImage(image, box, color, imageOptions);
        }

        std::string Render::_displayShader(const std::string& ocioInput)
        {
            FTK_P();
            std::string input;
#if defined(TLRENDER_OCIO)
            std::shared_ptr<OCIOData> ocioData;
            if (p.ocioOptions.enabled &&
                !p.ocioOptions.display.empty() &&
                !p.ocioOptions.view.empty())
            {
                input = !ocioInput.empty() ? ocioInput : p.ocioOptions.input;
                if (!input.empty())
                {
                    ocioData = _ocioData(input);
                }
            }
            p.ocioDataBound = ocioData;
            const std::string name = "display:" + p.ocioKey + '\n' + input;
#else // TLRENDER_OCIO
            const std::string name = "display:" + input;
#endif // TLRENDER_OCIO
            if (p.displayShaders.find(name) == p.displayShaders.end())
            {
                ShaderStage toLinear;
                ShaderStage ocio;
                ShaderStage lut;
                size_t samplers = 1;
#if defined(TLRENDER_OCIO)
                if (ocioData)
                {
                    toLinear = shaderStage(ocioData->toLinear, samplers);
                    samplers += ocioData->toLinear.textures.size();
                    ocio = shaderStage(ocioData->display, samplers);
                    samplers += ocioData->display.textures.size();
                }
                if (p.lutData)
                {
                    lut = shaderStage(p.lutData->stage, samplers);
                    samplers += p.lutData->stage.textures.size();
                }
#endif // TLRENDER_OCIO
                p.baseRender->setShader(
                    name,
                    displayFragmentSource(toLinear, ocio, lut, p.lutOptions.order),
                    samplers);
                std::vector<ftk::gpu::TextureBinding> textures = toLinear.textures;
                textures.insert(textures.end(), ocio.textures.begin(), ocio.textures.end());
                textures.insert(textures.end(), lut.textures.begin(), lut.textures.end());
                p.displayShaders[name] = textures;
            }
            return name;
        }

        std::string Render::_toLinearShader(const std::string& input)
        {
            FTK_P();
            std::string out;
#if defined(TLRENDER_OCIO)
            const auto ocioData = _ocioData(input);
            if (ocioData && ocioData->toLinear.shaderDesc)
            {
                out = "toLinear:" + p.ocioKey + '\n' + input;
                if (p.displayShaders.find(out) == p.displayShaders.end())
                {
                    const ShaderStage toLinear = shaderStage(ocioData->toLinear, 1);
                    p.baseRender->setShader(
                        out,
                        toLinearFragmentSource(toLinear),
                        1 + ocioData->toLinear.textures.size());
                    p.displayShaders[out] = toLinear.textures;
                }
            }
#endif // TLRENDER_OCIO
            return out;
        }

#if defined(TLRENDER_OCIO)
        std::shared_ptr<OCIOData> Render::_ocioData(const std::string& input)
        {
            FTK_P();
            const std::string key = p.ocioKey + '\n' + input;
            auto i = p.ocioData.find(key);
            if (i == p.ocioData.end())
            {
                auto data = std::make_shared<OCIOData>();
                try
                {
                    OCIOOptions options = p.ocioOptions;
                    options.input = input;
                    ocioDataInit(*data, options, p.system->getDevice());
                }
                catch (const std::exception& e)
                {
                    _ocioError(e.what());
                    data.reset();
                }
                i = p.ocioData.insert(std::make_pair(key, data)).first;
            }
            return i->second;
        }

        void Render::_ocioError(const std::string& what)
        {
            if (auto logSystem = _logSystem.lock())
            {
                logSystem->print(
                    "tl::gpu::Render",
                    ftk::Format("Cannot use the OCIO configuration: {0}").arg(what),
                    ftk::LogType::Error);
            }
        }

        void Render::_ocioErase(const std::string& ocioKey)
        {
            FTK_P();
            const std::string suffix = ocioKey + '\n';
            auto i = p.ocioData.lower_bound(suffix);
            while (i != p.ocioData.end() &&
                0 == i->first.compare(0, suffix.size(), suffix))
            {
                i = p.ocioData.erase(i);
            }
            for (const std::string prefix : { "display:", "toLinear:" })
            {
                const std::string begin = prefix + suffix;
                auto j = p.displayShaders.lower_bound(begin);
                while (j != p.displayShaders.end() &&
                    0 == j->first.compare(0, begin.size(), begin))
                {
                    p.baseRender->removeShader(j->first);
                    j = p.displayShaders.erase(j);
                }
            }
        }
#endif // TLRENDER_OCIO

        std::string Render::_layerOCIOInput(
            const std::string& layerInput,
            const std::string& path,
            const std::shared_ptr<ftk::Image>& image)
        {
            FTK_P();
            std::string out = layerInput;
#if defined(TLRENDER_OCIO)
            if (out.empty() && p.ocioInputResolver && !path.empty())
            {
                const std::string key = p.ocioKey + '\n' + path;
                auto i = p.ocioInputCache.find(key);
                if (i == p.ocioInputCache.end())
                {
                    i = p.ocioInputCache.insert(std::make_pair(
                        key,
                        p.ocioInputResolver(
                            path,
                            image ? image->getTags() : ftk::ImageTags()))).first;
                }
                out = i->second;
            }
#endif // TLRENDER_OCIO
            return out;
        }

        void Render::_displayShadersReset()
        {
            FTK_P();
            for (const auto& i : p.displayShaders)
            {
                p.baseRender->removeShader(i.first);
            }
            p.displayShaders.clear();
        }

        std::shared_ptr<ftk::gpu::OffscreenBuffer> Render::_buffer(
            const std::string& name,
            const ftk::Size2I& size,
            ftk::gl::TextureType type)
        {
            FTK_P();
            ftk::gpu::BufferType bufferType = ftk::gpu::BufferType::RGBA_F16;
            switch (type)
            {
            case ftk::gl::TextureType::RGBA_U8: bufferType = ftk::gpu::BufferType::RGBA_U8; break;
            case ftk::gl::TextureType::RGBA_F32: bufferType = ftk::gpu::BufferType::RGBA_F32; break;
            default: break;
            }
            auto& buffer = p.buffers[name];
            if (size.isValid() &&
                (!buffer || buffer->getSize() != size || buffer->getType() != bufferType))
            {
                buffer = ftk::gpu::OffscreenBuffer::create(p.system, size, bufferType);
            }
            return buffer;
        }

        RenderFactory::RenderFactory(const std::shared_ptr<ftk::gpu::System>& system) :
            _system(system)
        {}

        std::shared_ptr<ftk::IRender> RenderFactory::createRender(
            const std::shared_ptr<ftk::LogSystem>& logSystem,
            const std::shared_ptr<ftk::FontSystem>& fontSystem)
        {
            return Render::create(_system.lock(), logSystem, fontSystem);
        }
    }
}

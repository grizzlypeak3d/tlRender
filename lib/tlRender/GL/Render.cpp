// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GL/RenderPrivate.h>

#include <ftk/GL/GL.h>
#include <ftk/GL/Util.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <array>
#include <list>

#define _USE_MATH_DEFINES
#include <math.h>

namespace tl
{
    namespace gl
    {
#if defined(TLRENDER_OCIO)
        OCIOTexture::OCIOTexture(
            unsigned id,
            std::string name,
            std::string sampler,
            unsigned type) :
            id(id),
            name(name),
            sampler(sampler),
            type(type)
        {}

        OCIOStage::~OCIOStage()
        {
            for (size_t i = 0; i < textures.size(); ++i)
            {
                glDeleteTextures(1, &textures[i].id);
            }
        }

        OCIOLUTData::~OCIOLUTData()
        {
            for (size_t i = 0; i < textures.size(); ++i)
            {
                glDeleteTextures(1, &textures[i].id);
            }
        }

        namespace
        {
            void setTextureParameters(GLenum textureType, OCIO::Interpolation interpolation)
            {
                if (OCIO::INTERP_NEAREST == interpolation)
                {
                    glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                }
                else
                {
                    glTexParameteri(textureType, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(textureType, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                }

                glTexParameteri(textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(textureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(textureType, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            }

            // Compile a processor for the GPU: the shader function under
            // the given names, and the textures it samples.
            void ocioStageInit(
                OCIOStage& stage,
                const char* functionName,
                const char* resourcePrefix)
            {
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
                stage.shaderDesc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0);
                stage.shaderDesc->setFunctionName(functionName);
                stage.shaderDesc->setResourcePrefix(resourcePrefix);
                stage.gpuProcessor->extractGpuShaderInfo(stage.shaderDesc);

                // Create 3D textures.
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
                const unsigned num3DTextures = stage.shaderDesc->getNum3DTextures();
                unsigned currentTexture = 0;
                for (unsigned i = 0; i < num3DTextures; ++i, ++currentTexture)
                {
                    const char* textureName = nullptr;
                    const char* samplerName = nullptr;
                    unsigned edgelen = 0;
                    OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                    stage.shaderDesc->get3DTexture(i, textureName, samplerName, edgelen, interpolation);
                    if (!textureName ||
                        !*textureName ||
                        !samplerName ||
                        !*samplerName ||
                        0 == edgelen)
                    {
                        throw std::runtime_error("The OCIO texture data is corrupted");
                    }

                    const float* values = nullptr;
                    stage.shaderDesc->get3DTextureValues(i, values);
                    if (!values)
                    {
                        throw std::runtime_error("The OCIO texture values are missing");
                    }

                    unsigned textureId = 0;
                    glGenTextures(1, &textureId);
                    glBindTexture(GL_TEXTURE_3D, textureId);
                    setTextureParameters(GL_TEXTURE_3D, interpolation);
                    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, edgelen, edgelen, edgelen, 0, GL_RGB, GL_FLOAT, values);
                    stage.textures.push_back(OCIOTexture(textureId, textureName, samplerName, GL_TEXTURE_3D));
                }

                // Create 1D textures.
                const unsigned numTextures = stage.shaderDesc->getNumTextures();
                for (unsigned i = 0; i < numTextures; ++i, ++currentTexture)
                {
                    const char* textureName = nullptr;
                    const char* samplerName = nullptr;
                    unsigned width = 0;
                    unsigned height = 0;
                    OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
                    OCIO::GpuShaderCreator::TextureDimensions dimensions = OCIO::GpuShaderDesc::TEXTURE_1D;
                    OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                    stage.shaderDesc->getTexture(
                        i,
                        textureName,
                        samplerName,
                        width,
                        height,
                        channel,
                        dimensions,
                        interpolation);
                    if (!textureName ||
                        !*textureName ||
                        !samplerName ||
                        !*samplerName ||
                        width == 0)
                    {
                        throw std::runtime_error("The OCIO texture data is corrupted");
                    }

                    const float* values = nullptr;
                    stage.shaderDesc->getTextureValues(i, values);
                    if (!values)
                    {
                        throw std::runtime_error("The OCIO texture values are missing");
                    }

                    unsigned textureId = 0;
                    GLint internalformat = GL_RGB32F;
                    GLenum format = GL_RGB;
                    if (OCIO::GpuShaderCreator::TEXTURE_RED_CHANNEL == channel)
                    {
                        internalformat = GL_R32F;
                        format = GL_RED;
                    }
                    glGenTextures(1, &textureId);
                    switch (dimensions)
                    {
                    case OCIO::GpuShaderDesc::TEXTURE_1D:
                        glBindTexture(GL_TEXTURE_1D, textureId);
                        setTextureParameters(GL_TEXTURE_1D, interpolation);
                        glTexImage1D(GL_TEXTURE_1D, 0, internalformat, width, 0, format, GL_FLOAT, values);
                        break;
                    case OCIO::GpuShaderDesc::TEXTURE_2D:
                        glBindTexture(GL_TEXTURE_2D, textureId);
                        setTextureParameters(GL_TEXTURE_2D, interpolation);
                        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_FLOAT, values);
                        break;
                    }
                    stage.textures.push_back(OCIOTexture(
                        textureId,
                        textureName,
                        samplerName,
                        (height > 1) ? GL_TEXTURE_2D : GL_TEXTURE_1D));
                }
            }

            // Load the configuration and build the two stages. The color
            // corrections apply between the halves of the transform when
            // the configuration names a scene linear role, so they operate
            // on linear values (#328); without the role the display stage
            // carries the whole transform and the corrections stay ahead
            // of it.
            void ocioDataInit(OCIOData& data, const OCIOOptions& options)
            {
                switch (options.config)
                {
                case OCIOConfig::BuiltIn:
                    data.config = OCIO::Config::CreateFromFile("ocio://default");
                    break;
                case OCIOConfig::EnvVar:
                    data.config = OCIO::Config::CreateFromEnv();
                    break;
                case OCIOConfig::File:
                    if (!options.fileName.empty())
                    {
                        data.config = OCIO::Config::CreateFromFile(options.fileName.c_str());
                    }
                    break;
                default: break;
                }
                if (!data.config)
                {
                    throw std::runtime_error("Cannot get OCIO configuration");
                }

                std::string displaySrc = options.input;
                if (data.config->hasRole(OCIO::ROLE_SCENE_LINEAR))
                {
                    data.toLinear.processor =
                        data.config->getProcessor(
                            options.input.c_str(),
                            OCIO::ROLE_SCENE_LINEAR);
                    if (!data.toLinear.processor)
                    {
                        throw std::runtime_error("Cannot get OCIO processor");
                    }
                    ocioStageInit(
                        data.toLinear,
                        "ocioToLinearFunc",
                        "ocioToLinear");
                    displaySrc = OCIO::ROLE_SCENE_LINEAR;
                }

                data.transform = OCIO::DisplayViewTransform::Create();
                if (!data.transform)
                {
                    throw std::runtime_error("Cannot create OCIO transform");
                }
                data.transform->setSrc(displaySrc.c_str());
                data.transform->setDisplay(options.display.c_str());
                data.transform->setView(options.view.c_str());

                data.lvp = OCIO::LegacyViewingPipeline::Create();
                if (!data.lvp)
                {
                    throw std::runtime_error("Cannot create OCIO viewing pipeline");
                }
                data.lvp->setDisplayViewTransform(data.transform);
                data.lvp->setLooksOverrideEnabled(true);
                data.lvp->setLooksOverride(options.look.c_str());

                data.display.processor = data.lvp->getProcessor(
                    data.config,
                    data.config->getCurrentContext());
                if (!data.display.processor)
                {
                    throw std::runtime_error("Cannot get OCIO processor");
                }
                ocioStageInit(data.display, "ocioFunc", "ocio");
            }
        }
#endif // TLRENDER_OCIO

        void Render::_init(
            const std::shared_ptr<ftk::LogSystem>& logSystem,
            const std::shared_ptr<ftk::FontSystem>& fontSystem)
        {
            IRender::_init(logSystem, fontSystem);
            FTK_P();
            p.baseRender = ftk::gl::Render::create(logSystem, fontSystem);
        }

        Render::Render() :
            _p(new Private)
        {}

        Render::~Render()
        {}

        std::shared_ptr<Render> Render::create(
            const std::shared_ptr<ftk::LogSystem>& logSystem,
            const std::shared_ptr<ftk::FontSystem>& fontSystem)
        {
            auto out = std::shared_ptr<Render>(new Render);
            out->_init(logSystem, fontSystem);
            return out;
        }

        void Render::begin(
            const ftk::Size2I& renderSize,
            const ftk::RenderOptions& renderOptions)
        {
            FTK_P();

            p.baseRender->begin(renderSize, renderOptions);

            if (!p.shaders["wipe"])
            {
                p.shaders["wipe"] = ftk::gl::Shader::create(
                    vertexSource(),
                    meshFragmentSource());
            }
            if (!p.shaders["overlay"])
            {
                p.shaders["overlay"] = ftk::gl::Shader::create(
                    vertexSource(),
                    textureFragmentSource());
            }
            if (!p.shaders["butterfly"])
            {
                p.shaders["butterfly"] = ftk::gl::Shader::create(
                    vertexSource(),
                    butterflyFragmentSource());
            }
            if (!p.shaders["difference"])
            {
                p.shaders["difference"] = ftk::gl::Shader::create(
                    vertexSource(),
                    differenceFragmentSource());
            }
            if (!p.shaders["dissolve"])
            {
                p.shaders["dissolve"] = ftk::gl::Shader::create(
                    vertexSource(),
                    dissolveFragmentSource());
            }
            _displayShader();

            p.vbos["wipe"] = ftk::gl::VBO::create(1 * 3, ftk::gl::VBOType::Pos2_F32);
            p.vaos["wipe"] = ftk::gl::VAO::create(p.vbos["wipe"]->getType(), p.vbos["wipe"]->getID());
            p.vbos["video"] = ftk::gl::VBO::create(2 * 3, ftk::gl::VBOType::Pos2_F32_UV_U16);
            p.vaos["video"] = ftk::gl::VAO::create(p.vbos["video"]->getType(), p.vbos["video"]->getID());

            setTransform(p.baseRender->getTransform());
        }

        void Render::end()
        {
            FTK_P();
            p.baseRender->end();
        }

        void Render::setOCIOOptions(const OCIOOptions& value)
        {
            FTK_P();
            if (value == p.ocioOptions)
                return;

#if defined(TLRENDER_OCIO)
            p.ocioData.clear();
            p.ocioDataBound.reset();
#endif // TLRENDER_OCIO

            p.ocioOptions = value;

#if defined(TLRENDER_OCIO)
            if (p.ocioOptions.enabled &&
                !p.ocioOptions.input.empty() &&
                !p.ocioOptions.display.empty() &&
                !p.ocioOptions.view.empty())
            {
                auto data = std::make_shared<OCIOData>();
                ocioDataInit(*data, p.ocioOptions);
                p.ocioData[p.ocioOptions.input] = data;
            }
#endif // TLRENDER_OCIO

            _displayShadersReset();
            _displayShader();
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
            if (p.lutOptions.enabled && !p.lutOptions.fileName.empty())
            {
                p.lutData.reset(new OCIOLUTData);

                p.lutData->config = OCIO::Config::CreateRaw();
                if (!p.lutData->config)
                {
                    throw std::runtime_error("Cannot create OCIO configuration");
                }

                p.lutData->transform = OCIO::FileTransform::Create();
                if (!p.lutData->transform)
                {
                    p.lutData.reset();
                    throw std::runtime_error("Cannot create OCIO transform");
                }
                p.lutData->transform->setSrc(p.lutOptions.fileName.c_str());
                p.lutData->transform->validate();

                p.lutData->processor = p.lutData->config->getProcessor(p.lutData->transform);
                if (!p.lutData->processor)
                {
                    p.lutData.reset();
                    throw std::runtime_error("Cannot get OCIO processor");
                }
                p.lutData->gpuProcessor = p.lutData->processor->getDefaultGPUProcessor();
                if (!p.lutData->gpuProcessor)
                {
                    p.lutData.reset();
                    throw std::runtime_error("Cannot get OCIO GPU processor");
                }
                p.lutData->shaderDesc = OCIO::GpuShaderDesc::CreateShaderDesc();
                if (!p.lutData->shaderDesc)
                {
                    p.lutData.reset();
                    throw std::runtime_error("Cannot create OCIO shader description");
                }
                p.lutData->shaderDesc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0);
                p.lutData->shaderDesc->setFunctionName("lutFunc");
                p.lutData->shaderDesc->setResourcePrefix("lut");
                p.lutData->gpuProcessor->extractGpuShaderInfo(p.lutData->shaderDesc);

                // Create 3D textures.
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
                const unsigned num3DTextures = p.lutData->shaderDesc->getNum3DTextures();
                unsigned currentTexture = 0;
                for (unsigned i = 0; i < num3DTextures; ++i, ++currentTexture)
                {
                    const char* textureName = nullptr;
                    const char* samplerName = nullptr;
                    unsigned edgelen = 0;
                    OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                    p.lutData->shaderDesc->get3DTexture(i, textureName, samplerName, edgelen, interpolation);
                    if (!textureName ||
                        !*textureName ||
                        !samplerName ||
                        !*samplerName ||
                        0 == edgelen)
                    {
                        p.lutData.reset();
                        throw std::runtime_error("The OCIO texture data is corrupted");
                    }

                    const float* values = nullptr;
                    p.lutData->shaderDesc->get3DTextureValues(i, values);
                    if (!values)
                    {
                        p.lutData.reset();
                        throw std::runtime_error("The OCIO texture values are missing");
                    }

                    unsigned textureId = 0;
                    glGenTextures(1, &textureId);
                    glBindTexture(GL_TEXTURE_3D, textureId);
                    setTextureParameters(GL_TEXTURE_3D, interpolation);
                    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, edgelen, edgelen, edgelen, 0, GL_RGB, GL_FLOAT, values);
                    p.lutData->textures.push_back(OCIOTexture(textureId, textureName, samplerName, GL_TEXTURE_3D));
                }

                // Create 1D textures.
                const unsigned numTextures = p.lutData->shaderDesc->getNumTextures();
                for (unsigned i = 0; i < numTextures; ++i, ++currentTexture)
                {
                    const char* textureName = nullptr;
                    const char* samplerName = nullptr;
                    unsigned width = 0;
                    unsigned height = 0;
                    OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
                    OCIO::GpuShaderDesc::TextureDimensions dimensions = OCIO::GpuShaderDesc::TEXTURE_1D;
                    OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                    p.lutData->shaderDesc->getTexture(
                        i, textureName,
                        samplerName,
                        width,
                        height,
                        channel,
                        dimensions,
                        interpolation);
                    if (!textureName ||
                        !*textureName ||
                        !samplerName ||
                        !*samplerName ||
                        width == 0)
                    {
                        p.lutData.reset();
                        throw std::runtime_error("The OCIO texture data is corrupted");
                    }

                    const float* values = nullptr;
                    p.lutData->shaderDesc->getTextureValues(i, values);
                    if (!values)
                    {
                        p.lutData.reset();
                        throw std::runtime_error("The OCIO texture values are missing");
                    }

                    unsigned textureId = 0;
                    GLint internalformat = GL_RGB32F;
                    GLenum format = GL_RGB;
                    if (OCIO::GpuShaderCreator::TEXTURE_RED_CHANNEL == channel)
                    {
                        internalformat = GL_R32F;
                        format = GL_RED;
                    }
                    glGenTextures(1, &textureId);
                    switch (dimensions)
                    {
                    case OCIO::GpuShaderDesc::TEXTURE_1D:
                        glBindTexture(GL_TEXTURE_1D, textureId);
                        setTextureParameters(GL_TEXTURE_1D, interpolation);
                        glTexImage1D(GL_TEXTURE_1D, 0, internalformat, width, 0, format, GL_FLOAT, values);
                        break;
                    case OCIO::GpuShaderDesc::TEXTURE_2D:
                        glBindTexture(GL_TEXTURE_2D, textureId);
                        setTextureParameters(GL_TEXTURE_2D, interpolation);
                        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_FLOAT, values);
                        break;
                    }
                    p.lutData->textures.push_back(OCIOTexture(
                        textureId,
                        textureName,
                        samplerName,
                        (height > 1) ? GL_TEXTURE_2D : GL_TEXTURE_1D));
                }
            }
#endif // TLRENDER_OCIO

            _displayShadersReset();
            _displayShader();
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
            FTK_P();
            p.baseRender->setTransform(value);
            for (auto i : p.shaders)
            {
                i.second->bind();
                i.second->setUniform("transform.mvp", value);
            }
        }
        
        ftk::RenderDiag Render::getDiag() const
        {
            return _p->baseRender->getDiag();
        }

        std::shared_ptr<ftk::gl::Shader> Render::_displayShader(
            const std::string& ocioInput)
        {
            FTK_P();
            std::string input;
#if defined(TLRENDER_OCIO)
            std::shared_ptr<OCIOData> ocioData;
            if (p.ocioOptions.enabled &&
                !p.ocioOptions.display.empty() &&
                !p.ocioOptions.view.empty())
            {
                // The item's own input color space when it has one, the
                // options' otherwise. An input is built the first time it
                // is seen; one that cannot be built is remembered as
                // empty, so the item draws without color management
                // rather than breaking the draw or being tried again
                // every frame.
                input = !ocioInput.empty() ? ocioInput : p.ocioOptions.input;
                if (!input.empty())
                {
                    auto i = p.ocioData.find(input);
                    if (i == p.ocioData.end())
                    {
                        auto data = std::make_shared<OCIOData>();
                        try
                        {
                            OCIOOptions options = p.ocioOptions;
                            options.input = input;
                            ocioDataInit(*data, options);
                        }
                        catch (const std::exception&)
                        {
                            data.reset();
                        }
                        i = p.ocioData.insert(std::make_pair(input, data)).first;
                    }
                    ocioData = i->second;
                }
            }
            p.ocioDataBound = ocioData;
#endif // TLRENDER_OCIO

            const std::string key = "display:" + input;
            if (!p.shaders[key])
            {
                std::string toLinearDef;
                std::string toLinear;
                std::string ocioDef;
                std::string ocio;
                std::string lutDef;
                std::string lut;

#if defined(TLRENDER_OCIO)
                if (ocioData && ocioData->toLinear.shaderDesc)
                {
                    toLinearDef = ocioData->toLinear.shaderDesc->getShaderText();
                    toLinear = "outColor = ocioToLinearFunc(outColor);";
                }
                if (ocioData && ocioData->display.shaderDesc)
                {
                    ocioDef = ocioData->display.shaderDesc->getShaderText();
                    ocio = "outColor = ocioFunc(outColor);";
                }
                if (p.lutData && p.lutData->shaderDesc)
                {
                    lutDef = p.lutData->shaderDesc->getShaderText();
                    lut = "outColor = lutFunc(outColor);";
                }
#endif // TLRENDER_OCIO
                const std::string source = displayFragmentSource(
                    toLinearDef,
                    toLinear,
                    ocioDef,
                    ocio,
                    lutDef,
                    lut,
                    p.lutOptions.order);
                p.shaders[key] = ftk::gl::Shader::create(vertexSource(), source);
            }
            const auto shader = p.shaders[key];
            shader->bind();
            shader->setUniform("transform.mvp", getTransform());
#if defined(TLRENDER_OCIO)
            size_t texturesOffset = 1;
            if (ocioData)
            {
                for (size_t i = 0; i < ocioData->toLinear.textures.size(); ++i)
                {
                    shader->setUniform(
                        ocioData->toLinear.textures[i].sampler,
                        static_cast<int>(texturesOffset + i));
                }
                texturesOffset += ocioData->toLinear.textures.size();
                for (size_t i = 0; i < ocioData->display.textures.size(); ++i)
                {
                    shader->setUniform(
                        ocioData->display.textures[i].sampler,
                        static_cast<int>(texturesOffset + i));
                }
                texturesOffset += ocioData->display.textures.size();
            }
            if (p.lutData)
            {
                for (size_t i = 0; i < p.lutData->textures.size(); ++i)
                {
                    shader->setUniform(
                        p.lutData->textures[i].sampler,
                        static_cast<int>(texturesOffset + i));
                }
                texturesOffset += p.lutData->textures.size();
            }
#endif // TLRENDER_OCIO
            return shader;
        }

        void Render::_displayShadersReset()
        {
            FTK_P();
            // The display shaders are keyed by the input color space;
            // drop them all.
            auto i = p.shaders.begin();
            while (i != p.shaders.end())
            {
                if (0 == i->first.compare(0, 8, "display:"))
                {
                    i = p.shaders.erase(i);
                }
                else
                {
                    ++i;
                }
            }
        }

        std::shared_ptr<ftk::IRender> RenderFactory::createRender(
            const std::shared_ptr<ftk::LogSystem>& logSystem,
            const std::shared_ptr<ftk::FontSystem>& fontSystem)
        {
            return Render::create(logSystem, fontSystem);
        }
    }
}

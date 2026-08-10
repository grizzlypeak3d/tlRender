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

        OCIOData::~OCIOData()
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

        namespace
        {
#if defined(TLRENDER_OCIO)
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
#endif // TLRENDER_OCIO
        }

        void Render::setOCIOOptions(const OCIOOptions& value)
        {
            FTK_P();
            if (value == p.ocioOptions)
                return;

#if defined(TLRENDER_OCIO)
            p.ocioData.reset();
#endif // TLRENDER_OCIO

            p.ocioOptions = value;

#if defined(TLRENDER_OCIO)
            if (p.ocioOptions.enabled &&
                !p.ocioOptions.input.empty() &&
                !p.ocioOptions.display.empty() &&
                !p.ocioOptions.view.empty())
            {
                p.ocioData.reset(new OCIOData);

                switch (p.ocioOptions.config)
                {
                case OCIOConfig::BuiltIn:
                    p.ocioData->config = OCIO::Config::CreateFromFile("ocio://default");
                    break;
                case OCIOConfig::EnvVar:
                    p.ocioData->config = OCIO::Config::CreateFromEnv();
                    break;
                case OCIOConfig::File:
                    if (!p.ocioOptions.fileName.empty())
                    {
                        p.ocioData->config = OCIO::Config::CreateFromFile(p.ocioOptions.fileName.c_str());
                    }
                    break;
                default: break;
                }
                if (!p.ocioData->config)
                {
                    throw std::runtime_error("Cannot get OCIO configuration");
                }

                p.ocioData->transform = OCIO::DisplayViewTransform::Create();
                if (!p.ocioData->transform)
                {
                    p.ocioData.reset();
                    throw std::runtime_error("Cannot create OCIO transform");
                }
                p.ocioData->transform->setSrc(p.ocioOptions.input.c_str());
                p.ocioData->transform->setDisplay(p.ocioOptions.display.c_str());
                p.ocioData->transform->setView(p.ocioOptions.view.c_str());

                p.ocioData->lvp = OCIO::LegacyViewingPipeline::Create();
                if (!p.ocioData->lvp)
                {
                    p.ocioData.reset();
                    throw std::runtime_error("Cannot create OCIO viewing pipeline");
                }
                p.ocioData->lvp->setDisplayViewTransform(p.ocioData->transform);
                p.ocioData->lvp->setLooksOverrideEnabled(true);
                p.ocioData->lvp->setLooksOverride(p.ocioOptions.look.c_str());

                p.ocioData->processor = p.ocioData->lvp->getProcessor(
                    p.ocioData->config,
                    p.ocioData->config->getCurrentContext());
                if (!p.ocioData->processor)
                {
                    p.ocioData.reset();
                    throw std::runtime_error("Cannot get OCIO processor");
                }
                p.ocioData->gpuProcessor = p.ocioData->processor->getDefaultGPUProcessor();
                if (!p.ocioData->gpuProcessor)
                {
                    p.ocioData.reset();
                    throw std::runtime_error("Cannot get OCIO GPU processor");
                }
                p.ocioData->shaderDesc = OCIO::GpuShaderDesc::CreateShaderDesc();
                if (!p.ocioData->shaderDesc)
                {
                    p.ocioData.reset();
                    throw std::runtime_error("Cannot create OCIO shader description");
                }
                p.ocioData->shaderDesc->setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0);
                p.ocioData->shaderDesc->setFunctionName("ocioFunc");
                p.ocioData->shaderDesc->setResourcePrefix("ocio");
                p.ocioData->gpuProcessor->extractGpuShaderInfo(p.ocioData->shaderDesc);

                // Create 3D textures.
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
                const unsigned num3DTextures = p.ocioData->shaderDesc->getNum3DTextures();
                unsigned currentTexture = 0;
                for (unsigned i = 0; i < num3DTextures; ++i, ++currentTexture)
                {
                    const char* textureName = nullptr;
                    const char* samplerName = nullptr;
                    unsigned edgelen = 0;
                    OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                    p.ocioData->shaderDesc->get3DTexture(i, textureName, samplerName, edgelen, interpolation);
                    if (!textureName ||
                        !*textureName ||
                        !samplerName ||
                        !*samplerName ||
                        0 == edgelen)
                    {
                        p.ocioData.reset();
                        throw std::runtime_error("The OCIO texture data is corrupted");
                    }

                    const float* values = nullptr;
                    p.ocioData->shaderDesc->get3DTextureValues(i, values);
                    if (!values)
                    {
                        p.ocioData.reset();
                        throw std::runtime_error("The OCIO texture values are missing");
                    }

                    unsigned textureId = 0;
                    glGenTextures(1, &textureId);
                    glBindTexture(GL_TEXTURE_3D, textureId);
                    setTextureParameters(GL_TEXTURE_3D, interpolation);
                    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, edgelen, edgelen, edgelen, 0, GL_RGB, GL_FLOAT, values);
                    p.ocioData->textures.push_back(OCIOTexture(textureId, textureName, samplerName, GL_TEXTURE_3D));
                }

                // Create 1D textures.
                const unsigned numTextures = p.ocioData->shaderDesc->getNumTextures();
                for (unsigned i = 0; i < numTextures; ++i, ++currentTexture)
                {
                    const char* textureName = nullptr;
                    const char* samplerName = nullptr;
                    unsigned width = 0;
                    unsigned height = 0;
                    OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
                    OCIO::GpuShaderCreator::TextureDimensions dimensions = OCIO::GpuShaderDesc::TEXTURE_1D;
                    OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                    p.ocioData->shaderDesc->getTexture(
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
                        p.ocioData.reset();
                        throw std::runtime_error("The OCIO texture data is corrupted");
                    }

                    const float* values = nullptr;
                    p.ocioData->shaderDesc->getTextureValues(i, values);
                    if (!values)
                    {
                        p.ocioData.reset();
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
                    p.ocioData->textures.push_back(OCIOTexture(
                        textureId,
                        textureName,
                        samplerName,
                        (height > 1) ? GL_TEXTURE_2D : GL_TEXTURE_1D));
                }
            }
#endif // TLRENDER_OCIO

            p.shaders["display"].reset();
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

            p.shaders["display"].reset();
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

        void Render::_displayShader()
        {
            FTK_P();
            if (!p.shaders["display"])
            {
                std::string ocioDef;
                std::string ocio;
                std::string lutDef;
                std::string lut;

#if defined(TLRENDER_OCIO)
                if (p.ocioData && p.ocioData->shaderDesc)
                {
                    ocioDef = p.ocioData->shaderDesc->getShaderText();
                    ocio = "outColor = ocioFunc(outColor);";
                }
                if (p.lutData && p.lutData->shaderDesc)
                {
                    lutDef = p.lutData->shaderDesc->getShaderText();
                    lut = "outColor = lutFunc(outColor);";
                }
#endif // TLRENDER_OCIO
                const std::string source = displayFragmentSource(
                    ocioDef,
                    ocio,
                    lutDef,
                    lut,
                    p.lutOptions.order);
                p.shaders["display"] = ftk::gl::Shader::create(vertexSource(), source);
            }
            p.shaders["display"]->bind();
            p.shaders["display"]->setUniform("transform.mvp", getTransform());
#if defined(TLRENDER_OCIO)
            size_t texturesOffset = 1;
            if (p.ocioData)
            {
                for (size_t i = 0; i < p.ocioData->textures.size(); ++i)
                {
                    p.shaders["display"]->setUniform(
                        p.ocioData->textures[i].sampler,
                        static_cast<int>(texturesOffset + i));
                }
                texturesOffset += p.ocioData->textures.size();
            }
            if (p.lutData)
            {
                for (size_t i = 0; i < p.lutData->textures.size(); ++i)
                {
                    p.shaders["display"]->setUniform(
                        p.lutData->textures[i].sampler,
                        static_cast<int>(texturesOffset + i));
                }
                texturesOffset += p.lutData->textures.size();
            }
#endif // TLRENDER_OCIO
        }

        std::shared_ptr<ftk::IRender> RenderFactory::createRender(
            const std::shared_ptr<ftk::LogSystem>& logSystem,
            const std::shared_ptr<ftk::FontSystem>& fontSystem)
        {
            return Render::create(logSystem, fontSystem);
        }
    }
}

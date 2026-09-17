// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GL/RenderPrivate.h>

#include <ftk/GL/GL.h>
#include <ftk/GL/Util.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <list>
#include <mutex>

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

            // OCIO emits the dialect asked of it. ES 3.0 filters 32 bit
            // float textures only with an extension, and half float in
            // core; a LUT interpolates fine at half.
#if defined(FTK_API_GLES_3)
            const OCIO::GpuLanguage gpuLanguage = OCIO::GPU_LANGUAGE_GLSL_ES_3_0;
            const GLint lutInternalFormatRGB = GL_RGB16F;
            const GLint lutInternalFormatR = GL_R16F;
#else // FTK_API_GLES_3
            const OCIO::GpuLanguage gpuLanguage = OCIO::GPU_LANGUAGE_GLSL_4_0;
            const GLint lutInternalFormatRGB = GL_RGB32F;
            const GLint lutInternalFormatR = GL_R32F;
#endif // FTK_API_GLES_3

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
                stage.shaderDesc->setLanguage(gpuLanguage);
                stage.shaderDesc->setFunctionName(functionName);
                stage.shaderDesc->setResourcePrefix(resourcePrefix);
                stage.gpuProcessor->extractGpuShaderInfo(stage.shaderDesc);

                // Create 3D textures.
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#if !defined(FTK_API_GLES_3)
                // ES has no byte swapping; the data is native order.
                glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
#endif // FTK_API_GLES_3
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
                    glTexImage3D(GL_TEXTURE_3D, 0, lutInternalFormatRGB, edgelen, edgelen, edgelen, 0, GL_RGB, GL_FLOAT, values);
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
                    GLint internalformat = lutInternalFormatRGB;
                    GLenum format = GL_RGB;
                    if (OCIO::GpuShaderCreator::TEXTURE_RED_CHANNEL == channel)
                    {
                        internalformat = lutInternalFormatR;
                        format = GL_RED;
                    }
                    glGenTextures(1, &textureId);
                    switch (dimensions)
                    {
                    case OCIO::GpuShaderDesc::TEXTURE_1D:
#if defined(FTK_API_GLES_3)
                        // ES has no 1D textures; the ES shader samples a
                        // height of one.
                        glBindTexture(GL_TEXTURE_2D, textureId);
                        setTextureParameters(GL_TEXTURE_2D, interpolation);
                        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, 1, 0, format, GL_FLOAT, values);
#else // FTK_API_GLES_3
                        glBindTexture(GL_TEXTURE_1D, textureId);
                        setTextureParameters(GL_TEXTURE_1D, interpolation);
                        glTexImage1D(GL_TEXTURE_1D, 0, internalformat, width, 0, format, GL_FLOAT, values);
#endif // FTK_API_GLES_3
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
#if defined(FTK_API_GLES_3)
                        GL_TEXTURE_2D));
#else // FTK_API_GLES_3
                        (height > 1) ? GL_TEXTURE_2D : GL_TEXTURE_1D));
#endif // FTK_API_GLES_3
                }
            }

            // The configuration the options name, read once. Reading one
            // parses a whole YAML document -- the built in configuration
            // included -- and OCIO keeps its processor cache on the object,
            // so building a second one throws that away as well.
            //
            // A configuration that comes from a file is remembered with
            // that file's size and write time, so editing it is still
            // picked up.
            struct OCIOConfigKey
            {
                OCIOConfig  kind = OCIOConfig::BuiltIn;
                std::string fileName;
                uintmax_t   size = 0;
                int64_t     time = 0;

                bool operator == (const OCIOConfigKey&) const = default;
            };

            OCIO::ConstConfigRcPtr ocioConfig(const OCIOOptions& options)
            {
                OCIOConfigKey key;
                key.kind = options.config;
                switch (options.config)
                {
                case OCIOConfig::EnvVar:
                    if (const char* env = std::getenv("OCIO"))
                    {
                        key.fileName = env;
                    }
                    break;
                case OCIOConfig::File:
                    key.fileName = options.fileName;
                    break;
                default: break;
                }
                if (!key.fileName.empty())
                {
                    std::error_code ec;
                    const std::filesystem::path path(key.fileName);
                    const auto size = std::filesystem::file_size(path, ec);
                    if (!ec)
                    {
                        key.size = size;
                    }
                    const auto time = std::filesystem::last_write_time(path, ec);
                    if (!ec)
                    {
                        key.time = time.time_since_epoch().count();
                    }
                }

                // Most recently used first, and bounded: what is in play is
                // the configuration the viewport draws through and whatever
                // the timeline items use.
                static std::mutex mutex;
                static std::list<
                    std::pair<OCIOConfigKey, OCIO::ConstConfigRcPtr> > cache;
                std::unique_lock<std::mutex> lock(mutex);
                for (auto i = cache.begin(); i != cache.end(); ++i)
                {
                    if (i->first == key)
                    {
                        cache.splice(cache.begin(), cache, i);
                        return cache.front().second;
                    }
                }

                OCIO::ConstConfigRcPtr out;
                switch (options.config)
                {
                case OCIOConfig::BuiltIn:
                    out = OCIO::Config::CreateFromFile("ocio://default");
                    break;
                case OCIOConfig::EnvVar:
                    out = OCIO::Config::CreateFromEnv();
                    break;
                case OCIOConfig::File:
                    if (!options.fileName.empty())
                    {
                        out = OCIO::Config::CreateFromFile(options.fileName.c_str());
                    }
                    break;
                default: break;
                }
                if (out)
                {
                    cache.push_front(std::make_pair(key, out));
                    while (cache.size() > 4)
                    {
                        cache.pop_back();
                    }
                }
                return out;
            }

            // What the data and shaders built from a set of options are
            // keyed by, so that two sets can be held at once.
            std::string ocioOptionsKey(const OCIOOptions& options)
            {
                return
                    std::string(options.enabled ? "1" : "0") + '\n' +
                    std::to_string(static_cast<int>(options.config)) + '\n' +
                    options.fileName + '\n' +
                    options.input + '\n' +
                    options.display + '\n' +
                    options.view + '\n' +
                    options.look;
            }

            // Load the configuration and build the two stages. The color
            // corrections apply between the halves of the transform when
            // the configuration names a scene linear role, so they operate
            // on linear values (#328); without the role the display stage
            // carries the whole transform and the corrections stay ahead
            // of it.
            void ocioDataInit(OCIOData& data, const OCIOOptions& options)
            {
                data.config = ocioConfig(options);
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
#if defined(TLRENDER_OCIO)
            // The key for the options as they start out, so that anything
            // drawn before the first setOCIOOptions() is keyed the same way
            // as it would be after being set to the same value.
            p.ocioKey = ocioOptionsKey(p.ocioOptions);
            p.ocioKeys.push_front(p.ocioKey);
#endif // TLRENDER_OCIO
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
            if (!p.shaders["clippingWarning"])
            {
                p.shaders["clippingWarning"] = ftk::gl::Shader::create(
                    vertexSource(),
                    clippingWarningFragmentSource());
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

            p.ocioOptions = value;

#if defined(TLRENDER_OCIO)
            p.ocioDataBound.reset();
            p.ocioToLinearBound.reset();

            // Switch to this set of options rather than throwing away what
            // the last set built: the viewport and the timeline items pass
            // different options every frame, so the two alternate.
            p.ocioKey = ocioOptionsKey(p.ocioOptions);
            auto i = std::find(p.ocioKeys.begin(), p.ocioKeys.end(), p.ocioKey);
            if (i != p.ocioKeys.end())
            {
                p.ocioKeys.erase(i);
            }
            p.ocioKeys.push_front(p.ocioKey);
            while (p.ocioKeys.size() > 4)
            {
                // The data holds textures and the shaders are compiled, so
                // what is no longer being drawn through is let go of.
                _ocioErase(p.ocioKeys.back());
                p.ocioKeys.pop_back();
            }

            if (p.ocioOptions.enabled &&
                !p.ocioOptions.input.empty() &&
                !p.ocioOptions.display.empty() &&
                !p.ocioOptions.view.empty())
            {
                const std::string key = p.ocioKey + '\n' + p.ocioOptions.input;
                if (p.ocioData.find(key) == p.ocioData.end())
                {
                    // Remembered as empty when it cannot be built, the same
                    // as _ocioData(): the picture is drawn without color
                    // management rather than not at all.
                    auto data = std::make_shared<OCIOData>();
                    try
                    {
                        ocioDataInit(*data, p.ocioOptions);
                    }
                    catch (const std::exception& e)
                    {
                        _ocioError(e.what());
                        data.reset();
                    }
                    p.ocioData[key] = data;
                }
            }
#endif // TLRENDER_OCIO

            _displayShader();
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
            // A LUT that cannot be read -- a file since moved, or one a
            // review names on another machine -- leaves the picture as it is
            // rather than taking it away: the error is logged and the display
            // draws without the LUT. Throwing from here stopped the draw it
            // was called from, so the viewport showed nothing. The LUT is
            // built apart and kept only once it is whole, so a failure
            // partway leaves nothing half made behind.
            if (p.lutOptions.enabled && !p.lutOptions.fileName.empty())
            {
                try
                {
                    auto lutData = std::make_unique<OCIOLUTData>();

                    lutData->config = OCIO::Config::CreateRaw();
                    if (!lutData->config)
                    {
                        throw std::runtime_error("Cannot create OCIO configuration");
                    }

                    lutData->transform = OCIO::FileTransform::Create();
                    if (!lutData->transform)
                    {
                        throw std::runtime_error("Cannot create OCIO transform");
                    }
                    lutData->transform->setSrc(p.lutOptions.fileName.c_str());
                    lutData->transform->setDirection(
                        LUTDirection::Inverse == p.lutOptions.direction ?
                            OCIO::TRANSFORM_DIR_INVERSE :
                            OCIO::TRANSFORM_DIR_FORWARD);
                    lutData->transform->validate();

                    lutData->processor = lutData->config->getProcessor(lutData->transform);
                    if (!lutData->processor)
                    {
                        throw std::runtime_error("Cannot get OCIO processor");
                    }
                    lutData->gpuProcessor = lutData->processor->getDefaultGPUProcessor();
                    if (!lutData->gpuProcessor)
                    {
                        throw std::runtime_error("Cannot get OCIO GPU processor");
                    }
                    lutData->shaderDesc = OCIO::GpuShaderDesc::CreateShaderDesc();
                    if (!lutData->shaderDesc)
                    {
                        throw std::runtime_error("Cannot create OCIO shader description");
                    }
                    lutData->shaderDesc->setLanguage(gpuLanguage);
                    lutData->shaderDesc->setFunctionName("lutFunc");
                    lutData->shaderDesc->setResourcePrefix("lut");
                    lutData->gpuProcessor->extractGpuShaderInfo(lutData->shaderDesc);

                    // Create 3D textures.
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    #if !defined(FTK_API_GLES_3)
                    // ES has no byte swapping; the data is native order.
                    glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
    #endif // FTK_API_GLES_3
                    const unsigned num3DTextures = lutData->shaderDesc->getNum3DTextures();
                    unsigned currentTexture = 0;
                    for (unsigned i = 0; i < num3DTextures; ++i, ++currentTexture)
                    {
                        const char* textureName = nullptr;
                        const char* samplerName = nullptr;
                        unsigned edgelen = 0;
                        OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                        lutData->shaderDesc->get3DTexture(i, textureName, samplerName, edgelen, interpolation);
                        if (!textureName ||
                            !*textureName ||
                            !samplerName ||
                            !*samplerName ||
                            0 == edgelen)
                        {
                                throw std::runtime_error("The OCIO texture data is corrupted");
                        }

                        const float* values = nullptr;
                        lutData->shaderDesc->get3DTextureValues(i, values);
                        if (!values)
                        {
                                throw std::runtime_error("The OCIO texture values are missing");
                        }

                        unsigned textureId = 0;
                        glGenTextures(1, &textureId);
                        glBindTexture(GL_TEXTURE_3D, textureId);
                        setTextureParameters(GL_TEXTURE_3D, interpolation);
                        glTexImage3D(GL_TEXTURE_3D, 0, lutInternalFormatRGB, edgelen, edgelen, edgelen, 0, GL_RGB, GL_FLOAT, values);
                        lutData->textures.push_back(OCIOTexture(textureId, textureName, samplerName, GL_TEXTURE_3D));
                    }

                    // Create 1D textures.
                    const unsigned numTextures = lutData->shaderDesc->getNumTextures();
                    for (unsigned i = 0; i < numTextures; ++i, ++currentTexture)
                    {
                        const char* textureName = nullptr;
                        const char* samplerName = nullptr;
                        unsigned width = 0;
                        unsigned height = 0;
                        OCIO::GpuShaderDesc::TextureType channel = OCIO::GpuShaderDesc::TEXTURE_RGB_CHANNEL;
                        OCIO::GpuShaderDesc::TextureDimensions dimensions = OCIO::GpuShaderDesc::TEXTURE_1D;
                        OCIO::Interpolation interpolation = OCIO::INTERP_LINEAR;
                        lutData->shaderDesc->getTexture(
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
                                throw std::runtime_error("The OCIO texture data is corrupted");
                        }

                        const float* values = nullptr;
                        lutData->shaderDesc->getTextureValues(i, values);
                        if (!values)
                        {
                                throw std::runtime_error("The OCIO texture values are missing");
                        }

                        unsigned textureId = 0;
                        GLint internalformat = lutInternalFormatRGB;
                        GLenum format = GL_RGB;
                        if (OCIO::GpuShaderCreator::TEXTURE_RED_CHANNEL == channel)
                        {
                            internalformat = lutInternalFormatR;
                            format = GL_RED;
                        }
                        glGenTextures(1, &textureId);
                        switch (dimensions)
                        {
                        case OCIO::GpuShaderDesc::TEXTURE_1D:
    #if defined(FTK_API_GLES_3)
                            // ES has no 1D textures; the ES shader samples a
                            // height of one.
                            glBindTexture(GL_TEXTURE_2D, textureId);
                            setTextureParameters(GL_TEXTURE_2D, interpolation);
                            glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, 1, 0, format, GL_FLOAT, values);
    #else // FTK_API_GLES_3
                            glBindTexture(GL_TEXTURE_1D, textureId);
                            setTextureParameters(GL_TEXTURE_1D, interpolation);
                            glTexImage1D(GL_TEXTURE_1D, 0, internalformat, width, 0, format, GL_FLOAT, values);
    #endif // FTK_API_GLES_3
                            break;
                        case OCIO::GpuShaderDesc::TEXTURE_2D:
                            glBindTexture(GL_TEXTURE_2D, textureId);
                            setTextureParameters(GL_TEXTURE_2D, interpolation);
                            glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, GL_FLOAT, values);
                            break;
                        }
                        lutData->textures.push_back(OCIOTexture(
                            textureId,
                            textureName,
                            samplerName,
    #if defined(FTK_API_GLES_3)
                            GL_TEXTURE_2D));
    #else // FTK_API_GLES_3
                            (height > 1) ? GL_TEXTURE_2D : GL_TEXTURE_1D));
    #endif // FTK_API_GLES_3
                    }
                    p.lutData = std::move(lutData);
                }
                catch (const std::exception& e)
                {
                    if (auto logSystem = _logSystem.lock())
                    {
                        logSystem->print(
                            "tl::gl::Render",
                            ftk::Format("Cannot read the LUT \"{0}\": {1}").
                                arg(p.lutOptions.fileName).
                                arg(e.what()),
                            ftk::LogType::Error);
                    }
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
                // options' otherwise.
                input = !ocioInput.empty() ? ocioInput : p.ocioOptions.input;
                if (!input.empty())
                {
                    ocioData = _ocioData(input);
                }
            }
            p.ocioDataBound = ocioData;
#endif // TLRENDER_OCIO

#if defined(TLRENDER_OCIO)
            // The options are in the key as well as the input: the shader
            // carries the transform's source, so two sets of options make
            // two different shaders for the same input.
            const std::string key = "display:" + p.ocioKey + '\n' + input;
#else // TLRENDER_OCIO
            const std::string key = "display:" + input;
#endif // TLRENDER_OCIO
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

#if defined(TLRENDER_OCIO)
        std::shared_ptr<OCIOData> Render::_ocioData(const std::string& input)
        {
            FTK_P();
            // Data is built the first time an input is seen; one that
            // cannot be built is remembered as empty, so it draws without
            // color management rather than breaking the draw or being
            // tried again every frame.
            const std::string key = p.ocioKey + '\n' + input;
            auto i = p.ocioData.find(key);
            if (i == p.ocioData.end())
            {
                auto data = std::make_shared<OCIOData>();
                try
                {
                    OCIOOptions options = p.ocioOptions;
                    options.input = input;
                    ocioDataInit(*data, options);
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
#endif // TLRENDER_OCIO

        std::shared_ptr<ftk::gl::Shader> Render::_toLinearShader(
            const std::string& input)
        {
            FTK_P();
            std::shared_ptr<ftk::gl::Shader> out;
#if defined(TLRENDER_OCIO)
            const auto ocioData = _ocioData(input);
            if (ocioData && ocioData->toLinear.shaderDesc)
            {
                const std::string key =
                    "toLinear:" + p.ocioKey + '\n' + input;
                if (!p.shaders[key])
                {
                    const std::string source = toLinearFragmentSource(
                        ocioData->toLinear.shaderDesc->getShaderText(),
                        "outColor = ocioToLinearFunc(outColor);");
                    p.shaders[key] = ftk::gl::Shader::create(vertexSource(), source);
                }
                out = p.shaders[key];
                out->bind();
                out->setUniform("textureSampler", 0);
                for (size_t i = 0; i < ocioData->toLinear.textures.size(); ++i)
                {
                    out->setUniform(
                        ocioData->toLinear.textures[i].sampler,
                        static_cast<int>(1 + i));
                }
                p.ocioToLinearBound = ocioData;
            }
#endif // TLRENDER_OCIO
            return out;
        }

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

#if defined(TLRENDER_OCIO)
        void Render::_ocioError(const std::string& what)
        {
            // Once for each set of options and input, since a failure is
            // remembered and not tried again.
            if (auto logSystem = _logSystem.lock())
            {
                logSystem->print(
                    "tl::gl::Render",
                    ftk::Format("Cannot use the OCIO configuration: {0}").arg(what),
                    ftk::LogType::Error);
            }
        }

        void Render::_ocioErase(const std::string& ocioKey)
        {
            FTK_P();
            // Everything built for one set of options: its data, and the
            // display and to-linear shaders compiled from it. All three are
            // keyed by the options followed by the input color space.
            const std::string suffix = ocioKey + '\n';
            const std::array<std::string, 3> prefixes =
                { "", "display:", "toLinear:" };
            const auto erase = [&suffix, &prefixes](auto& map, size_t prefix)
            {
                const std::string begin = prefixes[prefix] + suffix;
                auto i = map.lower_bound(begin);
                while (i != map.end() &&
                    0 == i->first.compare(0, begin.size(), begin))
                {
                    i = map.erase(i);
                }
            };
            erase(p.ocioData, 0);
            erase(p.shaders, 1);
            erase(p.shaders, 2);
        }
#endif // TLRENDER_OCIO

        void Render::_displayShadersReset()
        {
            FTK_P();
            // The display and to-linear shaders are keyed by the input
            // color space; drop them all.
            auto i = p.shaders.begin();
            while (i != p.shaders.end())
            {
                if (0 == i->first.compare(0, 8, "display:") ||
                    0 == i->first.compare(0, 9, "toLinear:"))
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

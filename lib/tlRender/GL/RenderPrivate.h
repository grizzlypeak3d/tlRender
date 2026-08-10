// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/GL/Render.h>

#include <ftk/GL/Mesh.h>
#include <ftk/GL/OffscreenBuffer.h>
#include <ftk/GL/Render.h>
#include <ftk/GL/Shader.h>
#include <ftk/GL/TextureAtlas.h>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorIO.h>
#endif // TLRENDER_OCIO

#include <list>

#if defined(TLRENDER_OCIO)
namespace OCIO = OCIO_NAMESPACE;
#endif // TLRENDER_OCIO

namespace tl
{
    namespace gl
    {
        std::string vertexSource();
        std::string meshFragmentSource();
        std::string textureFragmentSource();
        std::string displayFragmentSource(
            const std::string& ocioDef,
            const std::string& ocio,
            const std::string& lutDef,
            const std::string& lut,
            LUTOrder);
        std::string dissolveFragmentSource();
        std::string butterflyFragmentSource();
        std::string differenceFragmentSource();

#if defined(TLRENDER_OCIO)
        struct OCIOTexture
        {
            OCIOTexture(
                unsigned    id,
                std::string name,
                std::string sampler,
                unsigned    type);

            unsigned    id = -1;
            std::string name;
            std::string sampler;
            unsigned    type = -1;
        };

        struct OCIOData
        {
            ~OCIOData();

            OCIO::ConstConfigRcPtr config;
            OCIO::DisplayViewTransformRcPtr transform;
            OCIO::LegacyViewingPipelineRcPtr lvp;
            OCIO::ConstProcessorRcPtr processor;
            OCIO::ConstGPUProcessorRcPtr gpuProcessor;
            OCIO::GpuShaderDescRcPtr shaderDesc;
            std::vector<OCIOTexture> textures;
        };

        struct OCIOLUTData
        {
            ~OCIOLUTData();

            OCIO::ConstConfigRcPtr config;
            OCIO::FileTransformRcPtr transform;
            OCIO::ConstProcessorRcPtr processor;
            OCIO::ConstGPUProcessorRcPtr gpuProcessor;
            OCIO::GpuShaderDescRcPtr shaderDesc;
            std::vector<OCIOTexture> textures;
        };
#endif // TLRENDER_OCIO

        struct Render::Private
        {
            std::shared_ptr<ftk::gl::Render> baseRender;

            OCIOOptions ocioOptions;
            LUTOptions lutOptions;

#if defined(TLRENDER_OCIO)
            //! \todo Add a cache for OpenColorIO data.
            std::unique_ptr<OCIOData> ocioData;
            std::unique_ptr<OCIOLUTData> lutData;
#endif // TLRENDER_OCIO

            std::map<std::string, std::shared_ptr<ftk::gl::Shader> > shaders;
            std::map<std::string, std::shared_ptr<ftk::gl::OffscreenBuffer> > buffers;
            std::map<std::string, std::shared_ptr<ftk::gl::VBO> > vbos;
            std::map<std::string, std::shared_ptr<ftk::gl::VAO> > vaos;
        };
    }
}

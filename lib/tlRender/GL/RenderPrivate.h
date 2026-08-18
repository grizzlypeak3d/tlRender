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
            const std::string& toLinearDef,
            const std::string& toLinear,
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

        //! One OCIO processor compiled for the GPU: the shader function
        //! and the textures it samples.
        struct OCIOStage
        {
            ~OCIOStage();

            OCIO::ConstProcessorRcPtr processor;
            OCIO::ConstGPUProcessorRcPtr gpuProcessor;
            OCIO::GpuShaderDescRcPtr shaderDesc;
            std::vector<OCIOTexture> textures;
        };

        struct OCIOData
        {
            OCIO::ConstConfigRcPtr config;
            OCIO::DisplayViewTransformRcPtr transform;
            OCIO::LegacyViewingPipelineRcPtr lvp;
            // The transform is split around the color corrections when the
            // configuration names a scene_linear role, so the corrections
            // apply to linear values rather than to whatever the file's
            // color space is. Without the role, toLinear is empty and the
            // display stage carries the whole transform from the input.
            OCIOStage toLinear;
            OCIOStage display;
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

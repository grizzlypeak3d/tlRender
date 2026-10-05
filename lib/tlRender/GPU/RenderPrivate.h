// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/GPU/Render.h>

#include <tlRender/Timeline/RenderPrivate.h>

#include <ftk/GPU/OffscreenBuffer.h>
#include <ftk/GPU/System.h>
#include <ftk/GPU/Texture.h>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorIO.h>
#endif // TLRENDER_OCIO

#include <SDL3/SDL.h>

#include <list>
#include <map>

#if defined(TLRENDER_OCIO)
namespace OCIO = OCIO_NAMESPACE;
#endif // TLRENDER_OCIO

namespace tl
{
    namespace gpu
    {
        //! \name Shaders
        //! In Metal Shading Language and in GLSL for Vulkan; see
        //! ftk::gpu::ShaderSource.
        ///@{

        //! What a color management stage adds to a shader, in each
        //! language. OpenColorIO writes Metal a function that takes its
        //! textures and samplers as arguments, so the entry point is given
        //! them to pass on; the GLSL it writes declares them itself.
        struct ShaderStage
        {
            std::string mslDef;
            std::string mslArgs;
            std::string mslCall;
            std::string glslDef;
            std::string glslCall;
            //! The stage's textures, in the order of the slots they go in
            //! for the language the device takes.
            std::vector<ftk::gpu::TextureBinding> textures;
        };

        ftk::gpu::ShaderSource textureFragmentSource();
        ftk::gpu::ShaderSource displayFragmentSource(
            const ShaderStage& toLinear,
            const ShaderStage& ocio,
            const ShaderStage& lut,
            LUTOrder);
        ftk::gpu::ShaderSource toLinearFragmentSource(const ShaderStage& toLinear);
        ftk::gpu::ShaderSource dissolveFragmentSource();
        ftk::gpu::ShaderSource clippingWarningFragmentSource();
        ftk::gpu::ShaderSource butterflyFragmentSource();
        ftk::gpu::ShaderSource differenceFragmentSource();
        ftk::gpu::ShaderSource hdrFragmentSource();

        ///@}

        //! \name Uniforms
        //! Laid out as the shaders declare them.
        ///@{

        struct ColorUniforms
        {
            float color[4] = { 1.F, 1.F, 1.F, 1.F };
        };

        struct DisplayUniforms
        {
            float colorMatrix[16];
            float colorAdd[4] = { 0.F, 0.F, 0.F, 0.F };
            float magnifyAxes[2] = { 0.F, 0.F };
            float sourceSize[2] = { 1.F, 1.F };
            int32_t channels = 0;
            int32_t negative = 0;
            int32_t mirrorX = 0;
            int32_t mirrorY = 0;
            int32_t colorEnabled = 0;
            int32_t levelsEnabled = 0;
            int32_t exposureEnabled = 0;
            float exposure = 1.F;
            float softClip = 0.F;
            float levelsInLow = 0.F;
            float levelsInHigh = 1.F;
            float levelsGamma = 1.F;
            float levelsOutLow = 0.F;
            float levelsOutHigh = 1.F;
            float pad[2] = { 0.F, 0.F };
        };

        struct DissolveUniforms
        {
            float dissolve = 0.F;
            float pad[3] = { 0.F, 0.F, 0.F };
        };

        struct ClippingWarningUniforms
        {
            int32_t mode = 0;
            float low = 0.F;
            float high = 1.F;
            float pad = 0.F;
        };

        struct HDRUniforms
        {
            float color[4] = { 1.F, 1.F, 1.F, 1.F };
            int32_t eotf = 0;
            float whiteNits = 203.F;
            float pad[2] = { 0.F, 0.F };
        };

        struct DifferenceUniforms
        {
            float gain = 1.F;
            float pad[3] = { 0.F, 0.F, 0.F };
        };

        ///@}

#if defined(TLRENDER_OCIO)
        struct OCIOTexture
        {
            SDL_GPUTexture* texture = nullptr;
            SDL_GPUSampler* sampler = nullptr;
            std::string name;
            std::string samplerName;
            //! A table the shader had as a constant array; see
            //! OCIOStage::arrays.
            bool array = false;
        };

        //! A constant array of floats in the GLSL OpenColorIO writes.
        struct OCIOArray
        {
            std::string name;
            std::vector<float> values;
        };

        //! One OCIO processor compiled for the GPU: the shader function
        //! and the textures it samples.
        struct OCIOStage
        {
            ~OCIOStage();

            SDL_GPUDevice* device = nullptr;
            //! Whether the device filters thirty-two bit float textures,
            //! which is what the tables are kept as where it does.
            bool floatFilter = true;
            std::string functionName;
            std::string resourcePrefix;
            OCIO::ConstProcessorRcPtr processor;
            OCIO::ConstGPUProcessorRcPtr gpuProcessor;
            OCIO::GpuShaderDescRcPtr shaderDesc;
            std::vector<OCIOTexture> textures;
            //! The constant arrays in the shader's GLSL, which are taken
            //! out of it: each is a texture read by index instead, the last
            //! of the stage's textures where the device takes GLSL. A
            //! shader that indexes a constant array is as OpenColorIO
            //! writes the ACES 2 transforms, with a table of 363 hues that
            //! is searched, and Vulkan on a Raspberry Pi took ten seconds
            //! to compile it and a second to draw with it, where OpenGL ES
            //! there, which keeps such an array as a uniform, took none.
            std::vector<OCIOArray> arrays;
        };

        struct OCIOData
        {
            OCIO::ConstConfigRcPtr config;
            OCIO::DisplayViewTransformRcPtr transform;
            OCIO::LegacyViewingPipelineRcPtr lvp;
            OCIOStage toLinear;
            OCIOStage display;
        };

        struct OCIOLUTData
        {
            OCIO::ConstConfigRcPtr config;
            OCIO::FileTransformRcPtr transform;
            OCIOStage stage;
        };
#endif // TLRENDER_OCIO

        struct Render::Private
        {
            std::shared_ptr<ftk::gpu::System> system;
            std::shared_ptr<ftk::gpu::Render> baseRender;

            OCIOOptions ocioOptions;
            LUTOptions lutOptions;

#if defined(TLRENDER_OCIO)
            // See tl::gl::Render, which these are kept as.
            std::map<std::string, std::shared_ptr<OCIOData> > ocioData;
            std::string ocioKey;
            std::list<std::string> ocioKeys;
            std::shared_ptr<OCIOData> ocioDataBound;
            std::function<std::string(
                const std::string&,
                const ftk::ImageTags&)> ocioInputResolver;
            std::map<std::string, std::string> ocioInputCache;
            std::unique_ptr<OCIOLUTData> lutData;
#endif // TLRENDER_OCIO

            // The display and to-linear shaders that have been given to
            // the base renderer, by name, each with the textures its color
            // management samples: what is bound after the picture.
            std::map<std::string, std::vector<ftk::gpu::TextureBinding> > displayShaders;
            std::map<std::string, std::shared_ptr<ftk::gpu::OffscreenBuffer> > buffers;
        };
    }
}

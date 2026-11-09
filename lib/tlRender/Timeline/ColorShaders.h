// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/DisplayOptions.h>
#include <tlRender/Timeline/Export.h>
#include <tlRender/Timeline/RenderPrivate.h>

#include <string>

namespace tl
{
    //! \name Renderers
    //! The color a picture is drawn through, as shader source the OpenGL
    //! and GPU renderers both build their shaders from: written once, in
    //! the GLSL that Metal also reads once it has been given GLSL's type
    //! names, so that the renderers cannot draw it differently.
    ///@{

    //! The languages the renderers' shaders are written in.
    enum class ShaderLanguage
    {
        //! OpenGL's: GLSL 4.1, or OpenGL ES's 3.0.
        GLSL,
        //! Vulkan's: GLSL 4.5, compiled to SPIR-V.
        GLSL_VK,
        //! Metal's.
        MSL
    };

    //! Get the color functions, for a shader in the given language:
    //!
    //! - vec4 sampleTexture(TEXTURE, vec2 uv): the texture at a point.
    //! - vec4 sampleMitchell(TEXTURE, vec2 uv, vec2 size, vec2 axes): the
    //!   texture enlarged with the Mitchell-Netravali kernel along the axes
    //!   that are, of a texture so many texels in size.
    //! - vec4 applyColorMatrix(vec4, vec3 add, mat4)
    //! - vec4 applyLevels(vec4, float inLow, float inHigh, float gamma,
    //!   float outLow, float outHigh)
    //! - vec4 applySoftClip(vec4, float)
    //! - vec4 applyChannels(vec4, int): ftk::ChannelDisplay.
    //! - vec3 srgbToLinear(vec3), vec3 linearToSRGB(vec3): the sRGB curve,
    //!   carried on past one and mirrored below zero.
    //! - vec3 displayP3ToSRGB(vec3): Display P3 into extended sRGB.
    //! - vec3 pqToNits(vec3): SMPTE ST 2084.
    //! - vec3 rec2020ToRec709(vec3): linear light, by the primaries.
    //!
    //! TEXTURE is the texture parameters of the language: a sampler2D in
    //! GLSL, a texture2d<float> and a sampler in Metal. For Metal it starts
    //! with GLSL's vector and matrix type names, which the functions, and
    //! the body below, are written with.
    TL_TIMELINE_API std::string getColorShaderFunctions(ShaderLanguage);

    //! The display options as the display shader takes them, and as
    //! CPUColor applies them: what is turned on, and the values in the form
    //! the arithmetic uses -- the levels' gamma inverted, the exposure as a
    //! factor -- so that the renderers and the CPU cannot work them out
    //! differently.
    struct TL_TIMELINE_API_TYPE DisplayColorValues
    {
        int       channels        = 0;
        bool      negative        = false;
        bool      colorEnabled    = false;
        ftk::V3F  colorAdd;
        ftk::M44F colorMatrix;
        bool      levelsEnabled   = false;
        float     levelsInLow     = 0.F;
        float     levelsInHigh    = 1.F;
        float     levelsGamma     = 1.F;
        float     levelsOutLow    = 0.F;
        float     levelsOutHigh   = 1.F;
        bool      exposureEnabled = false;
        float     exposure        = 1.F;
        float     softClip        = 0.F;
    };

    //! Get the display options as the display shader takes them.
    TL_TIMELINE_API DisplayColorValues getDisplayColorValues(const DisplayOptions&);

    //! What the display shader's body reads, in its language.
    struct TL_TIMELINE_API_TYPE DisplayShaderInputs
    {
        //! What a uniform's name is prefixed with: "u." for a uniform
        //! block, nothing for OpenGL's separate uniforms.
        std::string uniforms;

        //! The texture arguments of sampleTexture() and sampleMitchell().
        std::string texture;

        //! The texture coordinates.
        std::string uv;

        //! Whether there is a displayP3 uniform: a Display P3 display's
        //! pixels taken into a window's extended sRGB here, rather than in
        //! a pass of their own.
        bool displayP3 = false;
    };

    //! Get the display shader's body: the picture sampled, mirrored and
    //! enlarged, and drawn through the color stages and the corrections to
    //! the channels display, left in outColor, which the caller declares.
    //! The uniforms are those of DisplayShaderInputs and:
    //!
    //! - vec2 magnifyAxes, vec2 sourceSize
    //! - int channels, negative, mirrorX, mirrorY
    //! - int colorEnabled; vec3 colorAdd (or a vec4 of which the rgb);
    //!   mat4 colorMatrix
    //! - int levelsEnabled; float levelsInLow, levelsInHigh, levelsGamma,
    //!   levelsOutLow, levelsOutHigh
    //! - int exposureEnabled; float exposure, softClip
    //! - int displayP3, when there is one
    TL_TIMELINE_API std::string getDisplayShaderBody(
        const DisplayShaderInputs&,
        const ColorStageCalls&);

    ///@}
}

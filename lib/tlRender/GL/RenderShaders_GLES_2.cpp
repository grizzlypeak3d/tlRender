// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GL/RenderPrivate.h>

#include <ftk/Core/Format.h>

namespace tl
{
    namespace gl
    {
        std::string vertexSource()
        {
            return
                "precision mediump float;\n"
                "\n"
                "attribute vec3 vPos;\n"
                "attribute vec2 vTexture;\n"
                "varying vec2 fTexture;\n"
                "\n"
                "struct Transform\n"
                "{\n"
                "    mat4 mvp;\n"
                "};\n"
                "\n"
                "uniform Transform transform;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    gl_Position = transform.mvp * vec4(vPos, 1.0);\n"
                "    fTexture = vTexture;\n"
                "}\n";
        }

        std::string meshFragmentSource()
        {
            return
                "precision mediump float;\n"
                "\n"
                "uniform vec4 color;\n"
                "\n"
                "void main()\n"
                "{\n"
                "\n"
                "    gl_FragColor = color;\n"
                "}\n";
        }

        std::string textureFragmentSource()
        {
            return
                "precision mediump float;\n"
                "\n"
                "varying vec2 fTexture;\n"
                "\n"
                "uniform vec4 color;\n"
                "uniform sampler2D textureSampler;\n"
                "\n"
                "void main()\n"
                "{\n"
                "\n"
                "    gl_FragColor = texture2D(textureSampler, fTexture) * color;\n"
                "}\n";
        }

        namespace
        {
            const std::string videoLevels =
                "// enum tl::image::VideoLevels\n"
                "const int VideoLevels_FullRange  = 0;\n"
                "const int VideoLevels_LegalRange = 1;\n";
        }

        std::string displayFragmentSource(
            const std::string& toLinearDef,
            const std::string& toLinear,
            const std::string& colorConfigDef,
            const std::string& colorConfig,
            const std::string& lutDef,
            const std::string& lut,
            LUTOrder lutOrder)
        {
             return ftk::Format(
                "precision mediump float;\n"
                "\n"
                "varying vec2 fTexture;\n"
                "\n"
                "// enum tl::ftk::ChannelDisplay\n"
                "const int Channels_Color = 0;\n"
                "const int Channels_Red   = 1;\n"
                "const int Channels_Green = 2;\n"
                "const int Channels_Blue  = 3;\n"
                "const int Channels_Alpha = 4;\n"
                "\n"
                "struct Levels\n"
                "{\n"
                "    float inLow;\n"
                "    float inHigh;\n"
                "    float gamma;\n"
                "    float outLow;\n"
                "    float outHigh;\n"
                "};\n"
                "\n"
                "struct Exposure\n"
                "{\n"
                "    float v;\n"
                "    float d;\n"
                "    float k;\n"
                "    float f;\n"
                "    float g;\n"
                "};\n"
                "\n"
                "{0}\n"
                "\n"
                "uniform sampler2D textureSampler;\n"
                "\n"
                "uniform int      channels;\n"
                "uniform bool     negative;\n"
                "uniform int      mirrorX;\n"
                "uniform int      mirrorY;\n"
                "uniform bool     colorEnabled;\n"
                "uniform vec3     colorAdd;\n"
                "uniform mat4     colorMatrix;\n"
                "uniform bool     levelsEnabled;\n"
                "uniform Levels   levels;\n"
                "uniform bool     exposureEnabled;\n"
                "uniform Exposure exposure;\n"
                "uniform float    softClip;\n"
                "uniform int      videoLevels;\n"
                "\n"
                "vec4 colorFunc(vec4 value, vec3 add, mat4 m)\n"
                "{\n"
                "    vec4 tmp;\n"
                "    tmp[0] = value[0] + add[0];\n"
                "    tmp[1] = value[1] + add[1];\n"
                "    tmp[2] = value[2] + add[2];\n"
                "    tmp[3] = 1.0;\n"
                "    tmp *= m;\n"
                "    tmp[3] = value[3];\n"
                "    return tmp;\n"
                "}\n"
                "\n"
                "vec4 levelsFunc(vec4 value, Levels data)\n"
                "{\n"
                "    vec4 tmp;\n"
                "    tmp[0] = (value[0] - data.inLow) / data.inHigh;\n"
                "    tmp[1] = (value[1] - data.inLow) / data.inHigh;\n"
                "    tmp[2] = (value[2] - data.inLow) / data.inHigh;\n"
                "    if (tmp[0] >= 0.0)\n"
                "        tmp[0] = pow(tmp[0], data.gamma);\n"
                "    if (tmp[1] >= 0.0)\n"
                "        tmp[1] = pow(tmp[1], data.gamma);\n"
                "    if (tmp[2] >= 0.0)\n"
                "        tmp[2] = pow(tmp[2], data.gamma);\n"
                "    value[0] = tmp[0] * data.outHigh + data.outLow;\n"
                "    value[1] = tmp[1] * data.outHigh + data.outLow;\n"
                "    value[2] = tmp[2] * data.outHigh + data.outLow;\n"
                "    return value;\n"
                "}\n"
                "\n"
                "vec4 softClipFunc(vec4 value, float softClip)\n"
                "{\n"
                "    float tmp = 1.0 - softClip;\n"
                "    if (value[0] > tmp)\n"
                "        value[0] = tmp + (1.0 - exp(-(value[0] - tmp) / softClip)) * softClip;\n"
                "    if (value[1] > tmp)\n"
                "        value[1] = tmp + (1.0 - exp(-(value[1] - tmp) / softClip)) * softClip;\n"
                "    if (value[2] > tmp)\n"
                "        value[2] = tmp + (1.0 - exp(-(value[2] - tmp) / softClip)) * softClip;\n"
                "    return value;\n"
                "}\n"
                "\n"
                "float knee(float value, float f)\n"
                "{\n"
                "    return log(value * f + 1.0) / f;\n"
                "}\n"
                "\n"
                "vec4 exposureFunc(vec4 value, Exposure data)\n"
                "{\n"
                "    value[0] = max(0.0, value[0] - data.d) * data.v;\n"
                "    value[1] = max(0.0, value[1] - data.d) * data.v;\n"
                "    value[2] = max(0.0, value[2] - data.d) * data.v;\n"
                "    if (value[0] > data.k)\n"
                "        value[0] = data.k + knee(value[0] - data.k, data.f);\n"
                "    if (value[1] > data.k)\n"
                "        value[1] = data.k + knee(value[1] - data.k, data.f);\n"
                "    if (value[2] > data.k)\n"
                "        value[2] = data.k + knee(value[2] - data.k, data.f);\n"
                "    if (value[0] > 0.0) value[0] = pow(value[0], data.g);\n"
                "    if (value[1] > 0.0) value[1] = pow(value[1], data.g);\n"
                "    if (value[2] > 0.0) value[2] = pow(value[2], data.g);\n"
                "    float s = pow(2.0, -3.5 * data.g);\n"
                "    value[0] *= s;\n"
                "    value[1] *= s;\n"
                "    value[2] *= s;\n"
                "    return value;\n"
                "}\n"
                "\n"
                "void main()\n"
                "{\n"
                "    vec2 t = fTexture;\n"
                "    if (1 == mirrorX)\n"
                "    {\n"
                "        t.x = 1.0 - t.x;\n"
                "    }\n"
                "    if (1 == mirrorY)\n"
                "    {\n"
                "        t.y = 1.0 - t.y;\n"
                "    }\n"
                "\n"
                "    gl_FragColor = texture2D(textureSampler, t);\n"
                "\n"
                "    if (negative)\n"
                "    {\n"
                "        gl_FragColor.r = 1.0 - gl_FragColor.r;\n"
                "        gl_FragColor.g = 1.0 - gl_FragColor.g;\n"
                "        gl_FragColor.b = 1.0 - gl_FragColor.b;\n"
                "    }\n"
                "\n"
                "    // Video levels.\n"
                "    if (VideoLevels_LegalRange == videoLevels)\n"
                "    {\n"
                "        const float scale = (940.0 - 64.0) / 1023.0;\n"
                "        const float offset = 64.0 / 1023.0;\n"
                "        gl_FragColor.r = gl_FragColor.r * scale + offset;\n"
                "        gl_FragColor.g = gl_FragColor.g * scale + offset;\n"
                "        gl_FragColor.b = gl_FragColor.b * scale + offset;\n"
                "    }\n"
                "\n"
                "    // Apply color transformations.\n"
                "    if (colorEnabled)\n"
                "    {\n"
                "        gl_FragColor = colorFunc(gl_FragColor, colorAdd, colorMatrix);\n"
                "    }\n"
                "    if (levelsEnabled)\n"
                "    {\n"
                "        gl_FragColor = levelsFunc(gl_FragColor, levels);\n"
                "    }\n"
                "    if (exposureEnabled)\n"
                "    {\n"
                "        gl_FragColor = exposureFunc(gl_FragColor, exposure);\n"
                "    }\n"
                "    if (softClip > 0.0)\n"
                "    {\n"
                "        gl_FragColor = softClipFunc(gl_FragColor, softClip);\n"
                "    }\n"
                "\n"
                "    // Swizzle for the channels display.\n"
                "    if (Channels_Red == channels)\n"
                "    {\n"
                "        gl_FragColor.g = gl_FragColor.r;\n"
                "        gl_FragColor.b = gl_FragColor.r;\n"
                "    }\n"
                "    else if (Channels_Green == channels)\n"
                "    {\n"
                "        gl_FragColor.r = gl_FragColor.g;\n"
                "        gl_FragColor.b = gl_FragColor.g;\n"
                "    }\n"
                "    else if (Channels_Blue == channels)\n"
                "    {\n"
                "        gl_FragColor.r = gl_FragColor.b;\n"
                "        gl_FragColor.g = gl_FragColor.b;\n"
                "    }\n"
                "    else if (Channels_Alpha == channels)\n"
                "    {\n"
                "        gl_FragColor.r = gl_FragColor.a;\n"
                "        gl_FragColor.g = gl_FragColor.a;\n"
                "        gl_FragColor.b = gl_FragColor.a;\n"
                "    }\n"
                "}\n").
                arg(videoLevels);
        }

        std::string dissolveFragmentSource()
        {
            return
                "precision mediump float;\n"
                "\n"
                "varying vec2 fTexture;\n"
                "\n"
                "uniform float     dissolve;\n"
                "uniform sampler2D textureSampler;\n"
                "uniform sampler2D textureSampler2;\n"
                "\n"
                "void main()\n"
                "{\n"
                "\n"
                "    vec4 c = texture2D(textureSampler, fTexture);\n"
                "    vec4 c2 = texture2D(textureSampler2, fTexture);\n"
                "    float idissolve = 1.0 - dissolve;\n"
                "    gl_FragColor.r = c.r * idissolve + c2.r * dissolve;\n"
                "    gl_FragColor.g = c.g * idissolve + c2.g * dissolve;\n"
                "    gl_FragColor.b = c.b * idissolve + c2.b * dissolve;\n"
                "    gl_FragColor.a = c.a * idissolve + c2.a * dissolve;\n"
                "}\n";
        }

        std::string butterflyFragmentSource()
        {
            return
                "precision mediump float;\n"
                "\n"
                "varying vec2 fTexture;\n"
                "\n"
                "uniform sampler2D textureSampler;\n"
                "uniform sampler2D textureSamplerB;\n"
                "\n"
                "void main()\n"
                "{\n"
                // The same half of both, the second one mirrored, so that
                // the middle of the picture is on both sides of the seam.
                "    if (fTexture.x < .5)\n"
                "    {\n"
                "        gl_FragColor = texture2D(textureSampler, fTexture);\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        gl_FragColor = texture2D(\n"
                "            textureSamplerB,\n"
                "            vec2(1.0 - fTexture.x, fTexture.y));\n"
                "    }\n"
                "}\n";
        }

        std::string differenceFragmentSource()
        {
            return
                "precision mediump float;\n"
                "\n"
                "varying vec2 fTexture;\n"
                "\n"
                "uniform sampler2D textureSampler;\n"
                "uniform sampler2D textureSamplerB;\n"
                "uniform float     gain;\n"
                "\n"
                "void main()\n"
                "{\n"
                "\n"
                "    vec4 c = texture2D(textureSampler, fTexture);\n"
                "    vec4 cB = texture2D(textureSamplerB, fTexture);\n"
                "    gl_FragColor.r = abs(c.r - cB.r) * gain;\n"
                "    gl_FragColor.g = abs(c.g - cB.g) * gain;\n"
                "    gl_FragColor.b = abs(c.b - cB.b) * gain;\n"
                "    gl_FragColor.a = max(c.a, cB.a);\n"
                "}\n";
        }
    }
}


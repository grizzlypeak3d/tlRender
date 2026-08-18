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
                "#version 410\n"
                "\n"
                "in vec3 vPos;\n"
                "in vec2 vTexture;\n"
                "out vec2 fTexture;\n"
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
                "#version 410\n"
                "\n"
                "out vec4 outColor;\n"
                "\n"
                "uniform vec4 color;\n"
                "\n"
                "void main()\n"
                "{\n"
                "\n"
                "    outColor = color;\n"
                "}\n";
        }

        std::string textureFragmentSource()
        {
            return
                "#version 410\n"
                "\n"
                "in vec2 fTexture;\n"
                "out vec4 outColor;\n"
                "\n"
                "uniform vec4 color;\n"
                "uniform sampler2D textureSampler;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    outColor = texture(textureSampler, fTexture) * color;\n"
                "}\n";
        }

        std::string displayFragmentSource(
            const std::string& toLinearDef,
            const std::string& toLinear,
            const std::string& ocioDef,
            const std::string& ocio,
            const std::string& lutDef,
            const std::string& lut,
            LUTOrder lutOrder)
        {
            std::vector<std::string> args;
            args.push_back(toLinearDef);
            args.push_back(ocioDef);
            args.push_back(lutDef);
            // What runs before the color corrections and what runs after:
            // the transform to linear comes before so the corrections
            // operate on linear values, the display transform after, and
            // the LUT is on whichever side of the color management it was
            // asked for.
            std::string before;
            std::string after;
            switch (lutOrder)
            {
            case LUTOrder::PreConfig:
                before = lut + "\n    " + toLinear;
                after = ocio;
                break;
            case LUTOrder::PostConfig:
                before = toLinear;
                after = ocio + "\n    " + lut;
                break;
            default: break;
            }
            args.push_back(before);
            args.push_back(after);
            return ftk::Format(
                "#version 410\n"
                "\n"
                "in vec2 fTexture;\n"
                "out vec4 outColor;\n"
                "\n"
                "// enum tl::ftk::ChannelDisplay\n"
                "const uint Channels_Color = 0;\n"
                "const uint Channels_Red   = 1;\n"
                "const uint Channels_Green = 2;\n"
                "const uint Channels_Blue  = 3;\n"
                "const uint Channels_Alpha = 4;\n"
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
                "uniform sampler2D textureSampler;\n"
                "uniform bool      magnifyHighQuality;\n"
                "uniform vec2      textureSize;\n"
                "\n"
                "// Mitchell-Netravali with B = C = 1/3, the kernel the two\n"
                "// pass resample enlarges with. Sixteen taps evaluated here\n"
                "// rather than weighed from a table: enlarging reaches only\n"
                "// two texels either side however far it is taken, so there\n"
                "// is nothing to gain from an intermediate, and at a large\n"
                "// zoom an intermediate the size of the screen would be\n"
                "// enormous.\n"
                "float mitchell(float x)\n"
                "{\n"
                "    float b = 1.0 / 3.0;\n"
                "    float c = 1.0 / 3.0;\n"
                "    x = abs(x);\n"
                "    float x2 = x * x;\n"
                "    float x3 = x2 * x;\n"
                "    if (x < 1.0)\n"
                "    {\n"
                "        return ((12.0 - 9.0 * b - 6.0 * c) * x3 +\n"
                "            (-18.0 + 12.0 * b + 6.0 * c) * x2 +\n"
                "            (6.0 - 2.0 * b)) / 6.0;\n"
                "    }\n"
                "    if (x < 2.0)\n"
                "    {\n"
                "        return ((-b - 6.0 * c) * x3 +\n"
                "            (6.0 * b + 30.0 * c) * x2 +\n"
                "            (-12.0 * b - 48.0 * c) * x +\n"
                "            (8.0 * b + 24.0 * c)) / 6.0;\n"
                "    }\n"
                "    return 0.0;\n"
                "}\n"
                "\n"
                "vec4 sampleMitchell(sampler2D s, vec2 uv, vec2 size)\n"
                "{\n"
                "    vec2 pos = uv * size - 0.5;\n"
                "    vec2 base = floor(pos);\n"
                "    vec2 f = pos - base;\n"
                "    vec4 c = vec4(0.0);\n"
                "    float total = 0.0;\n"
                "    for (int j = -1; j <= 2; ++j)\n"
                "    {\n"
                "        for (int i = -1; i <= 2; ++i)\n"
                "        {\n"
                "            float w = mitchell(float(i) - f.x) * mitchell(float(j) - f.y);\n"
                "            vec2 t = (base + vec2(float(i), float(j)) + 0.5) / size;\n"
                "            c += w * texture(s, clamp(t, vec2(0.0), vec2(1.0)));\n"
                "            total += w;\n"
                "        }\n"
                "    }\n"
                "    // Normalised for the reason the tables are: the taps are\n"
                "    // a finite sample of a kernel whose integral is one.\n"
                "    return total > 0.0 ? c / total : c;\n"
                "}\n"
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
                "\n"
                "vec4 colorFunc(vec4 value, vec3 add, mat4 m)\n"
                "{\n"
                "    vec4 tmp;\n"
                "    tmp[0] = value[0] + add[0];\n"
                "    tmp[1] = value[1] + add[1];\n"
                "    tmp[2] = value[2] + add[2];\n"
                "    tmp[3] = 1.0;\n"
                "    tmp = m * tmp;\n"
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
                "    float s = pow(2, -3.5 * data.g);\n"
                "    value[0] *= s;\n"
                "    value[1] *= s;\n"
                "    value[2] *= s;\n"
                "    return value;\n"
                "}\n"
                "\n"
                "{0}\n"
                "\n"
                "{1}\n"
                "\n"
                "{2}\n"
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
                "    outColor = magnifyHighQuality ?\n"
                "        sampleMitchell(textureSampler, t, textureSize) :\n"
                "        texture(textureSampler, t);\n"
                "\n"
                "    if (negative)\n"
                "    {\n"
                "        outColor.r = 1.0 - outColor.r;\n"
                "        outColor.g = 1.0 - outColor.g;\n"
                "        outColor.b = 1.0 - outColor.b;\n"
                "    }\n"
                "\n"
                "    // To linear, so the color transformations operate on\n"
                "    // linear values.\n"
                "    {3}\n"
                "\n"
                "    // Apply color transformations.\n"
                "    if (colorEnabled)\n"
                "    {\n"
                "        outColor = colorFunc(outColor, colorAdd, colorMatrix);\n"
                "    }\n"
                "    if (levelsEnabled)\n"
                "    {\n"
                "        outColor = levelsFunc(outColor, levels);\n"
                "    }\n"
                "    if (exposureEnabled)\n"
                "    {\n"
                "        outColor = exposureFunc(outColor, exposure);\n"
                "    }\n"
                "    if (softClip > 0.0)\n"
                "    {\n"
                "        outColor = softClipFunc(outColor, softClip);\n"
                "    }\n"
                "\n"
                "    // Apply color management.\n"
                "    {4}\n"
                "\n"
                "    // Swizzle for the channels display.\n"
                "    if (Channels_Red == channels)\n"
                "    {\n"
                "        outColor.g = outColor.r;\n"
                "        outColor.b = outColor.r;\n"
                "    }\n"
                "    else if (Channels_Green == channels)\n"
                "    {\n"
                "        outColor.r = outColor.g;\n"
                "        outColor.b = outColor.g;\n"
                "    }\n"
                "    else if (Channels_Blue == channels)\n"
                "    {\n"
                "        outColor.r = outColor.b;\n"
                "        outColor.g = outColor.b;\n"
                "    }\n"
                "    else if (Channels_Alpha == channels)\n"
                "    {\n"
                "        outColor.r = outColor.a;\n"
                "        outColor.g = outColor.a;\n"
                "        outColor.b = outColor.a;\n"
                "    }\n"
                "}\n").
                arg(args[0]).
                arg(args[1]).
                arg(args[2]).
                arg(args[3]).
                arg(args[4]);
        }

        std::string dissolveFragmentSource()
        {
            return
                "#version 410\n"
                "\n"
                "in vec2 fTexture;\n"
                "out vec4 outColor;\n"
                "\n"
                "uniform float     dissolve;\n"
                "uniform sampler2D textureSampler;\n"
                "uniform sampler2D textureSampler2;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    vec4 c = texture(textureSampler, fTexture);\n"
                "    vec4 c2 = texture(textureSampler2, fTexture);\n"
                "    float idissolve = 1.0 - dissolve;\n"
                "    outColor.r = c.r * idissolve + c2.r * dissolve;\n"
                "    outColor.g = c.g * idissolve + c2.g * dissolve;\n"
                "    outColor.b = c.b * idissolve + c2.b * dissolve;\n"
                "    outColor.a = c.a * idissolve + c2.a * dissolve;\n"
                "}\n";
        }

        std::string butterflyFragmentSource()
        {
            return
                "#version 410\n"
                "\n"
                "in vec2 fTexture;\n"
                "out vec4 outColor;\n"
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
                "        outColor = texture(textureSampler, fTexture);\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        outColor = texture(\n"
                "            textureSamplerB,\n"
                "            vec2(1.0 - fTexture.x, fTexture.y));\n"
                "    }\n"
                "}\n";
        }

        std::string differenceFragmentSource()
        {
            return
                "#version 410\n"
                "\n"
                "in vec2 fTexture;\n"
                "out vec4 outColor;\n"
                "\n"
                "uniform sampler2D textureSampler;\n"
                "uniform sampler2D textureSamplerB;\n"
                "uniform float     gain;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    vec4 c = texture(textureSampler, fTexture);\n"
                "    vec4 cB = texture(textureSamplerB, fTexture);\n"
                "    outColor.r = abs(c.r - cB.r) * gain;\n"
                "    outColor.g = abs(c.g - cB.g) * gain;\n"
                "    outColor.b = abs(c.b - cB.b) * gain;\n"
                "    outColor.a = max(c.a, cB.a);\n"
                "}\n";
        }
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/ColorShaders.h>

#include <cmath>

namespace tl
{
    DisplayColorValues getDisplayColorValues(const DisplayOptions& options)
    {
        DisplayColorValues out;
        out.channels = static_cast<int>(options.channels);
        out.negative = options.negative;
        // A matrix that does nothing is not applied: it is not quite
        // nothing in the arithmetic.
        out.colorEnabled = options.color.enabled && options.color != Color();
        out.colorAdd = options.color.add;
        if (out.colorEnabled)
        {
            out.colorMatrix = color(options.color);
        }
        out.levelsEnabled = options.levels.enabled;
        out.levelsInLow = options.levels.inLow;
        out.levelsInHigh = options.levels.inHigh;
        out.levelsGamma = options.levels.gamma > 0.F ?
            (1.F / options.levels.gamma) :
            1000000.F;
        out.levelsOutLow = options.levels.outLow;
        out.levelsOutHigh = options.levels.outHigh;
        out.exposureEnabled = options.exposure.enabled;
        out.exposure = options.exposure.enabled ?
            std::pow(2.F, options.exposure.exposure) :
            1.F;
        out.softClip = options.softClip.enabled ? options.softClip.value : 0.F;
        return out;
    }

    std::string getColorShaderFunctions(ShaderLanguage language)
    {
        std::string out;
        std::string textureParams;
        std::string textureArgs;
        std::string textureSample;
        switch (language)
        {
        case ShaderLanguage::MSL:
            // Metal's names for GLSL's types: what follows is written
            // with GLSL's, and otherwise reads the same in both.
            out +=
                "typedef float2 vec2;\n"
                "typedef float3 vec3;\n"
                "typedef float4 vec4;\n"
                "typedef float4x4 mat4;\n"
                "\n";
            textureParams = "texture2d<float> t, sampler s";
            textureArgs = "t, s";
            textureSample = "t.sample(s, uv)";
            break;
        default:
            textureParams = "sampler2D s";
            textureArgs = "s";
            textureSample = "texture(s, uv)";
            break;
        }
        out +=
            "vec4 sampleTexture(" + textureParams + ", vec2 uv)\n"
            "{\n"
            "    return " + textureSample + ";\n"
            "}\n"
            "\n"
            // Mitchell-Netravali with B = C = 1/3, the kernel the two pass
            // resample enlarges with. Sixteen taps evaluated here rather
            // than weighed from a table: enlarging reaches only two texels
            // either side however far it is taken, so there is nothing to
            // gain from an intermediate, and at a large zoom an
            // intermediate the size of the screen would be enormous.
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
            // The weight along one axis: the kernel where that axis is
            // enlarged, and otherwise the nearest texel alone, which at the
            // one to one size the picture is then drawn at is the texel
            // itself.
            "float axisWeight(float x, float enlarged)\n"
            "{\n"
            "    return enlarged > 0.5 ? mitchell(x) : (abs(x) <= 0.5 ? 1.0 : 0.0);\n"
            "}\n"
            "\n"
            "vec4 sampleMitchell(" + textureParams + ", vec2 uv, vec2 size, vec2 axes)\n"
            "{\n"
            "    vec2 pos = uv * size - 0.5;\n"
            "    vec2 base = floor(pos);\n"
            "    vec2 f = pos - base;\n"
            "    vec4 c = vec4(0.0);\n"
            "    float total = 0.0;\n"
            "    vec4 lo = vec4(1.0e38);\n"
            "    vec4 hi = vec4(-1.0e38);\n"
            "    for (int j = -1; j <= 2; ++j)\n"
            "    {\n"
            "        for (int i = -1; i <= 2; ++i)\n"
            "        {\n"
            "            float w = axisWeight(float(i) - f.x, axes.x) *\n"
            "                axisWeight(float(j) - f.y, axes.y);\n"
            "            vec2 tc = (base + vec2(float(i), float(j)) + 0.5) / size;\n"
            "            vec4 texel = sampleTexture(" + textureArgs + ", clamp(tc, vec2(0.0), vec2(1.0)));\n"
            "            c += w * texel;\n"
            "            total += w;\n"
            "            if (i >= 0 && i <= 1 && j >= 0 && j <= 1)\n"
            "            {\n"
            "                lo = min(lo, texel);\n"
            "                hi = max(hi, texel);\n"
            "            }\n"
            "        }\n"
            "    }\n"
            // Normalised for the reason the tables are: the taps are a
            // finite sample of a kernel whose integral is one.
            "    c = total > 0.0 ? c / total : c;\n"
            // Held to the four texels around the sample. The kernel's
            // negative lobes overshoot a hard edge -- a ring below black and
            // above white that the picture does not have, and that the
            // clipping warning then reports.
            "    return clamp(c, lo, hi);\n"
            "}\n"
            "\n"
            "vec4 applyColorMatrix(vec4 value, vec3 add, mat4 m)\n"
            "{\n"
            "    vec4 tmp = m * vec4(value.rgb + add, 1.0);\n"
            "    tmp.a = value.a;\n"
            "    return tmp;\n"
            "}\n"
            "\n"
            "float levelsChannel(float value, float inLow, float inHigh, float gamma, float outLow, float outHigh)\n"
            "{\n"
            "    float tmp = (value - inLow) / inHigh;\n"
            "    if (tmp >= 0.0)\n"
            "    {\n"
            "        tmp = pow(tmp, gamma);\n"
            "    }\n"
            "    return tmp * outHigh + outLow;\n"
            "}\n"
            "\n"
            "vec4 applyLevels(vec4 value, float inLow, float inHigh, float gamma, float outLow, float outHigh)\n"
            "{\n"
            "    value.r = levelsChannel(value.r, inLow, inHigh, gamma, outLow, outHigh);\n"
            "    value.g = levelsChannel(value.g, inLow, inHigh, gamma, outLow, outHigh);\n"
            "    value.b = levelsChannel(value.b, inLow, inHigh, gamma, outLow, outHigh);\n"
            "    return value;\n"
            "}\n"
            "\n"
            "float softClipChannel(float value, float amount)\n"
            "{\n"
            "    float tmp = 1.0 - amount;\n"
            "    if (value > tmp)\n"
            "    {\n"
            "        value = tmp + (1.0 - exp(-(value - tmp) / amount)) * amount;\n"
            "    }\n"
            "    return value;\n"
            "}\n"
            "\n"
            "vec4 applySoftClip(vec4 value, float amount)\n"
            "{\n"
            "    value.r = softClipChannel(value.r, amount);\n"
            "    value.g = softClipChannel(value.g, amount);\n"
            "    value.b = softClipChannel(value.b, amount);\n"
            "    return value;\n"
            "}\n"
            "\n"
            // ftk::ChannelDisplay: color, red, green, blue, alpha.
            "vec4 applyChannels(vec4 value, int mode)\n"
            "{\n"
            "    if (1 == mode)\n"
            "    {\n"
            "        value.g = value.r;\n"
            "        value.b = value.r;\n"
            "    }\n"
            "    else if (2 == mode)\n"
            "    {\n"
            "        value.r = value.g;\n"
            "        value.b = value.g;\n"
            "    }\n"
            "    else if (3 == mode)\n"
            "    {\n"
            "        value.r = value.b;\n"
            "        value.g = value.b;\n"
            "    }\n"
            "    else if (4 == mode)\n"
            "    {\n"
            "        value.r = value.a;\n"
            "        value.g = value.a;\n"
            "        value.b = value.a;\n"
            "    }\n"
            "    return value;\n"
            "}\n"
            "\n"
            // The choice between the two pieces of the curve by step(),
            // which both languages have, rather than by a vector of
            // booleans, which they write differently.
            "vec3 srgbToLinear(vec3 v)\n"
            "{\n"
            "    vec3 a = abs(v);\n"
            "    return sign(v) * mix(\n"
            "        pow((a + 0.055) / 1.055, vec3(2.4)), a / 12.92, step(a, vec3(0.04045)));\n"
            "}\n"
            "\n"
            "vec3 linearToSRGB(vec3 v)\n"
            "{\n"
            "    vec3 a = abs(v);\n"
            "    return sign(v) * mix(\n"
            "        1.055 * pow(a, vec3(1.0 / 2.4)) - 0.055, a * 12.92, step(a, vec3(0.0031308)));\n"
            "}\n"
            "\n"
            // The sRGB curve undone, P3's primaries taken to Rec. 709's by
            // row, and the curve again: what sRGB cannot hold is kept, past
            // one and below zero, for a window that holds extended sRGB.
            "vec3 displayP3ToSRGB(vec3 v)\n"
            "{\n"
            "    vec3 l = srgbToLinear(v);\n"
            "    return linearToSRGB(vec3(\n"
            "        dot(l, vec3(1.224940, -0.224940, 0.000000)),\n"
            "        dot(l, vec3(-0.042057, 1.042057, 0.000000)),\n"
            "        dot(l, vec3(-0.019638, -0.078636, 1.098274))));\n"
            "}\n"
            "\n"
            "vec3 pqToNits(vec3 v)\n"
            "{\n"
            "    float m1 = 0.1593017578125;\n"
            "    float m2 = 78.84375;\n"
            "    float c1 = 0.8359375;\n"
            "    float c2 = 18.8515625;\n"
            "    float c3 = 18.6875;\n"
            "    vec3 p = pow(clamp(v, vec3(0.0), vec3(1.0)), vec3(1.0 / m2));\n"
            "    return 10000.0 * pow(max(p - c1, vec3(0.0)) / (c2 - c3 * p), vec3(1.0 / m1));\n"
            "}\n"
            "\n"
            "vec3 rec2020ToRec709(vec3 v)\n"
            "{\n"
            "    return vec3(\n"
            "        dot(v, vec3(1.660491, -0.587641, -0.072850)),\n"
            "        dot(v, vec3(-0.124550, 1.132900, -0.008349)),\n"
            "        dot(v, vec3(-0.018151, -0.100579, 1.118730)));\n"
            "}\n";
        return out;
    }

    std::string getDisplayShaderBody(
        const DisplayShaderInputs& inputs,
        const ColorStageCalls& calls)
    {
        const std::string& u = inputs.uniforms;
        std::string out =
            "    vec2 t = " + inputs.uv + ";\n"
            "    if (1 == " + u + "mirrorX)\n"
            "    {\n"
            "        t.x = 1.0 - t.x;\n"
            "    }\n"
            "    if (1 == " + u + "mirrorY)\n"
            "    {\n"
            "        t.y = 1.0 - t.y;\n"
            "    }\n"
            "\n"
            "    outColor = max(" + u + "magnifyAxes.x, " + u + "magnifyAxes.y) > 0.5 ?\n"
            "        sampleMitchell(" + inputs.texture + ", t, " + u + "sourceSize, " + u + "magnifyAxes) :\n"
            "        sampleTexture(" + inputs.texture + ", t);\n"
            "\n"
            "    if (" + u + "negative != 0)\n"
            "    {\n"
            "        outColor.rgb = vec3(1.0) - outColor.rgb;\n"
            "    }\n"
            "\n"
            // To linear, so the color transformations operate on linear
            // values; see getColorStages().
            "    " + calls.before + "\n"
            "\n"
            "    if (" + u + "colorEnabled != 0)\n"
            "    {\n"
            "        outColor = applyColorMatrix(outColor, " + u + "colorAdd.rgb, " + u + "colorMatrix);\n"
            "    }\n"
            "    if (" + u + "levelsEnabled != 0)\n"
            "    {\n"
            "        outColor = applyLevels(\n"
            "            outColor,\n"
            "            " + u + "levelsInLow,\n"
            "            " + u + "levelsInHigh,\n"
            "            " + u + "levelsGamma,\n"
            "            " + u + "levelsOutLow,\n"
            "            " + u + "levelsOutHigh);\n"
            "    }\n"
            "    if (" + u + "exposureEnabled != 0)\n"
            "    {\n"
            "        outColor.rgb *= " + u + "exposure;\n"
            "    }\n"
            "    if (" + u + "softClip > 0.0)\n"
            "    {\n"
            "        outColor = applySoftClip(outColor, " + u + "softClip);\n"
            "    }\n"
            "\n"
            // The color management.
            "    " + calls.after + "\n"
            "\n";
        if (inputs.displayP3)
        {
            out +=
                "    if (" + u + "displayP3 != 0)\n"
                "    {\n"
                "        outColor.rgb = displayP3ToSRGB(outColor.rgb);\n"
                "    }\n"
                "\n";
        }
        out +=
            "    outColor = applyChannels(outColor, " + u + "channels);\n";
        return out;
    }
}

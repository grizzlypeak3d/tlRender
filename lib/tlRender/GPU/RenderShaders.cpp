// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GPU/RenderPrivate.h>

namespace tl
{
    namespace gpu
    {
        // These say what tl::gl's shaders say; see GL/RenderShaders.cpp,
        // which is where the reasons are.

        namespace
        {
            const std::string header =
                "#include <metal_stdlib>\n"
                "using namespace metal;\n"
                "\n"
                "struct VertexOut\n"
                "{\n"
                "    float4 position [[position]];\n"
                "    float2 uv [[user(locn0)]];\n"
                "};\n"
                "\n";
        }

        namespace
        {
            std::string textureFragmentSourceMSL()
            {
                return header +
                    "struct Uniforms\n"
                    "{\n"
                    "    float4 color;\n"
                    "};\n"
                    "\n"
                    "fragment float4 fragmentMain(\n"
                    "    VertexOut in [[stage_in]],\n"
                    "    constant Uniforms& u [[buffer(0)]],\n"
                    "    texture2d<float> t0 [[texture(0)]],\n"
                    "    sampler s0 [[sampler(0)]])\n"
                    "{\n"
                    "    return t0.sample(s0, in.uv) * u.color;\n"
                    "}\n";
            }

            std::string displayFragmentSourceMSL(
                const ShaderStage& toLinear,
                const ShaderStage& ocio,
                const ShaderStage& lut,
                LUTOrder lutOrder)
            {
                std::string before;
                std::string after;
                switch (lutOrder)
                {
                case LUTOrder::PreConfig:
                    before = lut.mslCall + "\n    " + toLinear.mslCall;
                    after = ocio.mslCall;
                    break;
                case LUTOrder::PostConfig:
                    before = toLinear.mslCall;
                    after = ocio.mslCall + "\n    " + lut.mslCall;
                    break;
                default: break;
                }
                return header +
                    "// enum ftk::ChannelDisplay\n"
                    "constant int Channels_Color = 0;\n"
                    "constant int Channels_Red   = 1;\n"
                    "constant int Channels_Green = 2;\n"
                    "constant int Channels_Blue  = 3;\n"
                    "constant int Channels_Alpha = 4;\n"
                    "\n"
                    "struct Uniforms\n"
                    "{\n"
                    "    float4x4 colorMatrix;\n"
                    "    float4   colorAdd;\n"
                    "    float2   magnifyAxes;\n"
                    "    float2   sourceSize;\n"
                    "    int      channels;\n"
                    "    int      negative;\n"
                    "    int      mirrorX;\n"
                    "    int      mirrorY;\n"
                    "    int      colorEnabled;\n"
                    "    int      levelsEnabled;\n"
                    "    int      exposureEnabled;\n"
                    "    float    exposure;\n"
                    "    float    softClip;\n"
                    "    float    levelsInLow;\n"
                    "    float    levelsInHigh;\n"
                    "    float    levelsGamma;\n"
                    "    float    levelsOutLow;\n"
                    "    float    levelsOutHigh;\n"
                    "};\n"
                    "\n"
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
                    "float axisWeight(float x, float enlarged)\n"
                    "{\n"
                    "    return enlarged > 0.5 ? mitchell(x) : (abs(x) <= 0.5 ? 1.0 : 0.0);\n"
                    "}\n"
                    "\n"
                    "float4 sampleMitchell(texture2d<float> t, sampler s, float2 uv, float2 size, float2 axes)\n"
                    "{\n"
                    "    float2 pos = uv * size - 0.5;\n"
                    "    float2 base = floor(pos);\n"
                    "    float2 f = pos - base;\n"
                    "    float4 c = float4(0.0);\n"
                    "    float total = 0.0;\n"
                    "    float4 lo = float4(1.0e38);\n"
                    "    float4 hi = float4(-1.0e38);\n"
                    "    for (int j = -1; j <= 2; ++j)\n"
                    "    {\n"
                    "        for (int i = -1; i <= 2; ++i)\n"
                    "        {\n"
                    "            float w = axisWeight(float(i) - f.x, axes.x) *\n"
                    "                axisWeight(float(j) - f.y, axes.y);\n"
                    "            float2 tc = (base + float2(float(i), float(j)) + 0.5) / size;\n"
                    "            float4 texel = t.sample(s, clamp(tc, float2(0.0), float2(1.0)));\n"
                    "            c += w * texel;\n"
                    "            total += w;\n"
                    "            if (i >= 0 && i <= 1 && j >= 0 && j <= 1)\n"
                    "            {\n"
                    "                lo = min(lo, texel);\n"
                    "                hi = max(hi, texel);\n"
                    "            }\n"
                    "        }\n"
                    "    }\n"
                    "    c = total > 0.0 ? c / total : c;\n"
                    "    return clamp(c, lo, hi);\n"
                    "}\n"
                    "\n"
                    "float4 colorFunc(float4 value, float3 add, float4x4 m)\n"
                    "{\n"
                    "    float4 tmp = float4(value.rgb + add, 1.0);\n"
                    "    tmp = m * tmp;\n"
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
                    "float softClipChannel(float value, float softClip)\n"
                    "{\n"
                    "    float tmp = 1.0 - softClip;\n"
                    "    if (value > tmp)\n"
                    "    {\n"
                    "        value = tmp + (1.0 - exp(-(value - tmp) / softClip)) * softClip;\n"
                    "    }\n"
                    "    return value;\n"
                    "}\n"
                    "\n" +
                    toLinear.mslDef + "\n" +
                    ocio.mslDef + "\n" +
                    lut.mslDef + "\n" +
                    "fragment float4 fragmentMain(\n"
                    "    VertexOut in [[stage_in]],\n"
                    "    constant Uniforms& u [[buffer(0)]],\n"
                    "    texture2d<float> t0 [[texture(0)]],\n"
                    "    sampler s0 [[sampler(0)]]" +
                    toLinear.mslArgs + ocio.mslArgs + lut.mslArgs + ")\n"
                    "{\n"
                    "    float2 t = in.uv;\n"
                    "    if (1 == u.mirrorX)\n"
                    "    {\n"
                    "        t.x = 1.0 - t.x;\n"
                    "    }\n"
                    "    if (1 == u.mirrorY)\n"
                    "    {\n"
                    "        t.y = 1.0 - t.y;\n"
                    "    }\n"
                    "\n"
                    "    float4 outColor = any(u.magnifyAxes > float2(0.5)) ?\n"
                    "        sampleMitchell(t0, s0, t, u.sourceSize, u.magnifyAxes) :\n"
                    "        t0.sample(s0, t);\n"
                    "\n"
                    "    if (u.negative != 0)\n"
                    "    {\n"
                    "        outColor.rgb = 1.0 - outColor.rgb;\n"
                    "    }\n"
                    "\n"
                    "    // To linear, so the color transformations operate on\n"
                    "    // linear values.\n"
                    "    " + before + "\n"
                    "\n"
                    "    // Apply color transformations.\n"
                    "    if (u.colorEnabled != 0)\n"
                    "    {\n"
                    "        outColor = colorFunc(outColor, u.colorAdd.rgb, u.colorMatrix);\n"
                    "    }\n"
                    "    if (u.levelsEnabled != 0)\n"
                    "    {\n"
                    "        outColor.r = levelsChannel(outColor.r, u.levelsInLow, u.levelsInHigh, u.levelsGamma, u.levelsOutLow, u.levelsOutHigh);\n"
                    "        outColor.g = levelsChannel(outColor.g, u.levelsInLow, u.levelsInHigh, u.levelsGamma, u.levelsOutLow, u.levelsOutHigh);\n"
                    "        outColor.b = levelsChannel(outColor.b, u.levelsInLow, u.levelsInHigh, u.levelsGamma, u.levelsOutLow, u.levelsOutHigh);\n"
                    "    }\n"
                    "    if (u.exposureEnabled != 0)\n"
                    "    {\n"
                    "        outColor.rgb *= u.exposure;\n"
                    "    }\n"
                    "    if (u.softClip > 0.0)\n"
                    "    {\n"
                    "        outColor.r = softClipChannel(outColor.r, u.softClip);\n"
                    "        outColor.g = softClipChannel(outColor.g, u.softClip);\n"
                    "        outColor.b = softClipChannel(outColor.b, u.softClip);\n"
                    "    }\n"
                    "\n"
                    "    // Apply color management.\n"
                    "    " + after + "\n"
                    "\n"
                    "    // Swizzle for the channels display.\n"
                    "    if (Channels_Red == u.channels)\n"
                    "    {\n"
                    "        outColor.g = outColor.r;\n"
                    "        outColor.b = outColor.r;\n"
                    "    }\n"
                    "    else if (Channels_Green == u.channels)\n"
                    "    {\n"
                    "        outColor.r = outColor.g;\n"
                    "        outColor.b = outColor.g;\n"
                    "    }\n"
                    "    else if (Channels_Blue == u.channels)\n"
                    "    {\n"
                    "        outColor.r = outColor.b;\n"
                    "        outColor.g = outColor.b;\n"
                    "    }\n"
                    "    else if (Channels_Alpha == u.channels)\n"
                    "    {\n"
                    "        outColor.r = outColor.a;\n"
                    "        outColor.g = outColor.a;\n"
                    "        outColor.b = outColor.a;\n"
                    "    }\n"
                    "    return outColor;\n"
                    "}\n";
            }

            std::string toLinearFragmentSourceMSL(const ShaderStage& toLinear)
            {
                return header +
                    "struct Uniforms\n"
                    "{\n"
                    "    float4 color;\n"
                    "};\n"
                    "\n" +
                    toLinear.mslDef + "\n" +
                    "fragment float4 fragmentMain(\n"
                    "    VertexOut in [[stage_in]],\n"
                    "    constant Uniforms& u [[buffer(0)]],\n"
                    "    texture2d<float> t0 [[texture(0)]],\n"
                    "    sampler s0 [[sampler(0)]]" +
                    toLinear.mslArgs + ")\n"
                    "{\n"
                    "    float4 outColor = t0.sample(s0, in.uv);\n"
                    "    " + toLinear.mslCall + "\n"
                    "    return outColor;\n"
                    "}\n";
            }

            std::string dissolveFragmentSourceMSL()
            {
                return header +
                    "struct Uniforms\n"
                    "{\n"
                    "    float dissolve;\n"
                    "};\n"
                    "\n"
                    "fragment float4 fragmentMain(\n"
                    "    VertexOut in [[stage_in]],\n"
                    "    constant Uniforms& u [[buffer(0)]],\n"
                    "    texture2d<float> t0 [[texture(0)]],\n"
                    "    texture2d<float> t1 [[texture(1)]],\n"
                    "    sampler s0 [[sampler(0)]],\n"
                    "    sampler s1 [[sampler(1)]])\n"
                    "{\n"
                    "    float4 c = t0.sample(s0, in.uv);\n"
                    "    float4 c2 = t1.sample(s1, in.uv);\n"
                    "    return c * (1.0 - u.dissolve) + c2 * u.dissolve;\n"
                    "}\n";
            }

            std::string clippingWarningFragmentSourceMSL()
            {
                return header +
                    "struct Uniforms\n"
                    "{\n"
                    "    int mode;\n"
                    "    float low;\n"
                    "    float high;\n"
                    "};\n"
                    "\n"
                    "fragment float4 fragmentMain(\n"
                    "    VertexOut in [[stage_in]],\n"
                    "    constant Uniforms& u [[buffer(0)]],\n"
                    "    texture2d<float> t0 [[texture(0)]],\n"
                    "    sampler s0 [[sampler(0)]])\n"
                    "{\n"
                    "    float4 c = t0.sample(s0, in.uv);\n"
                    "    float top = max(c.r, max(c.g, c.b));\n"
                    "    float bottom = min(c.r, min(c.g, c.b));\n"
                    "    if (1 == u.mode)\n"
                    "    {\n"
                    "        float t = top;\n"
                    "        top = bottom;\n"
                    "        bottom = t;\n"
                    "    }\n"
                    "    else if (2 == u.mode)\n"
                    "    {\n"
                    "        top = dot(c.rgb, float3(0.2126, 0.7152, 0.0722));\n"
                    "        bottom = top;\n"
                    "    }\n"
                    "    if (top > u.high)\n"
                    "    {\n"
                    "        return float4(1.0, 0.0, 0.0, 1.0);\n"
                    "    }\n"
                    "    else if (bottom < u.low)\n"
                    "    {\n"
                    "        return float4(1.0, 0.0, 1.0, 1.0);\n"
                    "    }\n"
                    "    return float4(0.0);\n"
                    "}\n";
            }

            std::string butterflyFragmentSourceMSL()
            {
                return header +
                    "struct Uniforms\n"
                    "{\n"
                    "    float gain;\n"
                    "};\n"
                    "\n"
                    "fragment float4 fragmentMain(\n"
                    "    VertexOut in [[stage_in]],\n"
                    "    constant Uniforms& u [[buffer(0)]],\n"
                    "    texture2d<float> t0 [[texture(0)]],\n"
                    "    texture2d<float> t1 [[texture(1)]],\n"
                    "    sampler s0 [[sampler(0)]],\n"
                    "    sampler s1 [[sampler(1)]])\n"
                    "{\n"
                    "    if (in.uv.x < .5)\n"
                    "    {\n"
                    "        return t0.sample(s0, in.uv);\n"
                    "    }\n"
                    "    return t1.sample(s1, float2(1.0 - in.uv.x, in.uv.y));\n"
                    "}\n";
            }

            std::string differenceFragmentSourceMSL()
            {
                return header +
                    "struct Uniforms\n"
                    "{\n"
                    "    float gain;\n"
                    "};\n"
                    "\n"
                    "fragment float4 fragmentMain(\n"
                    "    VertexOut in [[stage_in]],\n"
                    "    constant Uniforms& u [[buffer(0)]],\n"
                    "    texture2d<float> t0 [[texture(0)]],\n"
                    "    texture2d<float> t1 [[texture(1)]],\n"
                    "    sampler s0 [[sampler(0)]],\n"
                    "    sampler s1 [[sampler(1)]])\n"
                    "{\n"
                    "    float4 c = t0.sample(s0, in.uv);\n"
                    "    float4 cB = t1.sample(s1, in.uv);\n"
                    "    float4 outColor;\n"
                    "    outColor.rgb = abs(c.rgb - cB.rgb) * u.gain;\n"
                    "    outColor.a = max(c.a, cB.a);\n"
                    "    return outColor;\n"
                    "}\n";
            }
        }

        namespace
        {
            // A picture that is display encoded for an HDR display, into
            // what a window holds: sRGB encoded, with one as white and more
            // than one brighter by the same curve. The picture's code
            // values are taken to light, to Rec. 709 -- where what is
            // outside it is negative, and kept -- and divided by the
            // luminance of white.
            std::string hdrFragmentSourceMSL()
            {
                return header +
                    "struct Uniforms\n"
                    "{\n"
                    "    float4 color;\n"
                    "    int eotf;\n"
                    "    float whiteNits;\n"
                    "};\n"
                    "\n"
                    "// enum tl::HDR_EOTF\n"
                    "constant int HDR_EOTF_ST2084 = 2;\n"
                    "\n"
                    "// SMPTE ST 2084, to nits.\n"
                    "float3 fromPQ(float3 v)\n"
                    "{\n"
                    "    const float m1 = 0.1593017578125;\n"
                    "    const float m2 = 78.84375;\n"
                    "    const float c1 = 0.8359375;\n"
                    "    const float c2 = 18.8515625;\n"
                    "    const float c3 = 18.6875;\n"
                    "    float3 p = pow(clamp(v, 0.0, 1.0), float3(1.0 / m2));\n"
                    "    return 10000.0 * pow(max(p - c1, 0.0) / (c2 - c3 * p), float3(1.0 / m1));\n"
                    "}\n"
                    "\n"
                    "// The sRGB curve, carried on past one and mirrored below zero.\n"
                    "float3 fromLinear(float3 v)\n"
                    "{\n"
                    "    float3 a = abs(v);\n"
                    "    float3 lo = a * 12.92;\n"
                    "    float3 hi = 1.055 * pow(a, float3(1.0 / 2.4)) - 0.055;\n"
                    "    return sign(v) * select(hi, lo, a <= float3(0.0031308));\n"
                    "}\n"
                    "\n"
                    "fragment float4 fragmentMain(\n"
                    "    VertexOut in [[stage_in]],\n"
                    "    constant Uniforms& u [[buffer(0)]],\n"
                    "    texture2d<float> t0 [[texture(0)]],\n"
                    "    sampler s0 [[sampler(0)]])\n"
                    "{\n"
                    "    float4 c = t0.sample(s0, in.uv);\n"
                    "    if (HDR_EOTF_ST2084 == u.eotf)\n"
                    "    {\n"
                    "        // Rec. 2020 primaries to Rec. 709, by row.\n"
                    "        float3 l = fromPQ(c.rgb) / u.whiteNits;\n"
                    "        float3 r709 = float3(\n"
                    "            dot(l, float3(1.660491, -0.587641, -0.072850)),\n"
                    "            dot(l, float3(-0.124550, 1.132900, -0.008349)),\n"
                    "            dot(l, float3(-0.018151, -0.100579, 1.118730)));\n"
                    "        c.rgb = fromLinear(r709);\n"
                    "    }\n"
                    "    return c * u.color;\n"
                    "}\n";
            }
        }

        namespace
        {
            const std::string headerGLSL =
                "#version 450\n"
                "\n"
                "layout(location = 0) in vec2 fTexture;\n"
                "layout(location = 0) out vec4 outColor;\n"
                "\n";

            std::string textureFragmentSourceGLSL()
            {
                return headerGLSL +
                    "layout(set = 2, binding = 0) uniform sampler2D s0;\n"
                    "\n"
                    "layout(set = 3, binding = 0) uniform Uniforms\n"
                    "{\n"
                    "    vec4 color;\n"
                    "} u;\n"
                    "\n"
                    "void main()\n"
                    "{\n"
                    "    outColor = texture(s0, fTexture) * u.color;\n"
                    "}\n";
            }

            std::string displayFragmentSourceGLSL(
                const ShaderStage& toLinear,
                const ShaderStage& ocio,
                const ShaderStage& lut,
                LUTOrder lutOrder)
            {
                std::string before;
                std::string after;
                switch (lutOrder)
                {
                case LUTOrder::PreConfig:
                    before = lut.glslCall + "\n    " + toLinear.glslCall;
                    after = ocio.glslCall;
                    break;
                case LUTOrder::PostConfig:
                    before = toLinear.glslCall;
                    after = ocio.glslCall + "\n    " + lut.glslCall;
                    break;
                default: break;
                }
                return headerGLSL +
                    "// enum ftk::ChannelDisplay\n"
                    "const int Channels_Color = 0;\n"
                    "const int Channels_Red   = 1;\n"
                    "const int Channels_Green = 2;\n"
                    "const int Channels_Blue  = 3;\n"
                    "const int Channels_Alpha = 4;\n"
                    "\n"
                    "layout(set = 2, binding = 0) uniform sampler2D s0;\n"
                    "\n"
                    "layout(set = 3, binding = 0) uniform Uniforms\n"
                    "{\n"
                    "    mat4  colorMatrix;\n"
                    "    vec4  colorAdd;\n"
                    "    vec2  magnifyAxes;\n"
                    "    vec2  sourceSize;\n"
                    "    int   channels;\n"
                    "    int   negative;\n"
                    "    int   mirrorX;\n"
                    "    int   mirrorY;\n"
                    "    int   colorEnabled;\n"
                    "    int   levelsEnabled;\n"
                    "    int   exposureEnabled;\n"
                    "    float exposure;\n"
                    "    float softClip;\n"
                    "    float levelsInLow;\n"
                    "    float levelsInHigh;\n"
                    "    float levelsGamma;\n"
                    "    float levelsOutLow;\n"
                    "    float levelsOutHigh;\n"
                    "} u;\n"
                    "\n"
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
                    "float axisWeight(float x, float enlarged)\n"
                    "{\n"
                    "    return enlarged > 0.5 ? mitchell(x) : (abs(x) <= 0.5 ? 1.0 : 0.0);\n"
                    "}\n"
                    "\n"
                    "vec4 sampleMitchell(sampler2D s, vec2 uv, vec2 size, vec2 axes)\n"
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
                    "            vec4 texel = texture(s, clamp(tc, vec2(0.0), vec2(1.0)));\n"
                    "            c += w * texel;\n"
                    "            total += w;\n"
                    "            if (i >= 0 && i <= 1 && j >= 0 && j <= 1)\n"
                    "            {\n"
                    "                lo = min(lo, texel);\n"
                    "                hi = max(hi, texel);\n"
                    "            }\n"
                    "        }\n"
                    "    }\n"
                    "    c = total > 0.0 ? c / total : c;\n"
                    "    return clamp(c, lo, hi);\n"
                    "}\n"
                    "\n"
                    "vec4 colorFunc(vec4 value, vec3 add, mat4 m)\n"
                    "{\n"
                    "    vec4 tmp = vec4(value.rgb + add, 1.0);\n"
                    "    tmp = m * tmp;\n"
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
                    "float softClipChannel(float value, float softClip)\n"
                    "{\n"
                    "    float tmp = 1.0 - softClip;\n"
                    "    if (value > tmp)\n"
                    "    {\n"
                    "        value = tmp + (1.0 - exp(-(value - tmp) / softClip)) * softClip;\n"
                    "    }\n"
                    "    return value;\n"
                    "}\n"
                    "\n" +
                    toLinear.glslDef + "\n" +
                    ocio.glslDef + "\n" +
                    lut.glslDef + "\n" +
                    "void main()\n"
                    "{\n"
                    "    vec2 t = fTexture;\n"
                    "    if (1 == u.mirrorX)\n"
                    "    {\n"
                    "        t.x = 1.0 - t.x;\n"
                    "    }\n"
                    "    if (1 == u.mirrorY)\n"
                    "    {\n"
                    "        t.y = 1.0 - t.y;\n"
                    "    }\n"
                    "\n"
                    "    outColor = any(greaterThan(u.magnifyAxes, vec2(0.5))) ?\n"
                    "        sampleMitchell(s0, t, u.sourceSize, u.magnifyAxes) :\n"
                    "        texture(s0, t);\n"
                    "\n"
                    "    if (u.negative != 0)\n"
                    "    {\n"
                    "        outColor.rgb = 1.0 - outColor.rgb;\n"
                    "    }\n"
                    "\n"
                    "    // To linear, so the color transformations operate on\n"
                    "    // linear values.\n"
                    "    " + before + "\n"
                    "\n"
                    "    // Apply color transformations.\n"
                    "    if (u.colorEnabled != 0)\n"
                    "    {\n"
                    "        outColor = colorFunc(outColor, u.colorAdd.rgb, u.colorMatrix);\n"
                    "    }\n"
                    "    if (u.levelsEnabled != 0)\n"
                    "    {\n"
                    "        outColor.r = levelsChannel(outColor.r, u.levelsInLow, u.levelsInHigh, u.levelsGamma, u.levelsOutLow, u.levelsOutHigh);\n"
                    "        outColor.g = levelsChannel(outColor.g, u.levelsInLow, u.levelsInHigh, u.levelsGamma, u.levelsOutLow, u.levelsOutHigh);\n"
                    "        outColor.b = levelsChannel(outColor.b, u.levelsInLow, u.levelsInHigh, u.levelsGamma, u.levelsOutLow, u.levelsOutHigh);\n"
                    "    }\n"
                    "    if (u.exposureEnabled != 0)\n"
                    "    {\n"
                    "        outColor.rgb *= u.exposure;\n"
                    "    }\n"
                    "    if (u.softClip > 0.0)\n"
                    "    {\n"
                    "        outColor.r = softClipChannel(outColor.r, u.softClip);\n"
                    "        outColor.g = softClipChannel(outColor.g, u.softClip);\n"
                    "        outColor.b = softClipChannel(outColor.b, u.softClip);\n"
                    "    }\n"
                    "\n"
                    "    // Apply color management.\n"
                    "    " + after + "\n"
                    "\n"
                    "    // Swizzle for the channels display.\n"
                    "    if (Channels_Red == u.channels)\n"
                    "    {\n"
                    "        outColor.g = outColor.r;\n"
                    "        outColor.b = outColor.r;\n"
                    "    }\n"
                    "    else if (Channels_Green == u.channels)\n"
                    "    {\n"
                    "        outColor.r = outColor.g;\n"
                    "        outColor.b = outColor.g;\n"
                    "    }\n"
                    "    else if (Channels_Blue == u.channels)\n"
                    "    {\n"
                    "        outColor.r = outColor.b;\n"
                    "        outColor.g = outColor.b;\n"
                    "    }\n"
                    "    else if (Channels_Alpha == u.channels)\n"
                    "    {\n"
                    "        outColor.r = outColor.a;\n"
                    "        outColor.g = outColor.a;\n"
                    "        outColor.b = outColor.a;\n"
                    "    }\n"
                    "}\n";
            }

            std::string toLinearFragmentSourceGLSL(const ShaderStage& toLinear)
            {
                return headerGLSL +
                    "layout(set = 2, binding = 0) uniform sampler2D s0;\n"
                    "\n"
                    "layout(set = 3, binding = 0) uniform Uniforms\n"
                    "{\n"
                    "    vec4 color;\n"
                    "} u;\n"
                    "\n" +
                    toLinear.glslDef + "\n" +
                    "void main()\n"
                    "{\n"
                    "    outColor = texture(s0, fTexture);\n"
                    "    " + toLinear.glslCall + "\n"
                    "}\n";
            }

            std::string dissolveFragmentSourceGLSL()
            {
                return headerGLSL +
                    "layout(set = 2, binding = 0) uniform sampler2D s0;\n"
                    "layout(set = 2, binding = 1) uniform sampler2D s1;\n"
                    "\n"
                    "layout(set = 3, binding = 0) uniform Uniforms\n"
                    "{\n"
                    "    float dissolve;\n"
                    "} u;\n"
                    "\n"
                    "void main()\n"
                    "{\n"
                    "    vec4 c = texture(s0, fTexture);\n"
                    "    vec4 c2 = texture(s1, fTexture);\n"
                    "    outColor = c * (1.0 - u.dissolve) + c2 * u.dissolve;\n"
                    "}\n";
            }

            std::string clippingWarningFragmentSourceGLSL()
            {
                return headerGLSL +
                    "layout(set = 2, binding = 0) uniform sampler2D s0;\n"
                    "\n"
                    "layout(set = 3, binding = 0) uniform Uniforms\n"
                    "{\n"
                    "    int mode;\n"
                    "    float low;\n"
                    "    float high;\n"
                    "} u;\n"
                    "\n"
                    "void main()\n"
                    "{\n"
                    "    vec4 c = texture(s0, fTexture);\n"
                    "    float top = max(c.r, max(c.g, c.b));\n"
                    "    float bottom = min(c.r, min(c.g, c.b));\n"
                    "    if (1 == u.mode)\n"
                    "    {\n"
                    "        float t = top;\n"
                    "        top = bottom;\n"
                    "        bottom = t;\n"
                    "    }\n"
                    "    else if (2 == u.mode)\n"
                    "    {\n"
                    "        top = dot(c.rgb, vec3(0.2126, 0.7152, 0.0722));\n"
                    "        bottom = top;\n"
                    "    }\n"
                    "    if (top > u.high)\n"
                    "    {\n"
                    "        outColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                    "    }\n"
                    "    else if (bottom < u.low)\n"
                    "    {\n"
                    "        outColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
                    "    }\n"
                    "    else\n"
                    "    {\n"
                    "        outColor = vec4(0.0);\n"
                    "    }\n"
                    "}\n";
            }

            const std::string pairGLSL =
                "layout(set = 2, binding = 0) uniform sampler2D s0;\n"
                "layout(set = 2, binding = 1) uniform sampler2D s1;\n"
                "\n"
                "layout(set = 3, binding = 0) uniform Uniforms\n"
                "{\n"
                "    float gain;\n"
                "} u;\n"
                "\n";

            std::string butterflyFragmentSourceGLSL()
            {
                return headerGLSL + pairGLSL +
                    "void main()\n"
                    "{\n"
                    "    if (fTexture.x < .5)\n"
                    "    {\n"
                    "        outColor = texture(s0, fTexture);\n"
                    "    }\n"
                    "    else\n"
                    "    {\n"
                    "        outColor = texture(s1, vec2(1.0 - fTexture.x, fTexture.y));\n"
                    "    }\n"
                    "}\n";
            }

            std::string differenceFragmentSourceGLSL()
            {
                return headerGLSL + pairGLSL +
                    "void main()\n"
                    "{\n"
                    "    vec4 c = texture(s0, fTexture);\n"
                    "    vec4 cB = texture(s1, fTexture);\n"
                    "    outColor.rgb = abs(c.rgb - cB.rgb) * u.gain;\n"
                    "    outColor.a = max(c.a, cB.a);\n"
                    "}\n";
            }
        }

        namespace
        {
            std::string hdrFragmentSourceGLSL()
            {
                return headerGLSL +
                    "layout(set = 2, binding = 0) uniform sampler2D s0;\n"
                    "\n"
                    "layout(set = 3, binding = 0) uniform Uniforms\n"
                    "{\n"
                    "    vec4 color;\n"
                    "    int eotf;\n"
                    "    float whiteNits;\n"
                    "} u;\n"
                    "\n"
                    "// enum tl::HDR_EOTF\n"
                    "const int HDR_EOTF_ST2084 = 2;\n"
                    "\n"
                    "// SMPTE ST 2084, to nits.\n"
                    "vec3 fromPQ(vec3 v)\n"
                    "{\n"
                    "    const float m1 = 0.1593017578125;\n"
                    "    const float m2 = 78.84375;\n"
                    "    const float c1 = 0.8359375;\n"
                    "    const float c2 = 18.8515625;\n"
                    "    const float c3 = 18.6875;\n"
                    "    vec3 p = pow(clamp(v, 0.0, 1.0), vec3(1.0 / m2));\n"
                    "    return 10000.0 * pow(max(p - c1, 0.0) / (c2 - c3 * p), vec3(1.0 / m1));\n"
                    "}\n"
                    "\n"
                    "// The sRGB curve, carried on past one and mirrored below zero.\n"
                    "vec3 fromLinear(vec3 v)\n"
                    "{\n"
                    "    vec3 a = abs(v);\n"
                    "    vec3 lo = a * 12.92;\n"
                    "    vec3 hi = 1.055 * pow(a, vec3(1.0 / 2.4)) - 0.055;\n"
                    "    return sign(v) * mix(hi, lo, lessThanEqual(a, vec3(0.0031308)));\n"
                    "}\n"
                    "\n"
                    "void main()\n"
                    "{\n"
                    "    vec4 c = texture(s0, fTexture);\n"
                    "    if (HDR_EOTF_ST2084 == u.eotf)\n"
                    "    {\n"
                    "        // Rec. 2020 primaries to Rec. 709, by row.\n"
                    "        vec3 l = fromPQ(c.rgb) / u.whiteNits;\n"
                    "        vec3 r709 = vec3(\n"
                    "            dot(l, vec3(1.660491, -0.587641, -0.072850)),\n"
                    "            dot(l, vec3(-0.124550, 1.132900, -0.008349)),\n"
                    "            dot(l, vec3(-0.018151, -0.100579, 1.118730)));\n"
                    "        c.rgb = fromLinear(r709);\n"
                    "    }\n"
                    "    outColor = c * u.color;\n"
                    "}\n";
            }
        }

        ftk::gpu::ShaderSource hdrFragmentSource()
        {
            return { hdrFragmentSourceMSL(), hdrFragmentSourceGLSL() };
        }

        ftk::gpu::ShaderSource textureFragmentSource()
        {
            return { textureFragmentSourceMSL(), textureFragmentSourceGLSL() };
        }

        ftk::gpu::ShaderSource displayFragmentSource(
            const ShaderStage& toLinear,
            const ShaderStage& ocio,
            const ShaderStage& lut,
            LUTOrder lutOrder)
        {
            return
            {
                displayFragmentSourceMSL(toLinear, ocio, lut, lutOrder),
                displayFragmentSourceGLSL(toLinear, ocio, lut, lutOrder)
            };
        }

        ftk::gpu::ShaderSource toLinearFragmentSource(const ShaderStage& toLinear)
        {
            return { toLinearFragmentSourceMSL(toLinear), toLinearFragmentSourceGLSL(toLinear) };
        }

        ftk::gpu::ShaderSource dissolveFragmentSource()
        {
            return { dissolveFragmentSourceMSL(), dissolveFragmentSourceGLSL() };
        }

        ftk::gpu::ShaderSource clippingWarningFragmentSource()
        {
            return { clippingWarningFragmentSourceMSL(), clippingWarningFragmentSourceGLSL() };
        }

        ftk::gpu::ShaderSource butterflyFragmentSource()
        {
            return { butterflyFragmentSourceMSL(), butterflyFragmentSourceGLSL() };
        }

        ftk::gpu::ShaderSource differenceFragmentSource()
        {
            return { differenceFragmentSourceMSL(), differenceFragmentSourceGLSL() };
        }
    }
}

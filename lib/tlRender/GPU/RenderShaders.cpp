// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GPU/RenderPrivate.h>

#include <tlRender/Timeline/ColorShaders.h>

namespace tl
{
    namespace gpu
    {
        // These say what tl::gl's shaders say; see GL/RenderShaders.cpp,
        // which is where the reasons are. The color functions are both
        // renderers', in tl/Timeline/ColorShaders.

        namespace
        {
            // A picture that is display encoded for an HDR display, into
            // what a window holds: sRGB encoded, with one as white and more
            // than one brighter by the same curve. The picture's code
            // values are taken to light, to Rec. 709 -- where what is
            // outside it is negative, and kept -- and divided by the
            // luminance of white. The eotf is tl::HDR_EOTF, of which two is
            // ST 2084. Written in GLSL's types, which Metal is given.
            const std::string hdrBody =
                "    if (2 == u.eotf)\n"
                "    {\n"
                "        c.rgb = linearToSRGB(rec2020ToRec709(pqToNits(c.rgb) / u.whiteNits));\n"
                "    }\n"
                "    else if (u.displayP3 != 0)\n"
                "    {\n"
                "        c.rgb = displayP3ToSRGB(c.rgb);\n"
                "    }\n";

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
                DisplayShaderInputs inputs;
                inputs.uniforms = "u.";
                inputs.texture = "t0, s0";
                inputs.uv = "in.uv";
                inputs.displayP3 = true;
                return header +
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
                    "    int      displayP3;\n"
                    "};\n"
                    "\n" +
                    getColorShaderFunctions(ShaderLanguage::MSL) + "\n" +
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
                    "    float4 outColor;\n" +
                    getDisplayShaderBody(
                        inputs,
                        getColorStageCalls(lutOrder, lut.mslCall, toLinear.mslCall, ocio.mslCall)) +
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
                    "    int displayP3;\n"
                    "};\n"
                    "\n" +
                    getColorShaderFunctions(ShaderLanguage::MSL) + "\n" +
                    "fragment float4 fragmentMain(\n"
                    "    VertexOut in [[stage_in]],\n"
                    "    constant Uniforms& u [[buffer(0)]],\n"
                    "    texture2d<float> t0 [[texture(0)]],\n"
                    "    sampler s0 [[sampler(0)]])\n"
                    "{\n"
                    "    float4 c = t0.sample(s0, in.uv);\n" +
                    hdrBody +
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
                DisplayShaderInputs inputs;
                inputs.uniforms = "u.";
                inputs.texture = "s0";
                inputs.uv = "fTexture";
                inputs.displayP3 = true;
                return headerGLSL +
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
                    "    int   displayP3;\n"
                    "} u;\n"
                    "\n" +
                    getColorShaderFunctions(ShaderLanguage::GLSL_VK) + "\n" +
                    toLinear.glslDef + "\n" +
                    ocio.glslDef + "\n" +
                    lut.glslDef + "\n" +
                    "void main()\n"
                    "{\n" +
                    getDisplayShaderBody(
                        inputs,
                        getColorStageCalls(lutOrder, lut.glslCall, toLinear.glslCall, ocio.glslCall)) +
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
                    "    int displayP3;\n"
                    "} u;\n"
                    "\n" +
                    getColorShaderFunctions(ShaderLanguage::GLSL_VK) + "\n" +
                    "void main()\n"
                    "{\n"
                    "    vec4 c = texture(s0, fTexture);\n" +
                    hdrBody +
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

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/GL/RenderPrivate.h>

#include <tlRender/Timeline/ColorShaders.h>

#include <ftk/Core/Format.h>

namespace tl
{
    namespace gl
    {
        std::string vertexSource()
        {
            return
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

        std::string toLinearFragmentSource(
            const std::string& toLinearDef,
            const std::string& toLinear)
        {
            return ftk::Format(
                "\n"
                "in vec2 fTexture;\n"
                "out vec4 outColor;\n"
                "\n"
                "uniform sampler2D textureSampler;\n"
                "\n"
                "{0}\n"
                "\n"
                "void main()\n"
                "{\n"
                "    outColor = texture(textureSampler, fTexture);\n"
                "    {1}\n"
                "}\n").
                arg(toLinearDef).
                arg(toLinear);
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
            DisplayShaderInputs inputs;
            inputs.texture = "textureSampler";
            inputs.uv = "fTexture";
            return
                "\n"
                "in vec2 fTexture;\n"
                "out vec4 outColor;\n"
                "\n"
                "uniform sampler2D textureSampler;\n"
                "uniform vec2      magnifyAxes;\n"
                // Not "textureSize", which is a built in function: desktop
                // GLSL lets a uniform hide it, OpenGL ES does not.
                "uniform vec2      sourceSize;\n"
                "uniform int       channels;\n"
                "uniform int       negative;\n"
                "uniform int       mirrorX;\n"
                "uniform int       mirrorY;\n"
                "uniform int       colorEnabled;\n"
                "uniform vec3      colorAdd;\n"
                "uniform mat4      colorMatrix;\n"
                "uniform int       levelsEnabled;\n"
                "uniform float     levelsInLow;\n"
                "uniform float     levelsInHigh;\n"
                "uniform float     levelsGamma;\n"
                "uniform float     levelsOutLow;\n"
                "uniform float     levelsOutHigh;\n"
                "uniform int       exposureEnabled;\n"
                "uniform float     exposure;\n"
                "uniform float     softClip;\n"
                "\n" +
                getColorShaderFunctions(ShaderLanguage::GLSL) + "\n" +
                toLinearDef + "\n" +
                ocioDef + "\n" +
                lutDef + "\n" +
                "void main()\n"
                "{\n" +
                getDisplayShaderBody(inputs, getColorStageCalls(lutOrder, lut, toLinear, ocio)) +
                "}\n";
        }

        std::string dissolveFragmentSource()
        {
            return
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

        std::string clippingWarningFragmentSource()
        {
            return
                "\n"
                "in vec2 fTexture;\n"
                "out vec4 outColor;\n"
                "\n"
                "uniform int       mode;\n"
                "uniform float     low;\n"
                "uniform float     high;\n"
                "uniform sampler2D textureSampler;\n"
                "\n"
                "void main()\n"
                "{\n"
                "    vec4 c = texture(textureSampler, fTexture);\n"
                // What is compared with the top and the bottom of the range:
                // any channel is the largest above and the smallest below,
                // all channels the other way around, and the luminance both.
                "    float top = max(c.r, max(c.g, c.b));\n"
                "    float bottom = min(c.r, min(c.g, c.b));\n"
                "    if (1 == mode)\n"
                "    {\n"
                "        float t = top;\n"
                "        top = bottom;\n"
                "        bottom = t;\n"
                "    }\n"
                "    else if (2 == mode)\n"
                "    {\n"
                "        top = dot(c.rgb, vec3(0.2126, 0.7152, 0.0722));\n"
                "        bottom = top;\n"
                "    }\n"
                "    if (top > high)\n"
                "    {\n"
                "        outColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                "    }\n"
                "    else if (bottom < low)\n"
                "    {\n"
                "        outColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
                "    }\n"
                "    else\n"
                "    {\n"
                "        outColor = vec4(0.0);\n"
                "    }\n"
                "}\n";
        }

        std::string displayP3FragmentSource()
        {
            // What sRGB cannot hold the window clips.
            return
                "\n"
                "in vec2 fTexture;\n"
                "out vec4 outColor;\n"
                "\n"
                "uniform sampler2D textureSampler;\n"
                "\n" +
                getColorShaderFunctions(ShaderLanguage::GLSL) + "\n" +
                "void main()\n"
                "{\n"
                "    vec4 c = texture(textureSampler, fTexture);\n"
                "    outColor = vec4(displayP3ToSRGB(c.rgb), c.a);\n"
                "}\n";
        }

        std::string butterflyFragmentSource()
        {
            return
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

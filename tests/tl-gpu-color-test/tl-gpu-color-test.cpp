// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

// The GPU renderer against the CPU: the same pixels drawn through the
// display shader on Metal or Vulkan and through tl::CPUColor, for the two to
// match, as tl::gl_test::RenderTest checks the OpenGL renderer. What keeps
// the renderers, and the thumbnails drawn on the CPU, from drifting apart.
//
// With no GPU device to draw with, the test is skipped: exit code 77.

#include <tlRender/GPU/Render.h>

#include <tlRender/Timeline/CPUColor.h>

#include <ftk/GPU/OffscreenBuffer.h>
#include <ftk/GPU/System.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/FontSystem.h>

#include <OpenColorIO/OpenColorIO.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace tl;

namespace OCIO = OCIO_NAMESPACE;

int main(int, char**)
{
    int r = 1;
    try
    {
        auto context = ftk::Context::create();
        ftk::gpu::init(context);
        auto system = context->getSystem<ftk::gpu::System>();
        try
        {
            system->getDevice();
        }
        catch (const std::exception& e)
        {
            std::cout << "SKIPPED: no GPU device: " << e.what() << std::endl;
            return 77;
        }
        auto render = gpu::Render::create(
            system,
            context->getLogSystem(),
            context->getSystem<ftk::FontSystem>());

        // Ramps of every channel, a little past one at the end.
        const ftk::Size2I size(32, 16);
        auto image = ftk::Image::create(size, ftk::ImageType::RGBA_F32);
        float* pixels = reinterpret_cast<float*>(image->getData());
        for (int y = 0; y < size.h; ++y)
        {
            for (int x = 0; x < size.w; ++x)
            {
                float* p = pixels + (y * size.w + x) * 4;
                p[0] = x / static_cast<float>(size.w - 1) * 1.1F;
                p[1] = y / static_cast<float>(size.h - 1);
                p[2] = (x + y) / static_cast<float>(size.w + size.h - 2);
                p[3] = 1.F;
            }
        }
        VideoLayer layer;
        layer.image = image;
        VideoFrame frame;
        frame.size = size;
        frame.layers.push_back(layer);

        // The cases of tl::gl_test::RenderTest::_cpuColor().
        auto config = OCIO::Config::CreateFromFile("ocio://default");
        OCIOOptions ocio;
        ocio.enabled = true;
        ocio.config = OCIOConfig::BuiltIn;
        ocio.input = "ACEScg";
        ocio.display = config->getDefaultDisplay();
        ocio.view = config->getDefaultView(ocio.display.c_str());
        LUTOptions lut;
        lut.enabled = true;
        lut.fileName = std::string(TLRENDER_SAMPLE_DATA) + "/LUT_SRGB_256.lut";
        DisplayOptions corrections;
        corrections.color.enabled = true;
        corrections.color.add = ftk::V3F(.02F, 0.F, -.01F);
        corrections.color.contrast = ftk::V3F(1.2F, 1.2F, 1.2F);
        corrections.color.saturation = ftk::V3F(1.3F, 1.3F, 1.3F);
        corrections.color.hue = 10.F;
        corrections.levels.enabled = true;
        corrections.levels.inLow = .05F;
        corrections.levels.inHigh = .9F;
        corrections.levels.gamma = 1.3F;
        corrections.exposure.enabled = true;
        corrections.exposure.exposure = .5F;
        corrections.softClip.enabled = true;
        corrections.softClip.value = .2F;
        DisplayOptions channels;
        channels.negative = true;
        channels.channels = ftk::ChannelDisplay::Red;

        struct Case
        {
            std::string name;
            OCIOOptions ocio;
            LUTOptions lut;
            DisplayOptions display;
        };
        std::vector<Case> cases;
        cases.push_back({ "nothing", OCIOOptions(), LUTOptions(), DisplayOptions() });
        cases.push_back({ "corrections", OCIOOptions(), LUTOptions(), corrections });
        cases.push_back({ "negative and channels", OCIOOptions(), LUTOptions(), channels });
        cases.push_back({ "LUT", OCIOOptions(), lut, DisplayOptions() });
        cases.push_back({ "OCIO", ocio, LUTOptions(), DisplayOptions() });
        lut.order = LUTOrder::PreConfig;
        cases.push_back({ "OCIO, LUT before, corrections", ocio, lut, corrections });
        lut.order = LUTOrder::PostConfig;
        cases.push_back({ "OCIO, LUT after, corrections", ocio, lut, corrections });

        bool ok = true;
        for (const auto& c : cases)
        {
            auto buffer = ftk::gpu::OffscreenBuffer::create(
                system, size, ftk::gpu::BufferType::RGBA_F32);
            render->getBaseRender()->setTarget(buffer);
            render->begin(size);
            render->setOCIOOptions(c.ocio);
            render->setLUTOptions(c.lut);
            render->drawVideo(
                { frame },
                { ftk::Box2I(0, 0, size.w, size.h) },
                { ftk::ImageOptions() },
                { c.display });
            render->end();
            const auto out = buffer->read();

            std::vector<float> expected(pixels, pixels + size.w * size.h * 4);
            CPUColor(c.ocio, c.lut, c.display).apply(expected.data(), size.w * size.h);

            // As the OpenGL test allows: OCIO's shaders are not its CPU
            // arithmetic to the last bit, so a little more is allowed as the
            // value grows; a stage left out or out of order is far past it.
            const ftk::ImageInfo& info = out->getInfo();
            const float* result = reinterpret_cast<const float*>(out->getData());
            float maxDiff = 0.F;
            bool caseOK = true;
            for (int y = 0; y < size.h; ++y)
            {
                const int row = info.layout.mirror.y ? size.h - 1 - y : y;
                for (int x = 0; x < size.w; ++x)
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        const float a = result[(row * size.w + x) * 4 + i];
                        const float b = expected[(y * size.w + x) * 4 + i];
                        const float d = std::abs(a - b);
                        maxDiff = std::max(maxDiff, d);
                        if (!(d <= .002F + .002F * std::abs(b)))
                        {
                            caseOK = false;
                        }
                    }
                }
            }
            std::cout << c.name << ": largest difference " << maxDiff <<
                (caseOK ? "" : " FAIL") << std::endl;
            ok &= caseOK;
        }
        std::cout << (ok ? "PASS" : "FAIL") << std::endl;
        r = ok ? 0 : 1;
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what() << std::endl;
    }
    return r;
}

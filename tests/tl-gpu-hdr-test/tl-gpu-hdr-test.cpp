// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

// A spike: a picture that is PQ, drawn into a window and presented.
//
// The picture's code values are taken into what a window holds as the
// picture is drawn there (tl::gpu::Render::drawTextureHDR), and the window is
// written into the swapchain in the swapchain's terms (ftk::gpu::Present).
// Into an HDR10 swapchain, with the window's white where the picture's was
// taken to be, that is there and back: what arrives should be the code
// values that set out. Into an extended linear one it is the picture's
// light, in Rec. 709, as a multiple of white.

#include <tlRender/GPU/Render.h>

#include <ftk/GPU/OffscreenBuffer.h>
#include <ftk/GPU/Present.h>
#include <ftk/GPU/System.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/FontSystem.h>

#include <SDL3/SDL.h>

#include <cmath>
#include <iostream>

using namespace tl;

namespace
{
    float fromPQ(float v)
    {
        const float m1 = .1593017578125F;
        const float m2 = 78.84375F;
        const float c1 = .8359375F;
        const float c2 = 18.8515625F;
        const float c3 = 18.6875F;
        const float p = std::pow(v, 1.F / m2);
        return 10000.F * std::pow(std::max(p - c1, 0.F) / (c2 - c3 * p), 1.F / m1);
    }
}

int main(int, char**)
{
    int r = 1;
    try
    {
        auto context = ftk::Context::create();
        ftk::gpu::init(context);
        auto system = context->getSystem<ftk::gpu::System>();
        auto render = gpu::Render::create(
            system,
            context->getLogSystem(),
            context->getSystem<ftk::FontSystem>());
        auto presenter = ftk::gpu::Present::create(system);
        SDL_GPUDevice* device = system->getDevice();

        const ftk::Size2I size(16, 16);
        const float whiteNits = 203.F;
        bool ok = true;
        struct Case
        {
            const char* name;
            float code[3];
        };
        for (const Case& c :
            {
                // 203 nits, the reference white.
                Case{ "white", { .5807F, .5807F, .5807F } },
                // 1000 nits.
                Case{ "highlight", { .7518F, .7518F, .7518F } },
                // A color well outside Rec. 709.
                Case{ "green", { .30F, .70F, .25F } },
                Case{ "red", { .75F, .35F, .30F } }
            })
        {
            // The picture, as the viewport's buffer holds it.
            auto picture = ftk::gpu::OffscreenBuffer::create(system, size, ftk::gpu::BufferType::RGBA_F32);
            render->getBaseRender()->setTarget(picture);
            ftk::RenderOptions options;
            options.clearColor = ftk::Color4F(c.code[0], c.code[1], c.code[2], 1.F);
            render->begin(size, options);
            render->end();

            // Into a window.
            auto window = ftk::gpu::OffscreenBuffer::create(system, size, ftk::gpu::BufferType::RGBA_F32);
            render->getBaseRender()->setTarget(window);
            render->begin(size);
            render->drawTextureHDR(
                picture->getID(),
                ftk::Box2I(0, 0, size.w, size.h),
                true,
                ftk::AlphaBlend::Straight,
                HDR_EOTF::ST2084,
                whiteNits);
            render->end();

            // And out of it, into each kind of swapchain.
            const auto present = [&](ftk::gpu::Composition composition, float sdrWhiteLevel)
            {
                auto target = ftk::gpu::OffscreenBuffer::create(system, size, ftk::gpu::BufferType::RGBA_F32);
                SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
                presenter->draw(
                    cmd,
                    window->getTexture(),
                    target->getTexture(),
                    static_cast<int>(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT),
                    composition,
                    sdrWhiteLevel);
                SDL_SubmitGPUCommandBuffer(cmd);
                return target->getPixel(ftk::V2I(8, 8));
            };

            const ftk::Color4F hdr10 = present(ftk::gpu::Composition::HDR10, whiteNits / 80.F);
            const float d10 = std::max(
                std::fabs(hdr10.r - c.code[0]),
                std::max(std::fabs(hdr10.g - c.code[1]), std::fabs(hdr10.b - c.code[2])));

            // The light, in Rec. 709, as a multiple of white.
            const float l[3] =
            {
                fromPQ(c.code[0]) / whiteNits,
                fromPQ(c.code[1]) / whiteNits,
                fromPQ(c.code[2]) / whiteNits
            };
            const float expected[3] =
            {
                1.660491F * l[0] - .587641F * l[1] - .072850F * l[2],
                -.124550F * l[0] + 1.132900F * l[1] - .008349F * l[2],
                -.018151F * l[0] - .100579F * l[1] + 1.118730F * l[2]
            };
            const ftk::Color4F linear = present(ftk::gpu::Composition::HDRExtendedLinear, 1.F);
            const float dLinear = std::max(
                std::fabs(linear.r - expected[0]),
                std::max(std::fabs(linear.g - expected[1]), std::fabs(linear.b - expected[2])));
            const float scale = std::max(1.F, std::max(std::fabs(expected[0]), std::max(std::fabs(expected[1]), std::fabs(expected[2]))));

            std::cout << c.name << ": PQ " << c.code[0] << " " << c.code[1] << " " << c.code[2] <<
                " -> HDR10 " << hdr10.r << " " << hdr10.g << " " << hdr10.b <<
                " (off by " << d10 << "), extended linear " <<
                linear.r << " " << linear.g << " " << linear.b <<
                ", expected " << expected[0] << " " << expected[1] << " " << expected[2] << std::endl;
            ok &= d10 < .002F && dLinear / scale < .002F;
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

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Core/Gamut.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace tl
{
    bool isDisplayP3(const ftk::ImageTags& tags)
    {
        const auto primaries = tags.find("Color Primaries");
        const auto transfer = tags.find("Color Transfer");
        return
            primaries != tags.end() && "smpte432" == primaries->second &&
            transfer != tags.end() && "iec61966-2-1" == transfer->second;
    }

    void displayP3ToSRGB(const std::shared_ptr<ftk::Image>& image)
    {
        if (!image || image->getType() != ftk::ImageType::RGBA_U8)
            return;

        // Both have the sRGB curve: undone through a table of the 256
        // codes, P3's primaries taken to Rec. 709's in linear light, and
        // the curve again through a finer table.
        static const std::array<float, 256> toLinear = []
        {
            std::array<float, 256> out{};
            for (int i = 0; i < 256; ++i)
            {
                const float v = i / 255.F;
                out[i] = v <= .04045F ? v / 12.92F : std::pow((v + .055F) / 1.055F, 2.4F);
            }
            return out;
        }();
        static const int fromLinearSize = 4096;
        static const std::array<uint8_t, fromLinearSize + 1> fromLinear = []
        {
            std::array<uint8_t, fromLinearSize + 1> out{};
            for (int i = 0; i <= fromLinearSize; ++i)
            {
                const float v = i / static_cast<float>(fromLinearSize);
                const float e = v <= .0031308F ? v * 12.92F : 1.055F * std::pow(v, 1.F / 2.4F) - .055F;
                out[i] = static_cast<uint8_t>(std::lround(std::clamp(e, 0.F, 1.F) * 255.F));
            }
            return out;
        }();
        const auto encode = [](float v)
        {
            return fromLinear[std::lround(std::clamp(v, 0.F, 1.F) * fromLinearSize)];
        };

        uint8_t* p = image->getData();
        const size_t count = image->getByteCount() / 4;
        for (size_t i = 0; i < count; ++i, p += 4)
        {
            const float r = toLinear[p[0]];
            const float g = toLinear[p[1]];
            const float b = toLinear[p[2]];
            p[0] = encode(1.224940F * r - .224940F * g);
            p[1] = encode(-.042057F * r + 1.042057F * g);
            p[2] = encode(-.019638F * r - .078636F * g + 1.098274F * b);
        }
    }
}

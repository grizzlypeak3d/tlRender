// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/CPUColor.h>

#include <algorithm>
#include <cmath>
#include <vector>

#if defined(TLRENDER_OCIO)
namespace OCIO = OCIO_NAMESPACE;

namespace tl
{
    namespace
    {
        // The functions of ColorShaders, one value at a time.

        float levelsChannel(float value, const DisplayColorValues& v)
        {
            float tmp = (value - v.levelsInLow) / v.levelsInHigh;
            if (tmp >= 0.F)
            {
                tmp = std::pow(tmp, v.levelsGamma);
            }
            return tmp * v.levelsOutHigh + v.levelsOutLow;
        }

        float softClipChannel(float value, float amount)
        {
            const float tmp = 1.F - amount;
            if (value > tmp)
            {
                value = tmp + (1.F - std::exp(-(value - tmp) / amount)) * amount;
            }
            return value;
        }

        float sign(float value)
        {
            return value > 0.F ? 1.F : (value < 0.F ? -1.F : 0.F);
        }

        float srgbToLinear(float v)
        {
            const float a = std::abs(v);
            return sign(v) * (a <= .04045F ? a / 12.92F : std::pow((a + .055F) / 1.055F, 2.4F));
        }

        float linearToSRGB(float v)
        {
            const float a = std::abs(v);
            return sign(v) * (a <= .0031308F ? a * 12.92F : 1.055F * std::pow(a, 1.F / 2.4F) - .055F);
        }

        void displayP3ToSRGB(float* rgb)
        {
            const float r = srgbToLinear(rgb[0]);
            const float g = srgbToLinear(rgb[1]);
            const float b = srgbToLinear(rgb[2]);
            rgb[0] = linearToSRGB(1.224940F * r - .224940F * g);
            rgb[1] = linearToSRGB(-.042057F * r + 1.042057F * g);
            rgb[2] = linearToSRGB(-.019638F * r - .078636F * g + 1.098274F * b);
        }
    }

    struct CPUColor::Private
    {
        DisplayColorValues values;
        std::vector<OCIO::ConstCPUProcessorRcPtr> before;
        std::vector<OCIO::ConstCPUProcessorRcPtr> after;
        bool displayP3 = false;
        bool window = false;
    };

    CPUColor::CPUColor(
        const OCIOOptions& ocio,
        const LUTOptions& lut,
        const DisplayOptions& display,
        bool window) :
        _p(new Private)
    {
        FTK_P();
        p.values = getDisplayColorValues(display);
        p.window = window;

        // As the renderers have it: a picture with no input is not put
        // through the configuration.
        OCIOProcessors processors;
        if (ocio.enabled &&
            !ocio.input.empty() &&
            !ocio.display.empty() &&
            !ocio.view.empty())
        {
            processors = getOCIOProcessors(ocio);
            p.displayP3 = "smpte432" == getDisplayColorTags(ocio, false)["Color Primaries"];
        }
        OCIO::ConstProcessorRcPtr lutProcessor;
        if (lut.enabled && !lut.fileName.empty())
        {
            lutProcessor = getOCIOLUTProcessor(lut).processor;
        }
        auto* stages = &p.before;
        for (const auto stage : getColorStages(lut.order))
        {
            OCIO::ConstProcessorRcPtr processor;
            switch (stage)
            {
            case ColorStage::LUT: processor = lutProcessor; break;
            case ColorStage::ToLinear: processor = processors.toLinear; break;
            case ColorStage::Display: processor = processors.display; break;
            case ColorStage::Corrections: stages = &p.after; break;
            }
            if (processor)
            {
                stages->push_back(processor->getDefaultCPUProcessor());
            }
        }
    }

    CPUColor::~CPUColor()
    {}

    bool CPUColor::isDisplayP3() const
    {
        return _p->displayP3;
    }

    void CPUColor::apply(float* rgba, size_t count) const
    {
        FTK_P();
        const DisplayColorValues& v = p.values;
        const auto processors = [rgba, count](
            const std::vector<OCIO::ConstCPUProcessorRcPtr>& value)
        {
            if (!value.empty())
            {
                OCIO::PackedImageDesc desc(rgba, static_cast<long>(count), 1, 4);
                for (const auto& processor : value)
                {
                    processor->apply(desc);
                }
            }
        };

        // In the order of getDisplayShaderBody().
        if (v.negative)
        {
            for (size_t i = 0; i < count; ++i)
            {
                float* c = rgba + i * 4;
                c[0] = 1.F - c[0];
                c[1] = 1.F - c[1];
                c[2] = 1.F - c[2];
            }
        }

        processors(p.before);

        for (size_t i = 0; i < count; ++i)
        {
            float* c = rgba + i * 4;
            if (v.colorEnabled)
            {
                const float in[4] =
                {
                    c[0] + v.colorAdd.x,
                    c[1] + v.colorAdd.y,
                    c[2] + v.colorAdd.z,
                    1.F
                };
                for (int row = 0; row < 3; ++row)
                {
                    c[row] =
                        v.colorMatrix.get(row, 0) * in[0] +
                        v.colorMatrix.get(row, 1) * in[1] +
                        v.colorMatrix.get(row, 2) * in[2] +
                        v.colorMatrix.get(row, 3) * in[3];
                }
            }
            if (v.levelsEnabled)
            {
                c[0] = levelsChannel(c[0], v);
                c[1] = levelsChannel(c[1], v);
                c[2] = levelsChannel(c[2], v);
            }
            if (v.exposureEnabled)
            {
                c[0] *= v.exposure;
                c[1] *= v.exposure;
                c[2] *= v.exposure;
            }
            if (v.softClip > 0.F)
            {
                c[0] = softClipChannel(c[0], v.softClip);
                c[1] = softClipChannel(c[1], v.softClip);
                c[2] = softClipChannel(c[2], v.softClip);
            }
        }

        processors(p.after);

        for (size_t i = 0; i < count; ++i)
        {
            float* c = rgba + i * 4;
            if (p.window && p.displayP3)
            {
                displayP3ToSRGB(c);
            }
            // ftk::ChannelDisplay: color, red, green, blue, alpha.
            switch (v.channels)
            {
            case 1: c[1] = c[2] = c[0]; break;
            case 2: c[0] = c[2] = c[1]; break;
            case 3: c[0] = c[1] = c[2]; break;
            case 4: c[0] = c[1] = c[2] = c[3]; break;
            default: break;
            }
        }
    }

    CPUColorTable::CPUColorTable(const CPUColor& color, int size) :
        _size(std::max(size, 2))
    {
        const size_t n = _size;
        std::vector<float> rgba(n * n * n * 4);
        float* c = rgba.data();
        for (size_t b = 0; b < n; ++b)
        {
            for (size_t g = 0; g < n; ++g)
            {
                for (size_t r = 0; r < n; ++r, c += 4)
                {
                    c[0] = r / static_cast<float>(n - 1);
                    c[1] = g / static_cast<float>(n - 1);
                    c[2] = b / static_cast<float>(n - 1);
                    c[3] = 1.F;
                }
            }
        }
        color.apply(rgba.data(), n * n * n);
        _table.resize(n * n * n * 3);
        for (size_t i = 0; i < n * n * n; ++i)
        {
            for (size_t j = 0; j < 3; ++j)
            {
                // Clamped here, the pixels are bytes: interpolating
                // between values out of range would bring them in where
                // they should not be.
                const float v = rgba[i * 4 + j];
                _table[i * 3 + j] = std::isfinite(v) ? std::clamp(v, 0.F, 1.F) : 0.F;
            }
        }
    }

    CPUColorTable::~CPUColorTable()
    {}

    void CPUColorTable::apply(uint8_t* rgba, size_t count) const
    {
        // Where each byte value falls between the table's points.
        const int n = _size;
        int index[256];
        float frac[256];
        for (int i = 0; i < 256; ++i)
        {
            const float v = i / 255.F * (n - 1);
            index[i] = std::min(static_cast<int>(v), n - 2);
            frac[i] = v - index[i];
        }
        const size_t dg = n * 3;
        const size_t db = n * n * 3;
        for (size_t i = 0; i < count; ++i, rgba += 4)
        {
            const int r = rgba[0];
            const int g = rgba[1];
            const int b = rgba[2];
            const float fr = frac[r];
            const float fg = frac[g];
            const float fb = frac[b];
            const float* c000 = _table.data() + index[b] * db + index[g] * dg + index[r] * 3;
            const float* c100 = c000 + 3;
            const float* c010 = c000 + dg;
            const float* c001 = c000 + db;
            const float* c110 = c100 + dg;
            const float* c101 = c100 + db;
            const float* c011 = c010 + db;
            const float* c111 = c110 + db;

            // Tetrahedral, as OpenColorIO interpolates its own tables: the
            // gray axis is the diagonal of each cell, so a color with two
            // channels the same is made from the corners along it, where
            // across the cell an ACES 2.0 view can have it a long way off
            // -- a bright yellow came out with a third of its blue back.
            const float* p1;
            const float* p2;
            float w0, w1, w2, w3;
            if (fr > fg)
            {
                if (fg > fb)
                {
                    p1 = c100; p2 = c110;
                    w0 = 1.F - fr; w1 = fr - fg; w2 = fg - fb; w3 = fb;
                }
                else if (fr > fb)
                {
                    p1 = c100; p2 = c101;
                    w0 = 1.F - fr; w1 = fr - fb; w2 = fb - fg; w3 = fg;
                }
                else
                {
                    p1 = c001; p2 = c101;
                    w0 = 1.F - fb; w1 = fb - fr; w2 = fr - fg; w3 = fg;
                }
            }
            else
            {
                if (fb > fg)
                {
                    p1 = c001; p2 = c011;
                    w0 = 1.F - fb; w1 = fb - fg; w2 = fg - fr; w3 = fr;
                }
                else if (fb > fr)
                {
                    p1 = c010; p2 = c011;
                    w0 = 1.F - fg; w1 = fg - fb; w2 = fb - fr; w3 = fr;
                }
                else
                {
                    p1 = c010; p2 = c110;
                    w0 = 1.F - fg; w1 = fg - fr; w2 = fr - fb; w3 = fb;
                }
            }
            for (size_t j = 0; j < 3; ++j)
            {
                const float v = w0 * c000[j] + w1 * p1[j] + w2 * p2[j] + w3 * c111[j];
                rgba[j] = static_cast<uint8_t>(std::clamp(v, 0.F, 1.F) * 255.F + .5F);
            }
        }
    }
}
#endif // TLRENDER_OCIO

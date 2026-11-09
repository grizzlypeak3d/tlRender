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
}
#endif // TLRENDER_OCIO

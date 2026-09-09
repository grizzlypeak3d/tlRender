// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Core/Util.h>

#include <ftk/Core/Box.h>
#include <ftk/Core/Image.h>
#include <ftk/Core/Matrix.h>
#include <ftk/Core/RenderOptions.h>

namespace tl
{
    //! Color values.
    struct TL_TIMELINE_API_TYPE Color
    {
        bool     enabled    = false;
        ftk::V3F add        = ftk::V3F(0.F, 0.F, 0.F);
        ftk::V3F brightness = ftk::V3F(1.F, 1.F, 1.F);
        ftk::V3F contrast   = ftk::V3F(1.F, 1.F, 1.F);
        ftk::V3F saturation = ftk::V3F(1.F, 1.F, 1.F);
        float    hue        = 0.F;

        bool operator == (const Color&) const = default;
    };

    //! Get a color matrix.
    TL_TIMELINE_API ftk::M44F color(const Color&);

    //! Levels values.
    struct TL_TIMELINE_API_TYPE Levels
    {
        bool  enabled = false;
        float inLow   = 0.F;
        float inHigh  = 1.F;
        float gamma   = 1.F;
        float outLow  = 0.F;
        float outHigh = 1.F;

        bool operator == (const Levels&) const = default;
    };

    //! A plain stop adjustment: the value scales the image by 2^stops,
    //! so zero is neutral. The old exrdisplay-style defog, knee, and
    //! gamma are gone -- SoftClip and Levels cover them, and the
    //! exrdisplay formula was never neutral at its defaults.
    struct TL_TIMELINE_API_TYPE Exposure
    {
        bool  enabled  = false;
        float exposure = 0.F;

        bool operator == (const Exposure&) const = default;
    };

    //! Soft clip.
    struct TL_TIMELINE_API_TYPE SoftClip
    {
        bool  enabled = false;
        float value   = 0.F;

        bool operator == (const SoftClip&) const = default;
    };

    //! Aspect ratio.
    struct TL_TIMELINE_API_TYPE AspectRatio
    {
        AspectRatio() = default;
        TL_TIMELINE_API explicit AspectRatio(float num, float den = 1.F);

        float num = 1.F;
        float den = 1.F;

        TL_TIMELINE_API bool isValid() const;

        TL_TIMELINE_API operator float () const;

        bool operator == (const AspectRatio&) const = default;
    };

    //! Get a label.
    TL_TIMELINE_API std::string getLabel(const AspectRatio&);

    //! Aspect ratio types.
    enum class TL_TIMELINE_API_TYPE AspectRatioType
    {
        Pixel,
        Display,

        Count,
        First = Pixel
    };
    FTK_ENUM(TL_TIMELINE_API, AspectRatioType);

    //! Aspect ratio options.
    struct TL_TIMELINE_API_TYPE AspectRatioOptions
    {
        AspectRatioOptions() = default;
        TL_TIMELINE_API AspectRatioOptions(const AspectRatio&, AspectRatioType);

        AspectRatio     value = AspectRatio(0.F, 0.F);
        AspectRatioType type  = AspectRatioType::Pixel;

        bool operator == (const AspectRatioOptions&) const = default;
    };

    //! Get the aspect ratio.
    TL_TIMELINE_API float getAspectRatio(
        const ftk::ImageInfo&,
        const AspectRatioOptions&);

    //! Get the render size.
    TL_TIMELINE_API ftk::Size2I getRenderSize(
        const ftk::ImageInfo&,
        const AspectRatioOptions&);

    //! Horizontal box alignment.
    enum class BoxHAlign
    {
        Center,
        Left,
        Right
    };

    //! Vertical box alignment.
    enum class BoxVAlign
    {
        Center,
        Top,
        Bottom
    };

    //! Get a box that fits within the given box.
    TL_TIMELINE_API ftk::Box2I getBox(
        const ftk::Box2I&,
        const ftk::ImageInfo&,
        const AspectRatioOptions&,
        BoxHAlign = BoxHAlign::Center,
        BoxVAlign = BoxVAlign::Center);

    //! Get a label.
    TL_TIMELINE_API std::string getLabel(const AspectRatioOptions&);

    //! Display options.
    struct TL_TIMELINE_API_TYPE DisplayOptions
    {
        ftk::ChannelDisplay channels    = ftk::ChannelDisplay::Color;
        bool                negative    = false;
        ftk::ImageMirror    mirror;
        AspectRatioOptions  aspectRatio;
        Color               color;
        Levels              levels;
        Exposure            exposure;
        SoftClip            softClip;

        //! Per item override of the OCIO input color space; empty uses
        //! OCIOOptions::input. Runtime state resolved per file rather
        //! than a setting, so it is not serialized.
        std::string         ocioInput;

        bool operator == (const DisplayOptions&) const = default;
    };

    //! \name Serialize
    ///@{

    TL_TIMELINE_API void to_json(nlohmann::json&, const Color&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const Levels&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const Exposure&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const SoftClip&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const AspectRatio&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const AspectRatioOptions&);
    TL_TIMELINE_API void to_json(nlohmann::json&, const DisplayOptions&);

    TL_TIMELINE_API void from_json(const nlohmann::json&, Color&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, Levels&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, Exposure&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, SoftClip&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, AspectRatio&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, AspectRatioOptions&);
    TL_TIMELINE_API void from_json(const nlohmann::json&, DisplayOptions&);

    ///@}
}

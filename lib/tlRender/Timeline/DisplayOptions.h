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
        //! Whether the color values are applied.
        bool     enabled    = false;
        //! Added to the red, green, and blue before the other values are
        //! applied; zero does nothing.
        ftk::V3F add        = ftk::V3F(0.F, 0.F, 0.F);
        //! Brightness, a multiplier for each channel; one does nothing.
        ftk::V3F brightness = ftk::V3F(1.F, 1.F, 1.F);
        //! Contrast for each channel, pivoting on 0.5; one does nothing.
        ftk::V3F contrast   = ftk::V3F(1.F, 1.F, 1.F);
        //! Saturation for each channel; one does nothing and zero is gray.
        ftk::V3F saturation = ftk::V3F(1.F, 1.F, 1.F);
        //! Hue rotation in turns: 0.5 is half way round the color wheel, and
        //! zero does nothing.
        float    hue        = 0.F;

        bool operator == (const Color&) const = default;
    };

    //! Get a color matrix.
    TL_TIMELINE_API ftk::M44F color(const Color&);

    //! Levels values.
    struct TL_TIMELINE_API_TYPE Levels
    {
        //! Whether the levels are applied.
        bool  enabled = false;
        //! The input black point: the value that becomes outLow.
        float inLow   = 0.F;
        //! The input white point: the value that becomes outHigh. Values
        //! outside the input range are not clamped.
        float inHigh  = 1.F;
        //! The gamma: after the input range, each value that is not negative
        //! is raised to one over it, so above one brightens the midtones.
        float gamma   = 1.F;
        //! The output black point.
        float outLow  = 0.F;
        //! The output white point.
        float outHigh = 1.F;

        bool operator == (const Levels&) const = default;
    };

    //! A plain stop adjustment: the value scales the image by 2^stops,
    //! so zero is neutral. The old exrdisplay-style defog, knee, and
    //! gamma are gone -- SoftClip and Levels cover them, and the
    //! exrdisplay formula was never neutral at its defaults.
    struct TL_TIMELINE_API_TYPE Exposure
    {
        //! Whether the exposure is applied.
        bool  enabled  = false;
        //! The exposure, in stops.
        float exposure = 0.F;

        bool operator == (const Exposure&) const = default;
    };

    //! Soft clip.
    struct TL_TIMELINE_API_TYPE SoftClip
    {
        //! Whether the soft clip is applied.
        bool  enabled = false;
        //! The amount, from 0 to 1: values above one minus the amount are
        //! rolled off toward one rather than passing it. Zero does nothing.
        float value   = 0.F;

        bool operator == (const SoftClip&) const = default;
    };

    //! Aspect ratio.
    struct TL_TIMELINE_API_TYPE AspectRatio
    {
        AspectRatio() = default;
        TL_TIMELINE_API explicit AspectRatio(float num, float den = 1.F);

        //! The numerator, the width part of the ratio.
        float num = 1.F;
        //! The denominator, the height part of the ratio.
        float den = 1.F;

        //! Whether both parts are positive.
        TL_TIMELINE_API bool isValid() const;

        TL_TIMELINE_API operator float () const;

        bool operator == (const AspectRatio&) const = default;
    };

    //! Get a label.
    TL_TIMELINE_API std::string getLabel(const AspectRatio&);

    //! Parse an aspect ratio as a ratio, "16:9", or as a number, "2.39"
    //! or "2.39:1". Both parts must be positive. Returns false and leaves
    //! the value alone otherwise.
    TL_TIMELINE_API bool parseAspectRatio(const std::string&, AspectRatio&);

    //! Aspect ratio types.
    enum class TL_TIMELINE_API_TYPE AspectRatioType
    {
        //! The value is the pixel aspect ratio, in place of the image's own.
        Pixel,
        //! The value is the aspect ratio of the whole picture.
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

        //! The aspect ratio. One that is not valid, as the default 0:0 is,
        //! leaves the image's own.
        AspectRatio     value = AspectRatio(0.F, 0.F);
        //! Whether the value is a pixel or a picture aspect ratio.
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
        //! The channels shown: the color, or one of red, green, blue and
        //! alpha as gray. Applied last, after the color management.
        ftk::ChannelDisplay channels    = ftk::ChannelDisplay::Color;
        //! Whether the color is inverted, one minus each of red, green and
        //! blue. Applied first, before the other adjustments.
        bool                negative    = false;
        //! How the picture is mirrored: across (x), down (y), or both.
        ftk::ImageMirror    mirror;
        AspectRatioOptions  aspectRatio;
        //! The color adjustments.
        Color               color;
        //! The levels adjustment.
        Levels              levels;
        //! The exposure adjustment.
        Exposure            exposure;
        //! The soft clip.
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

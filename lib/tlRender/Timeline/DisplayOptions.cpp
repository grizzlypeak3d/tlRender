// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/DisplayOptions.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

#include <algorithm>
#include <array>

namespace tl
{
    bool Color::operator == (const Color& other) const
    {
        return
            enabled == other.enabled &&
            add == other.add &&
            brightness == other.brightness &&
            contrast == other.contrast &&
            saturation == other.saturation &&
            hue == other.hue;
    }

    bool Color::operator != (const Color& other) const
    {
        return !(*this == other);
    }

    ftk::M44F color(const Color& in)
    {
        return
            ftk::brightness(in.brightness) *
            ftk::contrast(in.contrast) *
            ftk::saturation(in.saturation) *
            ftk::hue(in.hue);
    }

    bool Levels::operator == (const Levels& other) const
    {
        return
            enabled == other.enabled &&
            inLow == other.inLow &&
            inHigh == other.inHigh &&
            gamma == other.gamma &&
            outLow == other.outLow &&
            outHigh == other.outHigh;
    }

    bool Levels::operator != (const Levels& other) const
    {
        return !(*this == other);
    }

    bool Exposure::operator == (const Exposure& other) const
    {
        return
            enabled == other.enabled &&
            exposure == other.exposure &&
            defog == other.defog &&
            kneeLow == other.kneeLow &&
            kneeHigh == other.kneeHigh &&
            gamma == other.gamma;
    }

    bool Exposure::operator != (const Exposure& other) const
    {
        return !(*this == other);
    }

    bool SoftClip::operator == (const SoftClip& other) const
    {
        return
            enabled == other.enabled &&
            value == other.value;
    }

    bool SoftClip::operator != (const SoftClip& other) const
    {
        return !(*this == other);
    }

    AspectRatio::AspectRatio(float num, float den) :
        num(num),
        den(den)
    {}
    
    bool AspectRatio::isValid() const
    {
        return num > 0.F && den > 0.F;
    }

    AspectRatio::operator float() const
    {
        return den > 0.F ? (num / den) : 0.F;
    }

    bool AspectRatio::operator == (const AspectRatio& other) const
    {
        return
            num == other.num &&
            den == other.den;
    }

    bool AspectRatio::operator != (const AspectRatio& other) const
    {
        return !(*this == other);
    }

    TL_ENUM_IMPL(
        AspectRatioType,
        "Pixel",
        "Display");

    std::string getLabel(const AspectRatio& value)
    {
        return ftk::Format("{0}:{1}").
            arg(value.num).
            arg(value.den);
    }

    AspectRatioOptions::AspectRatioOptions(const AspectRatio& value, AspectRatioType type) :
        value(value),
        type(type)
    {}

    bool AspectRatioOptions::operator == (const AspectRatioOptions& other) const
    {
        return
            value == other.value &&
            type == other.type;
    }

    bool AspectRatioOptions::operator != (const AspectRatioOptions& other) const
    {
        return !(*this == other);
    }

    bool DisplayOptions::operator == (const DisplayOptions& other) const
    {
        return
            channels == other.channels &&
            negative == other.negative &&
            mirror == other.mirror &&
            aspectRatio == other.aspectRatio &&
            color == other.color &&
            levels == other.levels &&
            exposure == other.exposure &&
            softClip == other.softClip;
    }

    bool DisplayOptions::operator != (const DisplayOptions& other) const
    {
        return !(*this == other);
    }

    float getAspectRatio(
        const ftk::ImageInfo& info,
        const AspectRatioOptions& options)
    {
        float out = 0.F;
        if (options.value.isValid())
        {
            switch (options.type)
            {
            case AspectRatioType::Pixel:
                out = ftk::aspectRatio(info.size) * options.value;
                break;
            case AspectRatioType::Display:
                out = options.value;
                break;
            default: break;
            }
        }
        else
        {
            out = info.getAspect();
        }
        return out;
    }

    ftk::Size2I getRenderSize(
        const ftk::ImageInfo& info,
        const AspectRatioOptions& options)
    {
        ftk::Size2I out;
        if (options.value.isValid())
        {
            switch (options.type)
            {
            case AspectRatioType::Pixel:
                out.w = info.size.w * options.value;
                out.h = info.size.h;
                break;
            case AspectRatioType::Display:
                out.w = info.size.h * options.value;
                out.h = info.size.h;
                break;
            default: break;
            }
        }
        else
        {
            out.w = info.size.w * info.pixelAspectRatio;
            out.h = info.size.h;
        }
        return out;
    }

    ftk::Box2I getBox(
        const ftk::Box2I& box,
        const ftk::ImageInfo& info,
        const AspectRatioOptions& options,
        BoxHAlign hAlign,
        BoxVAlign vAlign)
    {
        ftk::Box2I out;
        const ftk::Size2I boxSize = box.size();
        const float boxAspect = ftk::aspectRatio(boxSize);
        const float aspect = getAspectRatio(info, options);
        if (boxAspect > aspect)
        {
            const int w = boxSize.h * aspect;
            const int h = boxSize.h;
            int x = box.min.x;
            switch (hAlign)
            {
                case BoxHAlign::Center:
                    x += boxSize.w / 2.F - (boxSize.h * aspect) / 2.F;
                    break;
                case BoxHAlign::Right:
                    x += boxSize.w - w;
                    break;
                default: break;
            }
            out = ftk::Box2I(x, box.min.y, w, h);
        }
        else
        {
            const int w = boxSize.w;
            const int h = boxSize.w / aspect;
            int y = box.min.y;
            switch (vAlign)
            {
                case BoxVAlign::Center:
                    y += boxSize.h / 2.F - (boxSize.w / aspect) / 2.F;
                    break;
                case BoxVAlign::Bottom:
                    y += boxSize.h - h;
                    break;
                default: break;
            }
            out = ftk::Box2I(box.min.x, y, w, h);
        }
        return out;
    }

    std::string getLabel(const AspectRatioOptions& value)
    {
        // A display aspect ratio is conventionally written as a ratio
        // ("2.39:1"), a pixel aspect ratio as a scalar ("2.00"). Writing a
        // pixel aspect ratio as "2:1" reads like a display ratio, which is
        // how it gets mistaken for one. Entry stays a ratio: broadcast pixel
        // aspect ratios such as 10:11 have no exact decimal.
        if (AspectRatioType::Pixel == value.type)
        {
            return ftk::Format("{0} {1}").
                arg(static_cast<float>(value.value), 2).
                arg(value.type);
        }
        return ftk::Format("{0} {1}").
            arg(getLabel(value.value)).
            arg(value.type);
    }

    void to_json(nlohmann::json& json, const Color& in)
    {
        json["Enabled"] = in.enabled;
        json["Add"] = in.add;
        json["Brightness"] = in.brightness;
        json["Contrast"] = in.contrast;
        json["Saturation"] = in.saturation;
        json["Hue"] = in.hue;
    }

    void to_json(nlohmann::json& json, const Levels& in)
    {
        json["Enabled"] = in.enabled;
        json["InLow"] = in.inLow;
        json["InHigh"] = in.inHigh;
        json["Gamma"] = in.gamma;
        json["OutLow"] = in.outLow;
        json["OutHigh"] = in.outHigh;
    }

    void to_json(nlohmann::json& json, const Exposure& in)
    {
        json["Enabled"] = in.enabled;
        json["Exposure"] = in.exposure;
        json["Defog"] = in.defog;
        json["KneeLow"] = in.kneeLow;
        json["KneeHigh"] = in.kneeHigh;
        json["Gamma"] = in.gamma;
    }

    void to_json(nlohmann::json& json, const SoftClip& in)
    {
        json["Enabled"] = in.enabled;
        json["Value"] = in.value;
    }

    void to_json(nlohmann::json& json, const AspectRatio& in)
    {
        json["Num"] = in.num;
        json["Den"] = in.den;
    }

    void to_json(nlohmann::json& json, const AspectRatioOptions& in)
    {
        json["Value"] = in.value;
        json["Type"] = to_string(in.type);
    }

    void to_json(nlohmann::json& json, const DisplayOptions& in)
    {
        json["Channels"] = to_string(in.channels);
        json["Negative"] = in.negative;
        json["AspectRatio"] = in.aspectRatio;
        json["Mirror"] = in.mirror;
        json["Color"] = in.color;
        json["Levels"] = in.levels;
        json["Exposure"] = in.exposure;
        json["SoftClip"] = in.softClip;
    }

    void from_json(const nlohmann::json& json, Color& out)
    {
        json.at("Enabled").get_to(out.enabled);
        json.at("Add").get_to(out.add);
        json.at("Brightness").get_to(out.brightness);
        json.at("Contrast").get_to(out.contrast);
        json.at("Saturation").get_to(out.saturation);
        json.at("Hue").get_to(out.hue);
    }

    void from_json(const nlohmann::json& json, Levels& out)
    {
        json.at("Enabled").get_to(out.enabled);
        json.at("InLow").get_to(out.inLow);
        json.at("InHigh").get_to(out.inHigh);
        json.at("Gamma").get_to(out.gamma);
        json.at("OutLow").get_to(out.outLow);
        json.at("OutHigh").get_to(out.outHigh);
    }

    void from_json(const nlohmann::json& json, Exposure& out)
    {
        json.at("Enabled").get_to(out.enabled);
        json.at("Exposure").get_to(out.exposure);
        json.at("Defog").get_to(out.defog);
        json.at("KneeLow").get_to(out.kneeLow);
        json.at("KneeHigh").get_to(out.kneeHigh);
        json.at("Gamma").get_to(out.gamma);
    }

    void from_json(const nlohmann::json& json, SoftClip& out)
    {
        json.at("Enabled").get_to(out.enabled);
        json.at("Value").get_to(out.value);
    }

    void from_json(const nlohmann::json& json, AspectRatio& out)
    {
        json.at("Num").get_to(out.num);
        json.at("Den").get_to(out.den);
    }

    void from_json(const nlohmann::json& json, AspectRatioOptions& out)
    {
        json.at("Value").get_to(out.value);
        from_string(json.at("Type").get<std::string>(), out.type);
    }

    void from_json(const nlohmann::json& json, DisplayOptions& out)
    {
        from_string(json.at("Channels").get<std::string>(), out.channels);
        json.at("Negative").get_to(out.negative);
        json.at("Mirror").get_to(out.mirror);
        json.at("AspectRatio").get_to(out.aspectRatio);
        json.at("Color").get_to(out.color);
        json.at("Levels").get_to(out.levels);
        json.at("Exposure").get_to(out.exposure);
        json.at("SoftClip").get_to(out.softClip);
    }
}

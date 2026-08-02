// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/CompareOptions.h>

#include <tlRender/Timeline/DisplayOptions.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/String.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace tl
{
    TL_ENUM_IMPL(
        Compare,
        "None",
        "B",
        "Wipe",
        "Overlay",
        "Difference",
        "Horizontal",
        "Vertical",
        "Tile");

    TL_ENUM_IMPL(
        CompareTime,
        "Relative",
        "Absolute");

    namespace
    {
        //! Get the image information used to lay out each video frame, taken
        //! from the first layer that has an image.
        //!
        //! When the frame has a canvas from the OTIO spatial coordinates, the
        //! canvas size is substituted for the image size. The canvas spans the
        //! whole timeline, so the layout stays the same no matter which clips
        //! are visible or what resolution they were rendered at; the layers are
        //! positioned within it when they are drawn. The canvas already
        //! describes the display geometry, so the pixel aspect ratio is not
        //! applied a second time.
        std::vector<ftk::ImageInfo> getInfos(const std::vector<VideoFrame>& videoFrame)
        {
            std::vector<ftk::ImageInfo> out;
            for (const auto& i : videoFrame)
            {
                ftk::ImageInfo info;
                for (const auto& layer : i.layers)
                {
                    if (layer.image)
                    {
                        info = layer.image->getInfo();
                        break;
                    }
                    else if (layer.imageB)
                    {
                        info = layer.imageB->getInfo();
                        break;
                    }
                }
                if (i.canvasSize.isValid())
                {
                    info.size = i.canvasSize;
                    info.pixelAspectRatio = 1.F;
                }
                out.push_back(info);
            }
            return out;
        }
    }

    bool CompareOptions::operator == (const CompareOptions& other) const
    {
        return
            compare == other.compare &&
            wipeCenter == other.wipeCenter &&
            wipeRotation == other.wipeRotation &&
            overlay == other.overlay &&
            fitToA == other.fitToA;
    }

    bool CompareOptions::operator != (const CompareOptions& other) const
    {
        return !(*this == other);
    }

    std::vector<ftk::Box2I> getBounds(
        const CompareOptions& options,
        const AspectRatioOptions& aspectRatioOptions,
        const std::vector<ftk::ImageInfo>& infos)
    {
        std::vector<ftk::Box2I> out;
        const size_t count = infos.size();
        switch (options.compare)
        {
        case Compare::Horizontal:
        {
            ftk::Size2I size;
            if (count > 0)
            {
                size = getRenderSize(infos[0], aspectRatioOptions);
                out.push_back(ftk::Box2I(0, 0, size.w, size.h));
            }
            if (options.fitToA && count > 1)
            {
                out.push_back(getBox(
                    ftk::Box2I(size.w, 0, size.w, size.h),
                    infos[1],
                    aspectRatioOptions,
                    BoxHAlign::Left));
            }
            else if (count > 1)
            {
                const ftk::Size2I sizeB = getRenderSize(infos[1], aspectRatioOptions);
                out.push_back(ftk::Box2I(size.w, 0, sizeB.w, sizeB.h));
            }
            break;
        }
        case Compare::Vertical:
        {
            ftk::Size2I size;
            if (count > 0)
            {
                size = getRenderSize(infos[0], aspectRatioOptions);
                out.push_back(ftk::Box2I(0, 0, size.w, size.h));
            }
            if (options.fitToA && count > 1)
            {
                out.push_back(getBox(
                    ftk::Box2I(0, size.h, size.w, size.h),
                    infos[1],
                    aspectRatioOptions,
                    BoxHAlign::Center,
                    BoxVAlign::Top));
            }
            else if (count > 1)
            {
                const ftk::Size2I sizeB = getRenderSize(infos[1], aspectRatioOptions);
                out.push_back(ftk::Box2I(0, size.h, sizeB.w, sizeB.h));
            }
            break;
        }
        case Compare::Tile:
            if (count > 0)
            {
                const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(count))));
                ftk::Size2I size;
                if (options.fitToA)
                {
                    size = getRenderSize(infos[0], aspectRatioOptions);
                }
                else
                {
                    for (size_t i = 0; i < count; ++i)
                    {
                        const ftk::Size2I size2 = getRenderSize(infos[i], aspectRatioOptions);
                        size.w = std::max(size.w, size2.w);
                        size.h = std::max(size.h, size2.h);
                    }
                }
                int c = 0;
                int x = 0;
                int y = 0;
                for (size_t i = 0; i < count; ++i)
                {
                    out.push_back(ftk::Box2I(x, y, size.w, size.h));
                    if (++c >= cols)
                    {
                        c = 0;
                        x = 0;
                        y += size.h;
                    }
                    else
                    {
                        x += size.w;
                    }
                }
            }
            break;
        default:
            if (options.fitToA && count > 0)
            {
                const ftk::Size2I size = getRenderSize(infos[0], aspectRatioOptions);
                for (size_t i = 0; i < count; ++i)
                {
                    out.push_back(ftk::Box2I(0, 0, size.w, size.h));
                }
            }
            else if (count > 0)
            {
                for (size_t i = 0; i < count; ++i)
                {
                    const ftk::Size2I size = getRenderSize(infos[i], aspectRatioOptions);
                    out.push_back(ftk::Box2I(0, 0, size.w, size.h));
                }
            }
            break;
        }
        return out;
    }

    std::vector<ftk::Box2I> getBoxes(
        const CompareOptions& options,
        const AspectRatioOptions& aspectRatioOptions,
        const std::vector<ftk::ImageInfo>& infos)
    {
        std::vector<ftk::Box2I> out;
        const std::vector<ftk::Box2I> bounds = getBounds(options, aspectRatioOptions, infos);
        for (size_t i = 0; i < bounds.size() && i < infos.size(); ++i)
        {
            out.push_back(getBox(bounds[i], infos[i], aspectRatioOptions));
        }
        return out;
    }

    std::vector<ftk::Box2I> getBoxes(
        const CompareOptions& options,
        const AspectRatioOptions& aspectRatioOptions,
        const std::vector<VideoFrame>& videoFrame)
    {
        return getBoxes(options, aspectRatioOptions, getInfos(videoFrame));
    }

    ftk::Size2I getRenderSize(
        const CompareOptions& options,
        const AspectRatioOptions& aspectRatioOptions,
        const std::vector<ftk::ImageInfo>& infos)
    {
        ftk::Size2I out;
        const auto bounds = getBounds(options, aspectRatioOptions, infos);
        const auto bbox = ftk::bbox(bounds);
        switch (options.compare)
        {
        case Compare::None:
            if (!bounds.empty())
            {
                out.w = bounds[0].w();
                out.h = bounds[0].h();
            }
            break;
        case Compare::B:
            if (bounds.size() > 1)
            {
                out.w = bounds[1].w();
                out.h = bounds[1].h();
            }
            break;
        default:
            out.w = bbox.w();
            out.h = bbox.h();
            break;
        }
        return out;
    }

    ftk::Size2I getRenderSize(
        const CompareOptions& options,
        const AspectRatioOptions& aspectRatioOptions,
        const std::vector<VideoFrame>& videoFrame)
    {
        return getRenderSize(options, aspectRatioOptions, getInfos(videoFrame));
    }

    OTIO_NS::RationalTime getCompareTime(
        const OTIO_NS::RationalTime& sourceTime,
        const OTIO_NS::TimeRange& sourceTimeRange,
        const OTIO_NS::TimeRange& compareTimeRange,
        CompareTime compare)
    {
        OTIO_NS::RationalTime out;
        switch (compare)
        {
        case CompareTime::Relative:
        {
            const OTIO_NS::RationalTime relativeTime =
                sourceTime - sourceTimeRange.start_time();
            const OTIO_NS::RationalTime relativeTimeRescaled = relativeTime.
                rescaled_to(compareTimeRange.duration().rate()).
                floor();
            out = compareTimeRange.start_time() + relativeTimeRescaled;
            break;
        }
        case CompareTime::Absolute:
            out = sourceTime.
                rescaled_to(compareTimeRange.duration().rate()).
                floor();
            break;
        default: break;
        }
        return out;
    }

    void to_json(nlohmann::json& json, const CompareOptions& in)
    {
        json["Compare"] = to_string(in.compare);
        json["WipeCenter"] = in.wipeCenter;
        json["WipeRotation"] = in.wipeRotation;
        json["Overlay"] = in.overlay;
        json["FitToA"] = in.fitToA;
    }

    void from_json(const nlohmann::json& json, CompareOptions& out)
    {
        from_string(json.at("Compare").get<std::string>(), out.compare);
        json.at("WipeCenter").get_to(out.wipeCenter);
        json.at("WipeRotation").get_to(out.wipeRotation);
        json.at("Overlay").get_to(out.overlay);
        json.at("FitToA").get_to(out.fitToA);
    }
}

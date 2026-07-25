// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/IItem.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

#include <opentimelineio/marker.h>

#include <sstream>

namespace tl
{
    namespace ui
    {
        TL_ENUM_IMPL(
            InOutDisplay,
            "Inside Range",
            "Outside Range");

        TL_ENUM_IMPL(
            CacheDisplay,
            "Video And Audio",
            "Video Only");

        TL_ENUM_IMPL(
            WaveformPrim,
            "Mesh",
            "Image");

        bool ItemOptions::operator == (const ItemOptions& other) const
        {
            return
                inputEnabled == other.inputEnabled;
        }

        bool ItemOptions::operator != (const ItemOptions& other) const
        {
            return !(*this == other);
        }

        bool DisplayOptions::operator == (const DisplayOptions& other) const
        {
            return
                inOutDisplay == other.inOutDisplay &&
                cacheDisplay == other.cacheDisplay &&
                minimize == other.minimize &&
                clipColors == other.clipColors &&
                thumbnails == other.thumbnails &&
                thumbnailHeight == other.thumbnailHeight &&
                waveforms == other.waveforms &&
                waveformWidth == other.waveformWidth &&
                waveformHeight == other.waveformHeight &&
                waveformPrim == other.waveformPrim &&
                clipRectScale == other.clipRectScale &&
                ocio == other.ocio &&
                lut == other.lut;
        }

        bool DisplayOptions::operator != (const DisplayOptions& other) const
        {
            return !(*this == other);
        }

        std::vector<Marker> getMarkers(const OTIO_NS::Item* item)
        {
            std::vector<Marker> out;
            for (const auto& marker : item->markers())
            {
                out.push_back({
                    marker->name(),
                    getMarkerColor(marker->color()),
                    marker->marked_range() });
            }
            return out;
        }

        ftk::Color4F toColor(const OTIO_NS::Color& value)
        {
            return ftk::Color4F(
                static_cast<float>(value.r()),
                static_cast<float>(value.g()),
                static_cast<float>(value.b()),
                static_cast<float>(value.a()));
        }

        ftk::Color4F getMarkerColor(const std::optional<OTIO_NS::Color>& value)
        {
            // OTIO gives markers green by default, so markers that do not
            // carry a color of their own keep the appearance they had.
            return value.has_value() ?
                toColor(value.value()) :
                ftk::Color4F(0.F, 1.F, 0.F);
        }

        ftk::Color4F getItemColor(
            const OTIO_NS::Item* otioItem,
            const ftk::Color4F& defaultColor,
            const DisplayOptions& displayOptions)
        {
            ftk::Color4F out = defaultColor;
            if (displayOptions.clipColors && otioItem)
            {
                if (const auto color = otioItem->color())
                {
                    out = toColor(color.value());
                }
            }
            return out;
        }

        DragDropData::DragDropData(
            const std::shared_ptr<IItem>& item) :
            _item(item)
        {}

        DragDropData::~DragDropData()
        {}

        const std::shared_ptr<IItem>& DragDropData::getItem() const
        {
            return _item;
        }

        struct IItem::Private
        {
            ftk::ColorRole selectRole = ftk::ColorRole::None;
            std::shared_ptr<ftk::Observer<bool> > timeUnitsObserver;
        };

        void IItem::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::string& objectName,
            const OTIO_NS::TimeRange& timeRange,
            const OTIO_NS::TimeRange& availableRange,
            const OTIO_NS::TimeRange& trimmedRange,
            double scale,
            const ItemOptions& options,
            const DisplayOptions& displayOptions,
            const std::shared_ptr<ItemData>& data,
            const std::shared_ptr<IWidget>& parent)
        {
            IMouseWidget::_init(context, objectName, parent);
            FTK_P();

            _timeRange = timeRange;
            _availableRange = availableRange;
            _trimmedRange = trimmedRange;
            _scale = scale;
            _options = options;
            _displayOptions = displayOptions;
            _data = data;

            p.timeUnitsObserver = ftk::Observer<bool>::create(
                data->timeUnitsModel->observeTimeUnitsChanged(),
                [this](bool)
                {
                    _timeUnitsUpdate();
                });
        }

        IItem::IItem() :
            _p(new Private)
        {}

        IItem::~IItem()
        {}

        const OTIO_NS::TimeRange& IItem::getTimeRange() const
        {
            return _timeRange;
        }

        void IItem::setScale(double value)
        {
            if (value == _scale)
                return;
            _scale = value;
            setSizeUpdate();
            setDrawUpdate();
        }

        void IItem::setOptions(const ItemOptions& value)
        {
            _options = value;
        }

        void IItem::setDisplayOptions(const DisplayOptions& value)
        {
            if (value == _displayOptions)
                return;
            _displayOptions = value;
            setSizeUpdate();
            setDrawUpdate();
        }

        ftk::ColorRole IItem::getSelectRole() const
        {
            return _p->selectRole;
        }

        void IItem::setSelectRole(ftk::ColorRole value)
        {
            FTK_P();
            if (value == p.selectRole)
                return;
            p.selectRole = value;
            setDrawUpdate();
        }

        OTIO_NS::RationalTime IItem::posToTime(float value) const
        {
            OTIO_NS::RationalTime out = invalidTime;
            const ftk::Box2I& g = getGeometry();
            if (g.w() > 0)
            {
                const double normalized = (value - g.min.x) /
                    static_cast<double>(_timeRange.duration().rescaled_to(1.0).value() * _scale);
                out = OTIO_NS::RationalTime(
                    _timeRange.start_time() +
                    OTIO_NS::RationalTime(
                        _timeRange.duration().value() * normalized,
                        _timeRange.duration().rate())).
                    round();
                out = ftk::clamp(
                    out,
                    _timeRange.start_time(),
                    _timeRange.end_time_inclusive());
            }
            return out;
        }

        int IItem::timeToPos(const OTIO_NS::RationalTime& value) const
        {
            const ftk::Box2I& g = getGeometry();
            const OTIO_NS::RationalTime t = value - _timeRange.start_time();
            return g.min.x + t.rescaled_to(1.0).value() * _scale;
        }

        ftk::Box2I IItem::_getClipRect(
            const ftk::Box2I& value,
            double scale)
        {
            ftk::Box2I out;
            const ftk::V2I c = ftk::center(value);
            out.min.x = (value.min.x - c.x) * scale + c.x;
            out.min.y = (value.min.y - c.y) * scale + c.y;
            out.max.x = (value.max.x - c.x) * scale + c.x;
            out.max.y = (value.max.y - c.y) * scale + c.y;
            return out;
        }

        std::string IItem::_getDurationLabel(const OTIO_NS::RationalTime& value)
        {
            const OTIO_NS::RationalTime rescaled = value.rescaled_to(_data->speed);
            return ftk::Format("{0}").
                arg(_data->timeUnitsModel->getLabel(rescaled));
        }

        void IItem::_timeUnitsUpdate()
        {}

        void to_json(nlohmann::json& json, const ItemOptions& value)
        {
            json["InputEnabled"] = value.inputEnabled;
        }

        void to_json(nlohmann::json& json, const DisplayOptions& value)
        {
            json["InOutDisplay"] = to_string(value.inOutDisplay);
            json["CacheDisplay"] = to_string(value.cacheDisplay);
            json["Minimize"] = value.minimize;
            json["Thumbnails"] = value.thumbnails;
            json["ThumbnailHeight"] = value.thumbnailHeight;
            json["Waveforms"] = value.waveforms;
            json["WaveformWidth"] = value.waveformWidth;
            json["WaveformHeight"] = value.waveformHeight;
            json["WaveformPrim"] = to_string(value.waveformPrim);
            json["ClipRectScale"] = value.clipRectScale;
            json["OCIO"] = value.ocio;
            json["LUT"] = value.lut;
        }

        void from_json(const nlohmann::json& json, ItemOptions& value)
        {
            json.at("InputEnabled").get_to(value.inputEnabled);
        }

        void from_json(const nlohmann::json& json, DisplayOptions& value)
        {
            from_string(json["InOutDisplay"].get<std::string>(), value.inOutDisplay);
            from_string(json["CacheDisplay"].get<std::string>(), value.cacheDisplay);
            json["Minimize"].get_to(value.minimize);
            json["Thumbnails"].get_to(value.thumbnails);
            json["ThumbnailHeight"].get_to(value.thumbnailHeight);
            json["Waveforms"].get_to(value.waveforms);
            json["WaveformWidth"].get_to(value.waveformWidth);
            json["WaveformHeight"].get_to(value.waveformHeight);
            from_string(json["WaveformPrim"].get<std::string>(), value.waveformPrim);
            json["ClipRectScale"].get_to(value.clipRectScale);
            json["OCIO"].get_to(value.ocio);
            json["LUT"].get_to(value.lut);
        }
    }
}

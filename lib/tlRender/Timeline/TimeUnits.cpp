// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

#include <cmath>
#include <cstdlib>

namespace tl
{
    FTK_ENUM_IMPL(
        TimeUnits,
        "Frames",
        "Seconds",
        "Timecode");

    std::string timeToText(
        const std::optional<OTIO_NS::RationalTime>& time,
        TimeUnits units)
    {
        std::string out;
        switch (units)
        {
        case TimeUnits::Frames:
            out = ftk::Format("{0}").
                arg(time.has_value() ? time->to_frames() : 0);
            break;
        case TimeUnits::Seconds:
            out = ftk::Format("{0}").
                arg(time.has_value() ? time->to_seconds() : 0.0, 2);
            break;
        case TimeUnits::Timecode:
        {
            if (time.has_value())
            {
                out = time->to_timecode();
                if (out.empty())
                {
                    // Timecode counts frames, and only at the rates it has
                    // names for. Audio timed in samples has neither, so the
                    // time itself is given rather than nothing: a file that
                    // is four minutes long should say so.
                    // Rounded whole and split afterwards, so that a time a
                    // fraction under the second does not round up into a
                    // thousandth place that does not exist.
                    const int total = static_cast<int>(
                        std::llround(time->to_seconds() * 1000.0));
                    out = ftk::Format("{0}:{1}:{2}.{3}").
                        arg(total / 3600000, 2, '0').
                        arg(total / 60000 % 60, 2, '0').
                        arg(total / 1000 % 60, 2, '0').
                        arg(total % 1000, 3, '0');
                }
            }
            if (out.empty())
            {
                out = "--:--:--:--";
            }
            break;
        }
        default: break;
        }
        return out;
    }

    std::optional<OTIO_NS::RationalTime> textToTime(
        const std::string& text,
        double rate,
        TimeUnits units,
        opentime::ErrorStatus* errorStatus)
    {
        std::optional<OTIO_NS::RationalTime> out;
        // Whether the whole string was a number, which atoi() and atof() do
        // not say: they answer 0 for text that is not one at all, so "abc"
        // would read as frame zero.
        const auto consumed =
            [&text](const char* end)
            {
                return
                    end != text.c_str() &&
                    end == text.c_str() + text.size();
            };
        switch (units)
        {
        case TimeUnits::Frames:
        {
            char* end = nullptr;
            const long value = std::strtol(text.c_str(), &end, 10);
            if (consumed(end))
            {
                out = OTIO_NS::RationalTime::from_frames(value, rate);
            }
            break;
        }
        case TimeUnits::Seconds:
        {
            char* end = nullptr;
            const double value = std::strtod(text.c_str(), &end);
            if (consumed(end))
            {
                out = OTIO_NS::RationalTime::from_seconds(value).rescaled_to(rate);
            }
            break;
        }
        case TimeUnits::Timecode:
            out = OTIO_NS::RationalTime::from_timecode(text, rate, errorStatus);
            break;
        default: break;
        }
        if (out.has_value() && out->is_invalid_time())
        {
            out.reset();
        }
        return out;
    }

    std::string formatString(TimeUnits units)
    {
        std::string out;
        switch (units)
        {
        case TimeUnits::Frames:
            out = "000000";
            break;
        case TimeUnits::Seconds:
            out = "000000.00";
            break;
        case TimeUnits::Timecode:
            out = "00:00:00;00";
            break;
        default: break;
        }
        return out;
    }

    std::string validator(TimeUnits units)
    {
        std::string out;
        switch (units)
        {
        case TimeUnits::Frames:
            out = "[0-9]*";
            break;
        case TimeUnits::Seconds:
            out = "[0-9]*\\.[0-9]+|[0-9]+";
            break;
        case TimeUnits::Timecode:
            out = "[0-9][0-9]:[0-9][0-9]:[0-9][0-9]:[0-9][0-9]";
            break;
        default: break;
        }
        return out;
    }

    void ITimeUnitsModel::_init(const std::shared_ptr<ftk::Context>& context)
    {
        _timeUnitsChanged = ftk::Observable<bool>::create();
    }

    ITimeUnitsModel::ITimeUnitsModel()
    {}

    ITimeUnitsModel::~ITimeUnitsModel()
    {}

    std::shared_ptr<ftk::IObservable<bool> > ITimeUnitsModel::observeTimeUnitsChanged() const
    {
        return _timeUnitsChanged;
    }

    struct TimeUnitsModel::Private
    {
        std::shared_ptr<ftk::Observable<TimeUnits> > timeUnits;
    };

    void TimeUnitsModel::_init(const std::shared_ptr<ftk::Context>& context)
    {
        FTK_P();
        ITimeUnitsModel::_init(context);
        p.timeUnits = ftk::Observable<TimeUnits>::create(TimeUnits::Timecode);
    }

    TimeUnitsModel::TimeUnitsModel() :
        _p(new Private)
    {}

    TimeUnitsModel::~TimeUnitsModel()
    {}

    std::shared_ptr<TimeUnitsModel> TimeUnitsModel::create(
        const std::shared_ptr<ftk::Context>& context)
    {
        auto out = std::shared_ptr<TimeUnitsModel>(new TimeUnitsModel);
        out->_init(context);
        return out;
    }

    TimeUnits TimeUnitsModel::getTimeUnits() const
    {
        return _p->timeUnits->get();
    }

    std::shared_ptr<ftk::IObservable<TimeUnits> > TimeUnitsModel::observeTimeUnits() const
    {
        return _p->timeUnits;
    }

    void TimeUnitsModel::setTimeUnits(TimeUnits value)
    {
        if (_p->timeUnits->setIfChanged(value))
        {
            _timeUnitsChanged->setAlways(true);
        }
    }

    std::string TimeUnitsModel::getLabel(
        const std::optional<OTIO_NS::RationalTime>& value) const
    {
        return timeToText(value, _p->timeUnits->get());
    }
}

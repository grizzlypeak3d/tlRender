// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Core/Time.h>
#include <tlRender/Core/Util.h>

#include <ftk/Core/Observable.h>

namespace ftk
{
    class Context;
}

namespace tl
{
    //! Time units.
    enum class TL_TIMELINE_API_TYPE TimeUnits
    {
        Frames,
        Seconds,
        Timecode,

        Count,
        First = Frames
    };
    FTK_ENUM(TL_TIMELINE_API, TimeUnits);

    //! Convert a time value to text. An unset time gives the placeholder
    //! text for the units, so a widget with no value still reads as a time.
    //! Timecode is drop frame only when asked for: 29.97 is not drop frame
    //! unless the media says so.
    TL_TIMELINE_API std::string timeToText(
        const std::optional<OTIO_NS::RationalTime>&,
        TimeUnits,
        bool dropFrame = false);

    //! Get whether a timecode is drop frame, which the ';' before the
    //! frames says.
    TL_TIMELINE_API bool isDropFrame(const std::string& timecode);

    //! Convert text to a time value. Unset when the text does not parse.
    TL_TIMELINE_API std::optional<OTIO_NS::RationalTime> textToTime(
        const std::string&     text,
        double                 rate,
        TimeUnits              units,
        opentime::ErrorStatus* error = nullptr);

    //! Get a time units format string.
    TL_TIMELINE_API std::string formatString(TimeUnits);

    //! Get a time units validator regular expression.
    TL_TIMELINE_API std::string validator(TimeUnits);

    //! Base class for time units models.
    class TL_TIMELINE_API_TYPE ITimeUnitsModel : public std::enable_shared_from_this<ITimeUnitsModel>
    {
        FTK_NON_COPYABLE(ITimeUnitsModel);

    protected:
        TL_TIMELINE_API void _init(const std::shared_ptr<ftk::Context>&);

        ITimeUnitsModel();

    public:
        TL_TIMELINE_API virtual ~ITimeUnitsModel() = 0;

        //! Observe when the time units are changed.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<bool> > observeTimeUnitsChanged() const;

        //! Get a time label in the current time units. An unset time gives
        //! the placeholder text for the units.
        TL_TIMELINE_API virtual std::string getLabel(
            const std::optional<OTIO_NS::RationalTime>&) const = 0;

        //! Get a label for a duration, which no start is taken from.
        TL_TIMELINE_API virtual std::string getDurationLabel(
            const OTIO_NS::RationalTime&) const;

        //! Get whether times are shown counted from the start (see
        //! TimeUnitsModel::setStartAtZero()), in place of the media's own.
        TL_TIMELINE_API virtual bool isStartAtZero() const;

    protected:
        std::shared_ptr<ftk::Observable<bool> > _timeUnitsChanged;
    };

    //! Time units model.
    class TL_TIMELINE_API_TYPE TimeUnitsModel : public ITimeUnitsModel
    {
        FTK_NON_COPYABLE(TimeUnitsModel);

    protected:
        TL_TIMELINE_API void _init(const std::shared_ptr<ftk::Context>&);

        TL_TIMELINE_API TimeUnitsModel();

    public:
        TL_TIMELINE_API virtual ~TimeUnitsModel();

        //! Create a new model.
        TL_TIMELINE_API static std::shared_ptr<TimeUnitsModel> create(
            const std::shared_ptr<ftk::Context>&);

        //! Get the time units.
        TL_TIMELINE_API TimeUnits getTimeUnits() const;

        //! Observe the time units.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<TimeUnits> > observeTimeUnits() const;
            
        //! Set the time units.
        TL_TIMELINE_API void setTimeUnits(TimeUnits);

        //! Get whether timecode is shown as drop frame.
        TL_TIMELINE_API bool isDropFrame() const;

        //! Set whether timecode is shown as drop frame, from the media.
        TL_TIMELINE_API void setDropFrame(bool);

        //! Observe whether times are shown counted from the start.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<bool> > observeStartAtZero() const;

        //! Set whether times are shown counted from the start: the first
        //! frame 0, 0.0 seconds, or 00:00:00:00, rather than the frame or
        //! timecode the media gives it. For someone told "at 20 seconds"
        //! by a player that counts from zero, as most do. Text typed in is
        //! read the same way. Durations are not changed.
        TL_TIMELINE_API void setStartAtZero(bool);

        //! Get the start the times are counted from.
        TL_TIMELINE_API const std::optional<OTIO_NS::RationalTime>& getStart() const;

        //! Set the start the times are counted from: the start of the
        //! timeline being shown, or nothing.
        TL_TIMELINE_API void setStart(const std::optional<OTIO_NS::RationalTime>&);

        //! A time as it is shown: counted from the start when that is on.
        TL_TIMELINE_API OTIO_NS::RationalTime toDisplay(const OTIO_NS::RationalTime&) const;

        //! A time as it was shown, back to the timeline's.
        TL_TIMELINE_API OTIO_NS::RationalTime fromDisplay(const OTIO_NS::RationalTime&) const;

        TL_TIMELINE_API std::string getLabel(
            const std::optional<OTIO_NS::RationalTime>&) const override;
        TL_TIMELINE_API std::string getDurationLabel(
            const OTIO_NS::RationalTime&) const override;
        TL_TIMELINE_API bool isStartAtZero() const override;

    private:
        FTK_PRIVATE();
    };
}

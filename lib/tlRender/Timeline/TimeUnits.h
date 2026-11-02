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

        TL_TIMELINE_API std::string getLabel(
            const std::optional<OTIO_NS::RationalTime>&) const override;

    private:
        FTK_PRIVATE();
    };
}

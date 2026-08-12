// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/TimeUnitsTest.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>

namespace tl
{
    namespace timeline_tests
    {
        TimeUnitsTest::TimeUnitsTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::TimeUnitsTest")
        {}

        std::shared_ptr<TimeUnitsTest> TimeUnitsTest::create(
            const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<TimeUnitsTest>(new TimeUnitsTest(context));
        }

        void TimeUnitsTest::run()
        {
            _convert();
            _model();
        }

        void TimeUnitsTest::_convert()
        {
            const OTIO_NS::RationalTime time(24.0, 24.0);
            for (auto units : getTimeUnitsEnums())
            {
                FTK_CHECK(!formatString(units).empty());
                FTK_CHECK(!validator(units).empty());

                // A time and no time both give text, so that a widget with
                // nothing in it still reads as a time.
                const std::string text = timeToText(time, units);
                FTK_CHECK(!text.empty());
                FTK_CHECK(!timeToText(std::nullopt, units).empty());
                _print(ftk::Format("{0}: \"{1}\", empty \"{2}\"").
                    arg(units).
                    arg(text).
                    arg(timeToText(std::nullopt, units)));

                // And back again.
                const auto time2 = textToTime(text, 24.0, units);
                FTK_CHECK(time2.has_value());
                FTK_CHECK(time2->rescaled_to(24.0).round() == time);

                // Text that is not a time.
                opentime::ErrorStatus error;
                textToTime("not a time", 24.0, units, &error);
            }

            // Timecode of a time with no timecode rate: audio counted in
            // samples has neither frames nor a rate with a name, so the time
            // itself is given rather than nothing.
            const std::string audio = timeToText(
                OTIO_NS::RationalTime(48000.0 * 90.0, 48000.0),
                TimeUnits::Timecode);
            FTK_CHECK(!audio.empty());
            _print(ftk::Format("Ninety seconds of audio: \"{0}\"").arg(audio));
        }

        void TimeUnitsTest::_model()
        {
            auto model = TimeUnitsModel::create(_context);
            FTK_CHECK(model->observeTimeUnits());

            bool changed = false;
            auto observer = ftk::Observer<bool>::create(
                model->observeTimeUnitsChanged(),
                [&changed](bool) { changed = true; });

            const TimeUnits units = model->getTimeUnits();
            const TimeUnits other = TimeUnits::Frames == units ?
                TimeUnits::Timecode :
                TimeUnits::Frames;
            model->setTimeUnits(other);
            FTK_CHECK(other == model->getTimeUnits());
            FTK_CHECK(changed);

            // Setting the units it already has changes nothing.
            changed = false;
            model->setTimeUnits(other);
            FTK_CHECK(!changed);

            FTK_CHECK(!model->getLabel(OTIO_NS::RationalTime(0.0, 24.0)).empty());
            FTK_CHECK(!model->getLabel(std::nullopt).empty());
        }
    }
}

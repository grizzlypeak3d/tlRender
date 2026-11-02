// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/TimeUnitsTest.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>

#include <string>
#include <vector>

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

            // Timecode fields that are not two digits wide: OTIO reads
            // them at fixed positions, which drops the seconds here.
            {
                const auto time = textToTime("1:00:40:00", 24.0, TimeUnits::Timecode);
                FTK_CHECK(time.has_value());
                FTK_CHECK(time->value() == 87360.0);
                const auto time2 = textToTime("01:0:40:0", 24.0, TimeUnits::Timecode);
                FTK_CHECK(time2.has_value());
                FTK_CHECK(time2->value() == 87360.0);
                const auto time3 = textToTime(" 01:00:40:00 ", 24.0, TimeUnits::Timecode);
                FTK_CHECK(time3.has_value());
                FTK_CHECK(time3->value() == 87360.0);
            }
            for (const auto& text : std::vector<std::string>{
                "",
                "01:00:40",
                "01:00:40:00:00",
                "01:00:40:0x",
                "01:00:40:000",
                "01::40:00",
                "01:00 :40:00" })
            {
                opentime::ErrorStatus error;
                FTK_CHECK(!textToTime(text, 24.0, TimeUnits::Timecode, &error).has_value());
                FTK_CHECK(opentime::is_error(error));
            }

            // Drop frame timecode keeps its ';' separator.
            {
                const auto time = textToTime("1:00:00;02", 30000.0 / 1001.0, TimeUnits::Timecode);
                FTK_CHECK(time.has_value());
                FTK_CHECK(time == OTIO_NS::RationalTime::from_timecode("01:00:00;02", 30000.0 / 1001.0));
            }

            // 29.97 timecode is drop frame only when asked for: OTIO's
            // default makes it drop frame from the rate alone (DJV #922).
            {
                const double rate = 30000.0 / 1001.0;
                const auto ndf = textToTime("01:00:00:00", rate, TimeUnits::Timecode);
                FTK_CHECK(ndf.has_value());
                FTK_CHECK("01:00:00:00" == timeToText(ndf, TimeUnits::Timecode));
                const auto df = textToTime("01:00:00;00", rate, TimeUnits::Timecode);
                FTK_CHECK(df.has_value());
                FTK_CHECK(ndf != df);
                FTK_CHECK("01:00:00;00" == timeToText(df, TimeUnits::Timecode, true));
                FTK_CHECK(isDropFrame("01:00:00;00"));
                FTK_CHECK(!isDropFrame("01:00:00:00"));
                FTK_CHECK(!isDropFrame(""));
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

            // Drop frame is off until the media says so, and changes the
            // labels.
            model->setTimeUnits(TimeUnits::Timecode);
            const OTIO_NS::RationalTime time = OTIO_NS::RationalTime::from_timecode(
                "01:00:00;00",
                30000.0 / 1001.0);
            FTK_CHECK(!model->isDropFrame());
            FTK_CHECK(!isDropFrame(model->getLabel(time)));
            changed = false;
            model->setDropFrame(true);
            FTK_CHECK(model->isDropFrame());
            FTK_CHECK(changed);
            FTK_CHECK("01:00:00;00" == model->getLabel(time));
            changed = false;
            model->setDropFrame(true);
            FTK_CHECK(!changed);
        }
    }
}

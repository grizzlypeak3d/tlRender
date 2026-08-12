// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CoreTest/TimeTest.h>

#include <tlRender/Core/Time.h>

#include <ftk/Core/Assert.h>

#include <sstream>

namespace tl
{
    namespace core_tests
    {
        TimeTest::TimeTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "core_tests::TimeTest")
        {}

        std::shared_ptr<TimeTest> TimeTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<TimeTest>(new TimeTest(context));
        }

        void TimeTest::run()
        {
            _otime();
            _util();
            _keycode();
            _timecode();
            _serialize();
        }
        
        void TimeTest::_otime()
        {
            {
                const OTIO_NS::TimeRange a(
                    OTIO_NS::RationalTime(24.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0));
                FTK_CHECK(compareExact(a, a));
                const OTIO_NS::TimeRange b(
                    OTIO_NS::RationalTime(1.0, 1.0),
                    OTIO_NS::RationalTime(1.0, 1.0));
                // The rescaling "==" is what compareExact() exists to avoid,
                // and what makes an unset time unsafe to spell as a value.
                FTK_CHECK(a == b);
                FTK_CHECK(!compareExact(a, b));
            }
            {
                // The same for optional times, where either side may be unset.
                const std::optional<OTIO_NS::RationalTime> a(
                    OTIO_NS::RationalTime(24.0, 24.0));
                const std::optional<OTIO_NS::RationalTime> b(
                    OTIO_NS::RationalTime(1.0, 1.0));
                const std::optional<OTIO_NS::RationalTime> unset;
                FTK_CHECK(compareExact(a, a));
                FTK_CHECK(a == b);
                FTK_CHECK(!compareExact(a, b));
                FTK_CHECK(compareExact(unset, unset));
                FTK_CHECK(!compareExact(a, unset));
                FTK_CHECK(!compareExact(unset, a));
            }
            {
                const std::optional<OTIO_NS::TimeRange> a(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(24.0, 24.0),
                    OTIO_NS::RationalTime(24.0, 24.0)));
                const std::optional<OTIO_NS::TimeRange> b(OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(1.0, 1.0),
                    OTIO_NS::RationalTime(1.0, 1.0)));
                const std::optional<OTIO_NS::TimeRange> unset;
                FTK_CHECK(compareExact(a, a));
                FTK_CHECK(a == b);
                FTK_CHECK(!compareExact(a, b));
                FTK_CHECK(compareExact(unset, unset));
                FTK_CHECK(!compareExact(a, unset));
            }
        }
        
        void TimeTest::_util()
        {
            {
                struct Data
                {
                    OTIO_NS::TimeRange range;
                    std::vector<OTIO_NS::RationalTime> frames;
                };
                const std::vector<Data> data =
                {
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(-1.0, -1.0),
                            OTIO_NS::RationalTime(-1.0, -1.0)),
                        {} }),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(0.0, 24.0),
                            OTIO_NS::RationalTime(1.0, 24.0)),
                        {
                            OTIO_NS::RationalTime(0.0, 24.0)
                        }}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(0.0, 24.0),
                            OTIO_NS::RationalTime(3.0, 24.0)),
                        {
                            OTIO_NS::RationalTime(0.0, 24.0),
                            OTIO_NS::RationalTime(1.0, 24.0),
                            OTIO_NS::RationalTime(2.0, 24.0)
                        }}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(0.0, 1.0),
                            OTIO_NS::RationalTime(1.0, 1.0)),
                        {
                            OTIO_NS::RationalTime(0.0, 1.0)
                        }}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(0.0, 1.0),
                            OTIO_NS::RationalTime(3.0, 1.0)),
                        {
                            OTIO_NS::RationalTime(0.0, 1.0),
                            OTIO_NS::RationalTime(1.0, 1.0),
                            OTIO_NS::RationalTime(2.0, 1.0)
                        }})
                };
                for (const auto& i : data)
                {
                    const auto frames = tl::frames(i.range);
                    FTK_CHECK(frames == i.frames);
                }
            }
            {
                struct Data
                {
                    OTIO_NS::TimeRange range;
                    std::vector<OTIO_NS::TimeRange> seconds;
                };
                const std::vector<Data> data =
                {
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(-1.0, -1.0),
                            OTIO_NS::RationalTime(-1.0, -1.0)),
                        {}}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(0.0, 24.0),
                            OTIO_NS::RationalTime(24.0, 24.0)),
                        {
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(0.0, 24.0),
                                OTIO_NS::RationalTime(24.0, 24.0))
                        }}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(0.0, 24.0),
                            OTIO_NS::RationalTime(72.0, 24.0)),
                        {
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(0.0, 24.0),
                                OTIO_NS::RationalTime(24.0, 24.0)),
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(24.0, 24.0),
                                OTIO_NS::RationalTime(24.0, 24.0)),
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(48.0, 24.0),
                                OTIO_NS::RationalTime(24.0, 24.0))
                        }}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(12.0, 24.0),
                            OTIO_NS::RationalTime(12.0, 24.0)),
                        {
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(12.0, 24.0),
                                OTIO_NS::RationalTime(12.0, 24.0))
                        }}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(12.0, 24.0),
                            OTIO_NS::RationalTime(24.0, 24.0)),
                        {
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(12.0, 24.0),
                                OTIO_NS::RationalTime(12.0, 24.0)),
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(24.0, 24.0),
                                OTIO_NS::RationalTime(12.0, 24.0))
                        }}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(23.0, 24.0),
                            OTIO_NS::RationalTime(24.0, 24.0)),
                        {
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(23.0, 24.0),
                                OTIO_NS::RationalTime(1.0, 24.0)),
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(24.0, 24.0),
                                OTIO_NS::RationalTime(23.0, 24.0))
                        }}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(-1.0, 24.0),
                            OTIO_NS::RationalTime(24.0, 24.0)),
                        {
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(-1.0, 24.0),
                                OTIO_NS::RationalTime(1.0, 24.0)),
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(0.0, 24.0),
                                OTIO_NS::RationalTime(23.0, 24.0))
                        }}),
                    Data({
                        OTIO_NS::TimeRange(
                            OTIO_NS::RationalTime(-1.0, 24.0),
                            OTIO_NS::RationalTime(48.0, 24.0)),
                        {
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(-1.0, 24.0),
                                OTIO_NS::RationalTime(1.0, 24.0)),
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(0.0, 24.0),
                                OTIO_NS::RationalTime(24.0, 24.0)),
                            OTIO_NS::TimeRange(
                                OTIO_NS::RationalTime(24.0, 24.0),
                                OTIO_NS::RationalTime(23.0, 24.0))
                        }})
                };
                for (const auto& i : data)
                {
                    const auto seconds = tl::seconds(i.range);
                    FTK_CHECK(seconds == i.seconds);
                }
            }
            {
                struct Data
                {
                    double rate = 0.0;
                    std::pair<int, int> rational;
                };
                const std::vector<Data> data =
                {
                    Data({ 0.0, std::make_pair(0, 1)}),
                    Data({ 24.0, std::make_pair(24, 1)}),
                    Data({ 30.0, std::make_pair(30, 1)}),
                    Data({ 60.0, std::make_pair(60, 1)}),
                    Data({ 23.97602397602398, std::make_pair(24000, 1001)}),
                    Data({ 29.97002997002997, std::make_pair(30000, 1001)}),
                    Data({ 59.94005994005994, std::make_pair(60000, 1001)}),
                    Data({ 23.98, std::make_pair(24000, 1001)}),
                    Data({ 29.97, std::make_pair(30000, 1001)}),
                    Data({ 59.94, std::make_pair(60000, 1001)})
                };
                for (const auto& i : data)
                {
                    const auto rational = toRational(i.rate);
                    FTK_CHECK(
                        rational.first == i.rational.first &&
                        rational.second == i.rational.second);
                }
            }
        }
        
        void TimeTest::_keycode()
        {
            {
                const std::string s = keycodeToString(1, 2, 3, 4, 5);
                int id = 0;
                int type = 0;
                int prefix = 0;
                int count = 0;
                int offset = 0;
                stringToKeycode(s, id, type, prefix, count, offset);
                FTK_CHECK(1 == id);
                FTK_CHECK(2 == type);
                FTK_CHECK(3 == prefix);
                FTK_CHECK(4 == count);
                FTK_CHECK(5 == offset);
            }
            try
            {
                const std::string s = "...";
                int id = 0;
                int type = 0;
                int prefix = 0;
                int count = 0;
                int offset = 0;
                stringToKeycode(s, id, type, prefix, count, offset);
                FTK_CHECK(false);
            }
            catch (const std::exception&)
            {}
        }
        
        void TimeTest::_timecode()
        {
            {
                const uint32_t t = timeToTimecode(1, 2, 3, 4);
                int hour = 0;
                int minute = 0;
                int second = 0;
                int frame = 0;
                timecodeToTime(t, hour, minute, second, frame);
                FTK_CHECK(1 == hour);
                FTK_CHECK(2 == minute);
                FTK_CHECK(3 == second);
                FTK_CHECK(4 == frame);
            }
            {
                const std::string s = "01:02:03:04";
                uint32_t t = 0;
                stringToTimecode(s, t);
                FTK_CHECK(s == timecodeToString(t));
            }
            try
            {
                const std::string s = "...";
                uint32_t t = 0;
                stringToTimecode(s, t);
                FTK_CHECK(false);
            }
            catch (const std::exception&)
            {}
        }

        void TimeTest::_serialize()
        {
            {
                const auto t = OTIO_NS::RationalTime(1.0, 24.0);
                const std::string s = opentime::OPENTIME_VERSION_NS::to_string(t);
                OTIO_NS::RationalTime t2;
                from_string(s, t2);
                FTK_CHECK(t == t2);
            }
            {
                const auto t = OTIO_NS::TimeRange(OTIO_NS::RationalTime(0.0, 24.0), OTIO_NS::RationalTime(1.0, 24.0));
                const std::string s = opentime::OPENTIME_VERSION_NS::to_string(t);
                OTIO_NS::TimeRange t2;
                from_string(s, t2);
                FTK_CHECK(t == t2);
            }
            {
                const OTIO_NS::RationalTime t(1.0, 24.0);
                nlohmann::json json;
                to_json(json, t);
                OTIO_NS::RationalTime t2;
                from_json(json, t2);
                FTK_CHECK(t == t2);
            }
            {
                // The stream operators, which is how a time reaches a log or
                // a format string.
                const OTIO_NS::RationalTime t(1.0, 24.0);
                std::stringstream ss;
                ss << t;
                FTK_CHECK(!ss.str().empty());
                FTK_CHECK(to_string(t) == ss.str());
            }
            {
                const auto t = OTIO_NS::TimeRange(
                    OTIO_NS::RationalTime(0.0, 24.0),
                    OTIO_NS::RationalTime(1.0, 24.0));
                std::stringstream ss;
                ss << t;
                FTK_CHECK(!ss.str().empty());
                FTK_CHECK(to_string(t) == ss.str());
            }
            {
                // Parsing a time from the command line, which takes the
                // argument out of the list when it parses.
                std::vector<std::string> args = { "1.0/24.0", "keep" };
                auto it = args.begin();
                OTIO_NS::RationalTime t;
                FTK_CHECK(cmdLineParse(args, it, t));
                FTK_CHECK(OTIO_NS::RationalTime(1.0, 24.0) == t);
                FTK_CHECK(1 == args.size());

                // And left alone when it does not.
                args = { "not a time" };
                it = args.begin();
                FTK_CHECK(!cmdLineParse(args, it, t));
                FTK_CHECK(1 == args.size());

                // The end of the list is not an error, just nothing to take.
                args.clear();
                it = args.begin();
                FTK_CHECK(!cmdLineParse(args, it, t));
            }
            {
                std::vector<std::string> args = { "0.0/24.0/24.0", "keep" };
                auto it = args.begin();
                OTIO_NS::TimeRange r;
                FTK_CHECK(cmdLineParse(args, it, r));
                FTK_CHECK(24.0 == r.duration().rate());
                FTK_CHECK(1 == args.size());

                args = { "not a range" };
                it = args.begin();
                FTK_CHECK(!cmdLineParse(args, it, r));
                FTK_CHECK(1 == args.size());

                args.clear();
                it = args.begin();
                FTK_CHECK(!cmdLineParse(args, it, r));
            }
            {
                const auto t = OTIO_NS::TimeRange(OTIO_NS::RationalTime(0.0, 24.0), OTIO_NS::RationalTime(1.0, 24.0));
                nlohmann::json json;
                to_json(json, t);
                OTIO_NS::TimeRange t2;
                from_json(json, t2);
                FTK_CHECK(t == t2);
            }
       }
    }
}

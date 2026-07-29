// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Core/Time.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/Math.h>
#include <ftk/Core/String.h>

#include <array>
#include <sstream>

namespace tl
{
    std::vector<OTIO_NS::RationalTime> frames(const OTIO_NS::TimeRange& value)
    {
        std::vector<OTIO_NS::RationalTime> out;
        if (isValid(value))
        {
            const auto start = value.start_time();
            const auto end = value.end_time_exclusive();
            const auto inc = OTIO_NS::RationalTime(1.0, value.duration().rate());
            for (auto time = start; time < end; time += inc)
            {
                out.push_back(time);
            }
        }
        return out;
    }

    std::vector<OTIO_NS::TimeRange> seconds(const OTIO_NS::TimeRange& value)
    {
        std::vector<OTIO_NS::TimeRange> out;
        if (isValid(value))
        {
            const OTIO_NS::TimeRange seconds(
                value.start_time().rescaled_to(1.0),
                value.duration().rescaled_to(1.0));
            for (double t = std::floor(seconds.start_time().value());
                t < std::ceil(seconds.end_time_exclusive().value());
                t += 1.0)
            {
                const double start = std::max(t, seconds.start_time().value());
                const double end = std::min(t + 1.0, seconds.end_time_exclusive().value());
                const OTIO_NS::TimeRange second(
                    OTIO_NS::RationalTime(start, 1.0),
                    OTIO_NS::RationalTime(end - start, 1.0));
                const OTIO_NS::TimeRange scaled(
                    second.start_time().rescaled_to(value.duration().rate()),
                    second.duration().rescaled_to(value.duration().rate()));
                out.push_back(scaled);
            }
        }
        return out;
    }

    std::pair<int, int> toRational(double value)
    {
        const std::array<std::pair<int, int>, 6> common =
        {
            std::make_pair(24, 1),
            std::make_pair(30, 1),
            std::make_pair(60, 1),
            std::make_pair(24000, 1001),
            std::make_pair(30000, 1001),
            std::make_pair(60000, 1001)
        };
        const double tolerance = 0.01;
        for (const auto& i : common)
        {
            const double diff = fabs(value - i.first / static_cast<double>(i.second));
            if (diff < tolerance)
            {
                return i;
            }
        }
        return std::make_pair(static_cast<int>(value), 1);
    }

    std::string keycodeToString(
        int id,
        int type,
        int prefix,
        int count,
        int offset)
    {
        std::vector<std::string> list;
        list.push_back(std::to_string(id));
        list.push_back(std::to_string(type));
        list.push_back(std::to_string(prefix));
        list.push_back(std::to_string(count));
        list.push_back(std::to_string(offset));
        return ftk::join(list, ":");
    }

    void stringToKeycode(
        const std::string& string,
        int& id,
        int& type,
        int& prefix,
        int& count,
        int& offset)
    {
        const auto pieces = ftk::split(string, ':');
        if (pieces.size() != 5)
        {
            throw ftk::ParseError();
        }
        id = std::stoi(pieces[0]);
        type = std::stoi(pieces[1]);
        prefix = std::stoi(pieces[2]);
        count = std::stoi(pieces[3]);
        offset = std::stoi(pieces[4]);
    }

    void timecodeToTime(
        uint32_t in,
        int& hour,
        int& minute,
        int& seconds,
        int& frame)
    {
        hour = (in >> 28 & 0x0f) * 10 + (in >> 24 & 0x0f);
        minute = (in >> 20 & 0x0f) * 10 + (in >> 16 & 0x0f);
        seconds = (in >> 12 & 0x0f) * 10 + (in >> 8 & 0x0f);
        frame = (in >> 4 & 0x0f) * 10 + (in >> 0 & 0x0f);
    }

    uint32_t timeToTimecode(
        int hour,
        int minute,
        int seconds,
        int frame)
    {
        return
            (hour / 10 & 0x0f) << 28 | (hour % 10 & 0x0f) << 24 |
            (minute / 10 & 0x0f) << 20 | (minute % 10 & 0x0f) << 16 |
            (seconds / 10 & 0x0f) << 12 | (seconds % 10 & 0x0f) << 8 |
            (frame / 10 & 0x0f) << 4 | (frame % 10 & 0x0f) << 0;
    }

    std::string timecodeToString(uint32_t in)
    {
        int hour = 0;
        int minute = 0;
        int second = 0;
        int frame = 0;
        timecodeToTime(in, hour, minute, second, frame);

        /*std::stringstream ss;
        ss << std::setfill('0') << std::setw(2) << hour;
        ss << std::setw(0) << ":";
        ss << std::setfill('0') << std::setw(2) << minute;
        ss << std::setw(0) << ":";
        ss << std::setfill('0') << std::setw(2) << second;
        ss << std::setw(0) << ":";
        ss << std::setfill('0') << std::setw(2) << frame;
        return ss.str();*/

        std::string out = "00:00:00:00";
        out[0] = 48 + hour / 10;
        out[1] = 48 + hour % 10;
        out[3] = 48 + minute / 10;
        out[4] = 48 + minute % 10;
        out[6] = 48 + second / 10;
        out[7] = 48 + second % 10;
        out[9] = 48 + frame / 10;
        out[10] = 48 + frame % 10;
        return out;
    }

    void stringToTimecode(const std::string& in, uint32_t& out)
    {
        int hour = 0;
        int minute = 0;
        int second = 0;
        int frame = 0;
        const auto pieces = ftk::split(in, ':');
        size_t i = 0;
        if (pieces.size() != 4)
        {
            throw ftk::ParseError();
        }
        hour = std::stoi(pieces[i]); ++i;
        minute = std::stoi(pieces[i]); ++i;
        second = std::stoi(pieces[i]); ++i;
        frame = std::stoi(pieces[i]);
        out = timeToTimecode(hour, minute, second, frame);
    }
}

namespace opentime
{
    namespace OPENTIME_VERSION_NS
    {
        std::string to_string(const RationalTime& value)
        {
            std::stringstream ss;
            ss << std::fixed << value.value() << "/" << value.rate();
            return ss.str();
        }

        std::string to_string(const TimeRange& value)
        {
            std::stringstream ss;
            ss << std::fixed << value.start_time().value() << "/" <<
                value.duration().value() << "/" <<
                value.duration().rate();
            return ss.str();
        }

        bool from_string(const std::string& s, RationalTime& value)
        {
            bool out = false;
            auto split = ftk::split(s, '/');
            if (2 == split.size())
            {
                value = RationalTime(std::stof(split[0]), std::stof(split[1]));
                out = true;
            }
            return out;
        }

        bool from_string(const std::string& s, TimeRange& value)
        {
            bool out = false;
            auto split = ftk::split(s, '/');
            if (3 == split.size())
            {
                double startTime = 0.0;
                {
                    std::stringstream ss(split[0]);
                    ss >> startTime;
                }
                double duration = 0.0;
                {
                    std::stringstream ss(split[1]);
                    ss >> duration;
                }
                double rate = 0.0;
                {
                    std::stringstream ss(split[2]);
                    ss >> rate;
                }
                value = TimeRange(
                    RationalTime(startTime, rate),
                    RationalTime(duration, rate));
                out = true;
            }
            return out;
        }

        void to_json(nlohmann::json& json, const RationalTime& value)
        {
            json = { value.value(), value.rate() };
        }

        void to_json(nlohmann::json& json, const TimeRange& value)
        {
            json = { value.start_time(), value.duration() };
        }

        void from_json(const nlohmann::json& json, RationalTime& value)
        {
            double v = 0.0;
            double rate = 0.0;
            json.at(0).get_to(v);
            json.at(1).get_to(rate);
            value = RationalTime(v, rate);
        }

        void from_json(const nlohmann::json& json, TimeRange& value)
        {
            RationalTime start;
            RationalTime duration;
            json.at(0).get_to(start);
            json.at(1).get_to(duration);
            value = TimeRange(start, duration);
        }

        std::ostream& operator << (std::ostream& os, const RationalTime& value)
        {
            os << to_string(value);
            return os;
        }

        std::ostream& operator << (std::ostream& os, const TimeRange& value)
        {
            os << to_string(value);
            return os;
        }

        bool cmdLineParse(std::vector<std::string>& args, std::vector<std::string>::iterator& it, RationalTime& value)
        {
            bool out = false;
            if (it != args.end())
            {
                out = from_string(*it, value);
                if (out)
                {
                    it = args.erase(it);
                }
            }
            return out;
        }

        bool cmdLineParse(std::vector<std::string>& args, std::vector<std::string>::iterator& it, TimeRange& value)
        {
            bool out = false;
            if (it != args.end())
            {
                out = from_string(*it, value);
                if (out)
                {
                    it = args.erase(it);
                }
            }
            return out;
        }
    }
}
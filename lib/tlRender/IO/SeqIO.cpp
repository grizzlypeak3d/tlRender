// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/SeqIO.h>

#include <ftk/Core/Format.h>

#include <thread>

namespace tl
{
    FTK_ENUM_IMPL(
        MissingFrames,
        "Error",
        "Hold",
        "Black",
        "Skip",
        "Gaps");

    bool isStructural(MissingFrames value)
    {
        return
            MissingFrames::Skip == value ||
            MissingFrames::Gaps == value;
    }

    SeqOptions::SeqOptions()
    {}

    bool SeqOptions::operator == (const SeqOptions& other) const
    {
        return
            defaultSpeed == other.defaultSpeed &&
            missingFrames == other.missingFrames;
    }

    bool SeqOptions::operator != (const SeqOptions& other) const
    {
        return !(*this == other);
    }

    IOOptions getOptions(const SeqOptions& value)
    {
        IOOptions out;
        out["SeqIO/DefaultSpeed"] = ftk::Format("{0}").arg(value.defaultSpeed);
        out["SeqIO/MissingFrames"] = to_string(value.missingFrames);
        return out;
    }

    MissingFrames getMissingFrames(const IOOptions& value)
    {
        MissingFrames out = SeqOptions().missingFrames;
        const auto i = value.find("SeqIO/MissingFrames");
        if (i != value.end())
        {
            from_string(i->second, out);
        }
        return out;
    }

    void to_json(nlohmann::json& json, const SeqOptions& value)
    {
        json["DefaultSpeed"] = value.defaultSpeed;
        json["MissingFrames"] = to_string(value.missingFrames);
    }

    void from_json(const nlohmann::json& json, SeqOptions& value)
    {
        json.at("DefaultSpeed").get_to(value.defaultSpeed);
        // Added later than the rest, so settings written by an earlier version
        // do not carry it. Insisting on it would throw away every other
        // sequence setting along with it.
        const auto i = json.find("MissingFrames");
        if (i != json.end())
        {
            from_string(i->get<std::string>(), value.missingFrames);
        }
    }
}
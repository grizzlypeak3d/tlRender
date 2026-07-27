// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/SeqIO.h>

#include <ftk/Core/Format.h>

#include <thread>

namespace tl
{
    SeqOptions::SeqOptions()
    {}

    bool SeqOptions::operator == (const SeqOptions& other) const
    {
        return defaultSpeed == other.defaultSpeed;
    }

    bool SeqOptions::operator != (const SeqOptions& other) const
    {
        return !(*this == other);
    }

    IOOptions getOptions(const SeqOptions& value)
    {
        IOOptions out;
        out["SeqIO/DefaultSpeed"] = ftk::Format("{0}").arg(value.defaultSpeed);
        return out;
    }

    void to_json(nlohmann::json& json, const SeqOptions& value)
    {
        json["DefaultSpeed"] = value.defaultSpeed;
    }

    void from_json(const nlohmann::json& json, SeqOptions& value)
    {
        json.at("DefaultSpeed").get_to(value.defaultSpeed);
    }
}
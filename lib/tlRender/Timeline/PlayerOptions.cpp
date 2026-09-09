// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/PlayerOptions.h>

namespace tl
{
    void to_json(nlohmann::json& json, const PlayerCacheOptions& value)
    {
        json["VideoGB"] = value.videoGB;
        json["AudioGB"] = value.audioGB;
        json["ReadBehind"] = value.readBehind;
    }

    void from_json(const nlohmann::json& json, PlayerCacheOptions& value)
    {
        json.at("VideoGB").get_to(value.videoGB);
        json.at("AudioGB").get_to(value.audioGB);
        json.at("ReadBehind").get_to(value.readBehind);
    }
}

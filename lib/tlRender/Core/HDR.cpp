// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Core/HDR.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/String.h>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace tl
{
    FTK_ENUM_IMPL(
        HDR_EOTF,
        "SDR",
        "HDR",
        "ST2084");

    FTK_ENUM_IMPL(
        HDRPrimaries,
        "Red",
        "Green",
        "Blue",
        "White");

    namespace
    {
        const float pqM1 = .1593017578125F;
        const float pqM2 = 78.84375F;
        const float pqC1 = .8359375F;
        const float pqC2 = 18.8515625F;
        const float pqC3 = 18.6875F;
    }

    float fromPQ(float value)
    {
        const float p = std::pow(std::clamp(value, 0.F, 1.F), 1.F / pqM2);
        return 10000.F * std::pow(std::max(p - pqC1, 0.F) / (pqC2 - pqC3 * p), 1.F / pqM1);
    }

    float toPQ(float value)
    {
        const float y = std::pow(std::clamp(value / 10000.F, 0.F, 1.F), pqM1);
        return std::pow((pqC1 + pqC2 * y) / (1.F + pqC3 * y), pqM2);
    }

    void to_json(nlohmann::json& json, const HDRData& value)
    {
        json["EOTF"] = to_string(value.eotf);
        for (size_t i = 0; i < value.primaries.size(); ++i)
        {
            json["Primaries"].push_back(value.primaries[i]);
        }
        json["DisplayMasteringLuminance"] = value.displayMasteringLuminance;
        json["MaxCLL"] = value.maxCLL;
        json["MaxFALL"] = value.maxFALL;
    }

    void from_json(const nlohmann::json& json, HDRData& value)
    {
        from_string(json.at("EOTF").get<std::string>(), value.eotf);
        for (size_t i = 0; i < value.primaries.size(); ++i)
        {
            json.at("Primaries").at(i).get_to(value.primaries[i]);
        }
        json.at("DisplayMasteringLuminance").get_to(value.displayMasteringLuminance);
        json.at("MaxCLL").get_to(value.maxCLL);
        json.at("MaxFALL").get_to(value.maxFALL);
    }
}

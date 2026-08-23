// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>
#include <tlRender/Core/Util.h>

#include <ftk/Core/Range.h>
#include <ftk/Core/Vector.h>
#include <ftk/Core/Util.h>

#include <nlohmann/json.hpp>

#include <array>

namespace tl
{
    //! HDR EOTF.
    enum class TL_CORE_API_TYPE HDR_EOTF
    {
        SDR,
        HDR,
        ST2084,

        Count,
        First = SDR
    };
    TL_ENUM(HDR_EOTF);

    //! HDR color primaries.
    enum class TL_CORE_API_TYPE HDRPrimaries
    {
        Red,
        Green,
        Blue,
        White,

        Count,
        First = Red
    };
    TL_ENUM(HDRPrimaries);

    //! HDR data.
    struct TL_CORE_API_TYPE HDRData
    {
        HDR_EOTF eotf = HDR_EOTF::SDR;

        //! Default Rec. 2020 color primaries (red, green, blue, white).
        std::array<ftk::V2F, static_cast<size_t>(HDRPrimaries::Count)> primaries =
        {
            ftk::V2F(.708F,  .292F),
            ftk::V2F(.170F,  .797F),
            ftk::V2F(.131F,  .046F),
            ftk::V2F(.3127F, .3290F)
        };
        ftk::RangeF displayMasteringLuminance = ftk::RangeF(0.F, 1000.F);
        float maxCLL = 1000.F;
        float maxFALL = 400.F;

        TL_CORE_API bool operator == (const HDRData&) const;
        TL_CORE_API bool operator != (const HDRData&) const;
    };

    //! \name Serialize
    ///@{

    TL_CORE_API void to_json(nlohmann::json&, const HDRData&);

    TL_CORE_API void from_json(const nlohmann::json&, HDRData&);

    ///@}
}

#include <tlRender/Core/HDRInline.h>

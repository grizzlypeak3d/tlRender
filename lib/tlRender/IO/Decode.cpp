// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/Decode.h>

namespace tl
{
    IDecode::IDecode()
    {}

    IDecode::~IDecode()
    {}

    double IDecode::getSpeed(const IOInfo&, double defaultSpeed) const
    {
        return defaultSpeed;
    }
}

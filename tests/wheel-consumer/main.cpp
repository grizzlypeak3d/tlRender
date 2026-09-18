// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/Init.h>
#include <tlRender/IO/System.h>

#include <ftk/Core/Context.h>

#include <iostream>

// Initializes tlRender and lists the I/O plugins, which loads the libraries
// the wheel carries -- OpenTimelineIO, OpenImageIO, FFmpeg -- as well as
// linking them.
int main()
{
    auto context = ftk::Context::create();
    tl::init(context);
    auto readSystem = context->getSystem<tl::ReadSystem>();
    for (const auto& name : readSystem->getNames())
    {
        std::cout << name << std::endl;
    }
    return readSystem->getNames().empty() ? 1 : 0;
}

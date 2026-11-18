// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace ui_tests
    {
        class ThumbnailColorsTest : public ftk::test::ITest
        {
        protected:
            ThumbnailColorsTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<ThumbnailColorsTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;
        };
    }
}

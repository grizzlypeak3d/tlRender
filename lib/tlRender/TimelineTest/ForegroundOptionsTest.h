// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace timeline_tests
    {
        //! Foreground options test.
        class ForegroundOptionsTest : public ftk::test::ITest
        {
        protected:
            ForegroundOptionsTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<ForegroundOptionsTest> create(
                const std::shared_ptr<ftk::Context>&);

            void run() override;
        };
    }
}

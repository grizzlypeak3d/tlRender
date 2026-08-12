// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace timeline_tests
    {
        //! Background options test.
        class BackgroundOptionsTest : public ftk::test::ITest
        {
        protected:
            BackgroundOptionsTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<BackgroundOptionsTest> create(
                const std::shared_ptr<ftk::Context>&);

            void run() override;
        };
    }
}

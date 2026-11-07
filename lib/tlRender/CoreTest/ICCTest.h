// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace core_tests
    {
        class ICCTest : public ftk::test::ITest
        {
        protected:
            ICCTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<ICCTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;
        };
    }
}

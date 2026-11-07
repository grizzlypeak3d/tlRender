// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace core_tests
    {
        class GamutTest : public ftk::test::ITest
        {
        protected:
            GamutTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<GamutTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;
        };
    }
}

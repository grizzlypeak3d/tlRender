// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace timeline_tests
    {
        class CPUColorTest : public ftk::test::ITest
        {
        protected:
            CPUColorTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<CPUColorTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _table();
        };
    }
}

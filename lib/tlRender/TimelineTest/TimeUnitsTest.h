// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace timeline_tests
    {
        //! Time units test.
        class TimeUnitsTest : public ftk::test::ITest
        {
        protected:
            TimeUnitsTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<TimeUnitsTest> create(
                const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _convert();
            void _model();
        };
    }
}

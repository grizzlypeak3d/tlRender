// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace io_tests
    {
        class EXRTest : public ftk::test::ITest
        {
        protected:
            EXRTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<EXRTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _enums();
            void _util();
            void _io();
            void _partial();
        };
    }
}

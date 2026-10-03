// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace core_tests
    {
        class QuantizeTest : public ftk::test::ITest
        {
        protected:
            QuantizeTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<QuantizeTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _fewColors();
            void _manyColors();
            void _stride();
            void _empty();
            void _frames();
        };
    }
}

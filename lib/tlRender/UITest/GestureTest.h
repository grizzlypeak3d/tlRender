// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace ftk
{
    class App;
}

namespace tl
{
    namespace ui_tests
    {
        class GestureTest : public ftk::test::ITest
        {
        protected:
            GestureTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<GestureTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _viewport(const std::shared_ptr<ftk::App>&);
            void _timeline(const std::shared_ptr<ftk::App>&);
        };
    }
}

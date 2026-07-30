// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace ui_tests
    {
        class ThumbnailSystemTest : public ftk::test::ITest
        {
        protected:
            ThumbnailSystemTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<ThumbnailSystemTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _gapSeq();
        };
    }
}

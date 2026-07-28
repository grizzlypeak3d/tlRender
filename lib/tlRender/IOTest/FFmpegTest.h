// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace io_tests
    {
        class FFmpegTest : public ftk::test::ITest
        {
        protected:
            FFmpegTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<FFmpegTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _convert();
            void _io();
            void _audio();
            void _split();
        };
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace io_tests
    {
        class IOTest : public ftk::test::ITest
        {
        protected:
            IOTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<IOTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _videoData();
            void _ioSystem();
            void _decode();
            void _seqDecode();
            void _missingFrames();
            void _seqRange();
            void _structural();
        };
    }
}

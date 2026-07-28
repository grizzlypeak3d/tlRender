// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace io_tests
    {
        class RequestQueueTest : public ftk::test::ITest
        {
        protected:
            RequestQueueTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<RequestQueueTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _roundTrip();
            void _shutdown();
            void _stopQueues();
            void _stopQueue();
            void _cancel();
            void _promiseGuard();
        };
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace io_tests
    {
        class PNGTest : public ftk::test::ITest
        {
        protected:
            PNGTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<PNGTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            //! The plugins are there whatever was built.
            void _plugins();

            //! Every type PNG stores, written and read back.
            void _roundTrip();

            //! What the writer offers for a type PNG cannot store.
            void _writeInfo();
        };
    }
}

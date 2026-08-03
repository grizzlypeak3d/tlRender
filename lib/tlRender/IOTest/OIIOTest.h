// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Read.h>
#include <tlRender/IO/Write.h>

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace io_tests
    {
        class OIIOTest : public ftk::test::ITest
        {
        protected:
            OIIOTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<OIIOTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            // Members rather than free helpers so they can report a
            // failed check, which goes through the test.
            void write(
                const std::shared_ptr<IWritePlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                const ftk::ImageInfo& imageInfo,
                const IOOptions& options);
            void read(
                const std::shared_ptr<IReadPlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                bool memoryIO,
                const IOOptions& options);
            void readError(
                const std::shared_ptr<IReadPlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                bool memoryIO,
                const IOOptions& options);
        };
    }
}

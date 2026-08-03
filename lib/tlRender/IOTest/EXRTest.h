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
        class EXRTest : public ftk::test::ITest
        {
        protected:
            EXRTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<EXRTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            // Members rather than free helpers so they can report a
            // failed check, which goes through the test.
            void write(
                const std::shared_ptr<IWritePlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                const ftk::ImageInfo& imageInfo,
                const ftk::ImageTags& tags,
                const IOOptions& options);
            void read(
                const std::shared_ptr<IReadPlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                bool memoryIO,
                const ftk::ImageTags& tags,
                const IOOptions& options);
            void readError(
                const std::shared_ptr<IReadPlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                bool memoryIO,
                const IOOptions& options);

            void _enums();
            void _util();
            void _io();
            void _partial();
        };
    }
}

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
        class FFmpegTest : public ftk::test::ITest
        {
        protected:
            FFmpegTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<FFmpegTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _commandLine();
            // Members rather than free helpers so they can report a
            // failed check, which goes through the test.
            void write(
                const std::shared_ptr<IWritePlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                const ftk::ImageInfo& imageInfo,
                const ftk::ImageTags& tags,
                const OTIO_NS::RationalTime& duration,
                const IOOptions& options);
            void read(
                const std::shared_ptr<IReadPlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                bool memoryIO,
                const ftk::ImageTags& tags,
                const OTIO_NS::RationalTime& duration,
                const IOOptions& options);
            void readError(
                const std::shared_ptr<IReadPlugin>& plugin,
                const std::shared_ptr<ftk::Image>& image,
                const ftk::Path& path,
                bool memoryIO,
                const IOOptions& options);

            void _convert();
            void _io();
            void _audio();
            void _split();
        };
    }
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/FFmpegCmd.h>

namespace tl
{
    namespace ffmpeg_cmd
    {
        class Pipe
        {
        public:
            Pipe(const std::vector<std::string>& cmd);

            ~Pipe();

            size_t read(uint8_t*, size_t);

            std::string readAll();

            //! Write to the process's standard input.
            bool write(const uint8_t*, size_t);

            //! Close the process's standard input and wait for it to
            //! finish; the exit code is returned. For a writer this is
            //! what finalizes the output file, so it must happen before
            //! destruction -- the destructor kills a process still
            //! running.
            int finish();

            //! Read whatever the process wrote to standard error, for
            //! reporting a failure.
            std::string readAllErrors();

            private:
            FTK_PRIVATE();
        };

        //! Get the information for a file with ffprobe. Both halves of the
        //! information come from one dump, so each reader keeps the half it
        //! serves and the tags, which are not separable.
        IOInfo getIOInfo(
            const ftk::Path&,
            const IOOptions&,
            const std::shared_ptr<ftk::LogSystem>&);

        typedef std::pair<int, int> Rational;

        Rational toRational(const std::string&);
        double toDouble(const Rational&);

        ftk::ImageType toImageType(const std::string&);
        std::string fromImageType(ftk::ImageType);

        AudioType toAudioType(const std::string&);
        std::string fromAudioType(AudioType);
    }
}

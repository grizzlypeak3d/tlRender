// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/FFmpegCmd.h>

#include <stdexcept>

namespace tl
{
    namespace ffmpeg_cmd
    {
        //! A command that could not be started: there is nothing by that
        //! name to run. Not having FFmpeg is not an error in itself, so
        //! this is told apart from a command that ran and failed.
        class CommandError : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

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

            //! Get the end of what the process wrote to standard error, for
            //! reporting a failure. Standard error is read as the process
            //! runs, so this waits for nothing while the process is alive.
            std::string readAllErrors();

            private:
            FTK_PRIVATE();
        };

        //! Get the information for a file with ffprobe. Both halves of the
        //! information come from one dump, so each reader keeps the half it
        //! serves and the tags, which are not separable.
        //!
        //! The audio streams are the ffprobe indices of the streams the
        //! audio is read from: one, or a run of mono streams merged as
        //! the channels of one track (see ffmpeg::Options::audioMerge).
        //!
        //! What went wrong, when something did, is given back for the reader
        //! to report to whoever opened the file. No ffprobe to run is given
        //! back and not logged: a file browser asks about every movie in a
        //! directory, and each one that needs FFmpeg would be an error on
        //! the screen for a file nobody opened.
        IOInfo getIOInfo(
            const ftk::Path&,
            const IOOptions&,
            const std::shared_ptr<ftk::LogSystem>&,
            std::vector<int>* audioStreams = nullptr,
            std::string* error = nullptr);

        typedef std::pair<int, int> Rational;

        Rational toRational(const std::string&);
        double toDouble(const Rational&);

        ftk::ImageType toImageType(const std::string&);
        std::string fromImageType(ftk::ImageType);

        AudioType toAudioType(const std::string&);
        std::string fromAudioType(AudioType);
    }
}

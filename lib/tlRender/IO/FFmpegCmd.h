// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Read.h>
#include <tlRender/IO/Write.h>

namespace tl
{
    //! FFmpeg video and audio with the command line application
    namespace ffmpeg_cmd
    {
        //! FFmpeg command line options.
        //!
        //! References:
        //! * https://academysoftwarefoundation.github.io/EncodingGuidelines/EncodeVP9.html
        struct TL_API_TYPE Options
        {
            Options() = default;
            Options(const IOOptions&);

            std::string ffmpegPath  = "ffmpeg";
            std::string ffprobePath = "ffprobe";

            TL_API IOOptions getIOOptions() const;

            TL_API bool operator == (const Options&) const;
            TL_API bool operator != (const Options&) const;
        };

        //! FFmpeg command line video reader.
        //!
        //! The video and audio readers each run their own ffmpeg process;
        //! they share nothing but the file name.
        class TL_API_TYPE VideoRead : public IVideoRead
        {
        protected:
            void _init(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            VideoRead();

        public:
            TL_API virtual ~VideoRead();

            //! Create a new reader.
            TL_API static std::shared_ptr<VideoRead> create(
                const ftk::Path&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            //! Create a new reader.
            TL_API static std::shared_ptr<VideoRead> create(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            TL_API std::future<IOInfo> getInfo() override;
            TL_API std::future<VideoData> readVideo(
                const OTIO_NS::RationalTime&,
                const IOOptions& = IOOptions()) override;
            TL_API void cancelRequests() override;

        private:
            void _run();

            FTK_PRIVATE();
        };

        //! FFmpeg command line audio reader.
        class TL_API_TYPE AudioRead : public IAudioRead
        {
        protected:
            void _init(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            AudioRead();

        public:
            TL_API virtual ~AudioRead();

            //! Create a new reader.
            TL_API static std::shared_ptr<AudioRead> create(
                const ftk::Path&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            //! Create a new reader.
            TL_API static std::shared_ptr<AudioRead> create(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            TL_API std::future<IOInfo> getInfo() override;
            TL_API std::future<AudioData> readAudio(
                const OTIO_NS::TimeRange&,
                const IOOptions& = IOOptions()) override;
            TL_API void cancelRequests() override;

        private:
            void _run();

            FTK_PRIVATE();
        };

        //! Get the version of the command line application, or nothing
        //! when there is none to ask.
        TL_API std::string getVersion(
            const IOOptions&,
            const std::shared_ptr<ftk::LogSystem>&);

        //! \name Serialize
        ///@{

        TL_API void to_json(nlohmann::json&, const Options&);

        TL_API void from_json(const nlohmann::json&, Options&);

        ///@}
    }
}

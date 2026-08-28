// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/Read.h>

namespace tl
{
    //! Movie reading in the browser.
    //!
    //! The browser supplies the decoders through WebCodecs and mp4box.js
    //! supplies the demuxer, so the page's shell has to include
    //! mp4box.js. The path is fetched as a URL relative to the page.
    //! Everything asynchronous runs on the main thread; the reader's
    //! thread requests a frame and waits, and the completion crosses
    //! back over shared memory.
    namespace webcodecs
    {
        //! WebCodecs video reader.
        class TL_IO_API_TYPE VideoRead : public IVideoRead
        {
        protected:
            void _init(
                const ftk::Path&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            VideoRead();

        public:
            TL_IO_API virtual ~VideoRead();

            //! Create a new reader.
            TL_IO_API static std::shared_ptr<VideoRead> create(
                const ftk::Path&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            TL_IO_API std::future<IOInfo> getInfo() override;
            TL_IO_API std::future<VideoData> readVideo(
                const OTIO_NS::RationalTime&,
                const IOOptions& = IOOptions()) override;
            TL_IO_API void cancelRequests() override;
            TL_IO_API std::string getError() const override;
            TL_IO_API size_t getErrorCount() const override;

        private:
            void _run();

            FTK_PRIVATE();
        };

        //! WebCodecs audio reader. The track decodes whole in the
        //! worker -- every AAC frame is a key frame -- so any range of
        //! samples is a copy.
        class TL_IO_API_TYPE AudioRead : public IAudioRead
        {
        protected:
            void _init(
                const ftk::Path&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            AudioRead();

        public:
            TL_IO_API virtual ~AudioRead();

            //! Create a new reader.
            TL_IO_API static std::shared_ptr<AudioRead> create(
                const ftk::Path&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            TL_IO_API std::future<IOInfo> getInfo() override;
            TL_IO_API std::future<AudioData> readAudio(
                const OTIO_NS::TimeRange&,
                const IOOptions& = IOOptions()) override;
            TL_IO_API void cancelRequests() override;
            TL_IO_API std::string getError() const override;
            TL_IO_API size_t getErrorCount() const override;

        private:
            void _run();

            FTK_PRIVATE();
        };

        //! WebCodecs read plugin.
        class TL_IO_API_TYPE ReadPlugin : public IReadPlugin
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);

            ReadPlugin();

        public:
            TL_IO_API virtual ~ReadPlugin();

            //! Create a new plugin.
            TL_IO_API static std::shared_ptr<ReadPlugin> create(
                const std::shared_ptr<ftk::LogSystem>&);

            TL_IO_API std::shared_ptr<IVideoRead> videoRead(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions& = IOOptions()) override;
            TL_IO_API std::shared_ptr<IAudioRead> audioRead(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions& = IOOptions()) override;
        };
    }
}

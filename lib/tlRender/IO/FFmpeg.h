// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/Read.h>
#include <tlRender/IO/Write.h>

struct AVFrame;

namespace tl
{
    //! FFmpeg video and audio I/O
    namespace ffmpeg
    {
        //! FFmpeg options.
        struct TL_IO_API_TYPE Options
        {
            bool   yuvToRgb    = false;
            bool   hwAccel     = false;
            size_t threadCount = 0;

            TL_IO_API bool operator == (const Options&) const;
            TL_IO_API bool operator != (const Options&) const;
        };

        //! Get FFmpeg options.
        TL_IO_API IOOptions getOptions(const Options&);

        //! FFmpeg video reader.
        //!
        //! The video and audio readers each own a demuxer; they share
        //! nothing but the file name.
        class TL_IO_API_TYPE VideoRead : public IVideoRead
        {
        protected:
            void _init(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
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

            //! Create a new reader.
            TL_IO_API static std::shared_ptr<VideoRead> create(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
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

        //! FFmpeg audio reader.
        class TL_IO_API_TYPE AudioRead : public IAudioRead
        {
        protected:
            void _init(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
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

            //! Create a new reader.
            TL_IO_API static std::shared_ptr<AudioRead> create(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
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

        //! A movie export preset: a name for what comes out, the options
        //! that produce it, and whether the command line writer does the
        //! work. Curated so choosing an output does not mean picking
        //! through every encoder FFmpeg has.
        struct TL_IO_API_TYPE WritePreset
        {
            std::string name;
            IOOptions options;
            bool command = false;
        };

        //! Get the movie export presets.
        TL_IO_API const std::vector<WritePreset>& getWritePresets();

        //! FFmpeg writer.
        class TL_IO_API_TYPE Write : public IWrite
        {
        protected:
            void _init(
                const ftk::Path&,
                const IOInfo&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            Write();

        public:
            TL_IO_API virtual ~Write();

            //! Create a new writer.
            TL_IO_API static std::shared_ptr<Write> create(
                const ftk::Path&,
                const IOInfo&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            TL_IO_API void writeVideo(
                const OTIO_NS::RationalTime&,
                const std::shared_ptr<ftk::Image>&,
                const IOOptions& = IOOptions()) override;

            TL_IO_API void writeAudio(
                const OTIO_NS::TimeRange&,
                const std::shared_ptr<Audio>&,
                const IOOptions& = IOOptions()) override;

            TL_IO_API void finish() override;

        private:
            void _encodeVideo(AVFrame*);
            void _encodeAudio(AVFrame*);
            void _drainAudioFifo(bool flush);

            FTK_PRIVATE();
        };

        //! FFmpeg read plugin.
        class TL_IO_API_TYPE ReadPlugin : public IReadPlugin
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);

            ReadPlugin();

        public:
            //! Create a new plugin.
            TL_IO_API static std::shared_ptr<ReadPlugin> create(
                const std::shared_ptr<ftk::LogSystem>&);

            TL_IO_API std::shared_ptr<IVideoRead> videoRead(
                const ftk::Path&,
                const IOOptions& = IOOptions()) override;
            TL_IO_API std::shared_ptr<IVideoRead> videoRead(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions & = IOOptions()) override;

            TL_IO_API std::shared_ptr<IAudioRead> audioRead(
                const ftk::Path&,
                const IOOptions& = IOOptions()) override;
            TL_IO_API std::shared_ptr<IAudioRead> audioRead(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions & = IOOptions()) override;

            TL_IO_API std::string getPluginInfo(
                const IOOptions& = IOOptions()) const override;

        private:
            static void _logCallback(void*, int, const char*, va_list);

            // av_log_set_callback() installs a process-global C callback with
            // no user-data parameter, so it can't be handed an instance
            // pointer; a file-scope weak_ptr is the available way to reach the
            // log system.
            static std::weak_ptr<ftk::LogSystem> _logSystemWeak;

            FTK_PRIVATE();
        };

        //! FFmpeg write plugin.
        class TL_IO_API_TYPE WritePlugin : public IWritePlugin
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);

            WritePlugin();

        public:
            //! Create a new plugin.
            TL_IO_API static std::shared_ptr<WritePlugin> create(
                const std::shared_ptr<ftk::LogSystem>&);

            //! Get the list of video codecs.
            TL_IO_API const std::vector<std::string>& getCodecs() const;

            //! Get the list of audio codecs.
            TL_IO_API const std::vector<std::string>& getAudioCodecs() const;

            TL_IO_API ftk::ImageInfo getInfo(
                const ftk::ImageInfo&,
                const IOOptions& = IOOptions()) const override;
            TL_IO_API std::shared_ptr<IWrite> write(
                const ftk::Path&,
                const IOInfo&,
                const IOOptions& = IOOptions()) override;

            TL_IO_API std::string getPluginInfo(
                const IOOptions& = IOOptions()) const override;

        private:
            FTK_PRIVATE();
        };

        //! \name Serialize
        ///@{

        TL_IO_API void to_json(nlohmann::json&, const Options&);

        TL_IO_API void from_json(const nlohmann::json&, Options&);

        ///@}
    }
}

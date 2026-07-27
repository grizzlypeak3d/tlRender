// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Decode.h>
#include <tlRender/IO/Plugin.h>

namespace tl
{
    //! Base class for readers.
    class TL_API_TYPE IRead : public IIO
    {
    protected:
        void _init(
            const ftk::Path&,
            const std::vector<ftk::MemFile>&,
            const IOOptions&,
            const std::shared_ptr<ftk::LogSystem>&);

        IRead();

    public:
        TL_API virtual ~IRead();

        //! Get the information.
        TL_API virtual std::future<IOInfo> getInfo() = 0;

        //! Read video data.
        TL_API virtual std::future<VideoData> readVideo(
            const OTIO_NS::RationalTime&,
            const IOOptions& = IOOptions());

        //! Read audio data.
        TL_API virtual std::future<AudioData> readAudio(
            const OTIO_NS::TimeRange&,
            const IOOptions& = IOOptions());

        //! Cancel pending requests.
        TL_API virtual void cancelRequests() = 0;

        //! Get the first error encountered while reading, or an empty
        //! string. Errors are also sent to the log. The default
        //! implementation returns an empty string.
        TL_API virtual std::string getError() const;

        //! Get the number of errors encountered while reading. The
        //! default implementation returns zero.
        TL_API virtual size_t getErrorCount() const;

    protected:
        std::vector<ftk::MemFile> _mem;
    };

    //! Base class for read plugins.
    class TL_API_TYPE IReadPlugin : public IIOPlugin
    {
        FTK_NON_COPYABLE(IReadPlugin);

    protected:
        void _init(
            const std::string& name,
            const std::map<std::string, FileType>& extensions,
            const std::shared_ptr<ftk::LogSystem>&);

        IReadPlugin();

    public:
        TL_API virtual ~IReadPlugin() = 0;

        //! Create a reader for the given path, or null when this format is
        //! decoded rather than read: a sequence of stateless files needs no
        //! reader, and decode() returns one for it instead.
        TL_API virtual std::shared_ptr<IRead> read(
            const ftk::Path&,
            const IOOptions& = IOOptions());

        //! Create a reader for the given path and memory locations, or null.
        TL_API virtual std::shared_ptr<IRead> read(
            const ftk::Path&,
            const std::vector<ftk::MemFile>&,
            const IOOptions& = IOOptions());

        //! Create a decoder, or null when this format has to be read
        //! statefully.
        TL_API virtual std::shared_ptr<IDecode> decode(
            const IOOptions& = IOOptions());

    private:
        FTK_PRIVATE();
    };
}
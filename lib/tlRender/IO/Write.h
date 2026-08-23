// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/Plugin.h>

namespace tl
{
    //! Base class for writers.
    class TL_IO_API_TYPE IWrite : public IIO
    {
    protected:
        void _init(
            const ftk::Path&,
            const IOOptions&,
            const IOInfo&,
            const std::shared_ptr<ftk::LogSystem>&);

        IWrite();

    public:
        TL_IO_API virtual ~IWrite();

        //! Write video data.
        TL_IO_API virtual void writeVideo(
            const OTIO_NS::RationalTime&,
            const std::shared_ptr<ftk::Image>&,
            const IOOptions& = IOOptions()) = 0;

        //! Write audio data. The default implementation does nothing;
        //! writers that support audio should override this. Audio is
        //! expected to be written sequentially, starting at the beginning
        //! of the audio time range given in the IOInfo.
        TL_IO_API virtual void writeAudio(
            const OTIO_NS::TimeRange&,
            const std::shared_ptr<Audio>&,
            const IOOptions& = IOOptions());

        //! Finish writing, flushing any buffered data and finalizing the
        //! file. This is called automatically by the destructor, but since
        //! destructors cannot throw, errors that occur while finalizing
        //! are only logged. Call finish() explicitly to have those errors
        //! reported as exceptions.
        TL_IO_API virtual void finish();

    protected:
        IOInfo _info;
    };

    //! Base class for write plugins.
    class TL_IO_API_TYPE IWritePlugin : public IIOPlugin
    {
        FTK_NON_COPYABLE(IWritePlugin);

    protected:
        void _init(
            const std::string& name,
            const std::map<std::string, FileType>& extensions,
            const std::shared_ptr<ftk::LogSystem>&);

        IWritePlugin();

    public:
        TL_IO_API virtual ~IWritePlugin() = 0;

        //! Get information for writing.
        TL_IO_API virtual ftk::ImageInfo getInfo(
            const ftk::ImageInfo&,
            const IOOptions& = IOOptions()) const = 0;

        //! Create a writer for the given path.
        TL_IO_API virtual std::shared_ptr<IWrite> write(
            const ftk::Path&,
            const IOInfo&,
            const IOOptions& = IOOptions()) = 0;

    protected:
        bool _isCompatible(const ftk::ImageInfo&, const IOOptions&) const;

    private:
        FTK_PRIVATE();
    };
}
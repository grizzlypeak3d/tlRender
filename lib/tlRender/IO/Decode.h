// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/IO.h>

#include <ftk/Core/FileIO.h>

namespace tl
{
    //! Base class for image decoders.
    //!
    //! A decoder turns one file, or one range of bytes within one, into one
    //! image. It has no thread, no request queue, and no position in a file,
    //! so one decoder serves any number of callers at once and can be driven
    //! synchronously.
    //!
    //! Which frame a file holds, and how files make up a sequence, is the
    //! caller's business. A format whose reader has to carry state, such as a
    //! movie with a demuxer position, is not decoded this way.
    class TL_IO_API_TYPE IDecode : public std::enable_shared_from_this<IDecode>
    {
        FTK_NON_COPYABLE(IDecode);

    protected:
        IDecode();

    public:
        TL_IO_API virtual ~IDecode() = 0;

        //! Get information about one file.
        //!
        //! The time range is left empty: one file does not know the sequence
        //! it belongs to.
        TL_IO_API virtual IOInfo getInfo(
            const std::string& fileName,
            const ftk::MemFile* = nullptr) = 0;

        //! Decode one file to an image.
        TL_IO_API virtual VideoData readVideo(
            const std::string& fileName,
            const ftk::MemFile*,
            const OTIO_NS::RationalTime&,
            const IOOptions& = IOOptions()) = 0;

        //! Get the speed the file declares, or the given default when the
        //! format has no way to declare one.
        TL_IO_API virtual double getSpeed(const IOInfo&, double defaultSpeed) const;
    };
}

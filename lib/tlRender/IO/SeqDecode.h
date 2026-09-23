// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/Decode.h>
#include <tlRender/IO/SeqIO.h>

#include <ftk/Core/Path.h>

#include <mutex>

namespace tl
{
    //! An image sequence, decoded one frame at a time.
    //!
    //! This is all that reading an image sequence needs: which file, or which
    //! range of bytes in a bundle, holds a frame, and a decoder to turn it
    //! into an image.
    //!
    //! It owns no thread and no request queue, and it is not written to after
    //! it is created, so the caller decides where reads happen: on a worker,
    //! on several at once, or in line.
    class TL_IO_API_TYPE SeqDecode : public std::enable_shared_from_this<SeqDecode>
    {
        FTK_NON_COPYABLE(SeqDecode);

    protected:
        void _init(
            const ftk::Path&,
            const std::vector<ftk::MemFile>&,
            const std::shared_ptr<IDecode>&,
            const IOOptions&);

        SeqDecode();

    public:
        TL_IO_API ~SeqDecode();

        //! Create a new sequence.
        //!
        //! This reads the first frame's header to find the image information,
        //! so it touches the file system once.
        TL_IO_API static std::shared_ptr<SeqDecode> create(
            const ftk::Path&,
            const std::vector<ftk::MemFile>&,
            const std::shared_ptr<IDecode>&,
            const IOOptions& = IOOptions());

        //! Get the path.
        TL_IO_API const ftk::Path& getPath() const;

        //! Get the information for the sequence, including the time range
        //! that the individual files do not know about.
        TL_IO_API const IOInfo& getInfo() const;

        //! Decode one frame.
        //!
        //! Safe to call from several threads at once.
        TL_IO_API VideoData readVideo(
            const OTIO_NS::RationalTime&,
            const IOOptions& = IOOptions()) const;

    private:
        //! Read the image information from the first frame that is there.
        IOInfo _probeInfo() const;

        //! The bytes for a frame, or null when the bundle does not hold it.
        const ftk::MemFile* _memFile(int64_t frame) const;

        //! The nearest frame at or before the given one that can be read, or
        //! the frame itself when there is none.
        int64_t _holdFrame(int64_t frame) const;

        VideoData _missingVideo(
            const OTIO_NS::RationalTime&,
            MissingFrames) const;

        //! Decode one file, reusing the last image when the same one is
        //! asked for again.
        VideoData _readCached(
            const std::string& name,
            const ftk::MemFile*,
            const OTIO_NS::RationalTime&,
            const IOOptions&) const;

        ftk::Path _path;
        std::vector<ftk::MemFile> _mem;
        std::shared_ptr<IDecode> _decode;
        IOOptions _options;

        // A still is one file held over many frames, and each of them would
        // otherwise decode it again. Only a single file is kept: a sequence
        // asks for a different frame almost every time, so holding one of
        // its images would cost memory to answer nothing.
        mutable std::mutex _cacheMutex;
        mutable std::string _cacheName;
        mutable IOOptions _cacheOptions;
        mutable uint16_t _cacheLayer = 0;
        mutable std::shared_ptr<ftk::Image> _cacheImage;

        int64_t _startFrame = 0;
        int64_t _endFrame = 0;
        IOInfo _info;
    };
}

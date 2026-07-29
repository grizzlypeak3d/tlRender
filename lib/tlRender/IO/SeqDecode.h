// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Decode.h>
#include <tlRender/IO/SeqIO.h>

#include <ftk/Core/Path.h>

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
    class TL_API_TYPE SeqDecode : public std::enable_shared_from_this<SeqDecode>
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
        TL_API ~SeqDecode();

        //! Create a new sequence.
        //!
        //! This reads the first frame's header to find the image information,
        //! so it touches the file system once.
        TL_API static std::shared_ptr<SeqDecode> create(
            const ftk::Path&,
            const std::vector<ftk::MemFile>&,
            const std::shared_ptr<IDecode>&,
            const IOOptions& = IOOptions());

        //! Get the path.
        TL_API const ftk::Path& getPath() const;

        //! Get the information for the sequence, including the time range
        //! that the individual files do not know about.
        TL_API const IOInfo& getInfo() const;

        //! Decode one frame.
        //!
        //! Safe to call from several threads at once.
        TL_API VideoData readVideo(
            const OTIO_NS::RationalTime&,
            const IOOptions& = IOOptions()) const;

        //! \name Skipped Frames
        //!
        //! In Skip the sequence is only as long as the frames it has, so the
        //! time it is read at is a position in that list rather than a frame
        //! number. These convert between the two, and are the identity when
        //! frames are not being skipped.
        ///@{

        //! The frame number the media gives to the given time.
        TL_API int64_t getFrame(const OTIO_NS::RationalTime&) const;

        //! The time the given frame number is read at. A frame the sequence
        //! does not have snaps down to the nearest one at or before it, or to
        //! the first when there is none, so there is always an answer.
        TL_API OTIO_NS::RationalTime getTime(int64_t frame) const;

        ///@}

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

        ftk::Path _path;
        std::vector<ftk::MemFile> _mem;
        std::shared_ptr<IDecode> _decode;
        IOOptions _options;
        int64_t _startFrame = 0;
        int64_t _endFrame = 0;
        // The frames the sequence has, in order, when they are being skipped.
        // Empty otherwise, which is what marks Skip as being in force: the
        // policy itself is read per call, but this is settled at open.
        std::vector<int64_t> _skip;
        IOInfo _info;
    };
}

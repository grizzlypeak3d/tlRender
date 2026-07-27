// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Decode.h>

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

    private:
        ftk::Path _path;
        std::vector<ftk::MemFile> _mem;
        std::shared_ptr<IDecode> _decode;
        IOOptions _options;
        int64_t _startFrame = 0;
        int64_t _endFrame = 0;
        IOInfo _info;
    };
}

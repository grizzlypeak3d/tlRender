// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/IO.h>

#include <ftk/Core/Path.h>

namespace tl
{
    //! Image sequence audio options.
    enum class TL_API_TYPE ImageSeqAudio
    {
        None,     //!< No audio
        Ext,      //!< Search for an audio file by extension
        FileName, //!< Use the given audio file name

        Count,
        First = None
    };
    TL_ENUM(ImageSeqAudio);

    //! Spatial coordinate options.
    enum class TL_API_TYPE Spatial
    {
        //! Ignore the OTIO spatial coordinates, laying out clips from their
        //! image sizes
        None,

        //! Use the OTIO spatial coordinates where clips provide them
        Coordinates,

        //! Use the OTIO spatial coordinates, and give clips without them the
        //! size of the first video clip, so that clips of differing
        //! resolutions are all displayed at the same size
        Normalize,

        Count,
        First = None
    };
    TL_ENUM(Spatial);

    //! Timeline options.
    struct TL_API_TYPE Options
    {
        //! Image sequence audio.
        ImageSeqAudio imageSeqAudio = ImageSeqAudio::Ext;

        //! Spatial coordinates.
        Spatial spatial = Spatial::Coordinates;

        //! Image sequence audio extensions.
        std::vector<std::string> imageSeqAudioExts = { ".mp3", ".wav" };

        //! Image sequence audio file name.
        std::string imageSeqAudioFileName;

        //! Enable workarounds for timelines that may not conform exactly
        //! to specification.
        bool compat = true;

        //! Run the timeline on its own thread.
        //!
        //! When this is false the timeline has no thread and no read pool:
        //! a request is filled by the call that makes it, and the future it
        //! returns is already resolved. That is what a caller that only
        //! wants a frame or two wants, and it is what lets a thumbnail be
        //! taken without a thread per file.
        bool threaded = true;

        //! Maximum number of video requests in flight.
        //!
        //! This caps the sequence decoding threads as well: no more frames
        //! are decoded at once than there are requests to decode them for,
        //! whatever "SeqIO/ThreadCount" is set to.
        size_t videoRequestMax = 16;

        //! Maximum number of audio requests.
        size_t audioRequestMax = 16;

        //! Request timeout.
        std::chrono::milliseconds requestTimeout = std::chrono::milliseconds(5);

        //! I/O options.
        IOOptions ioOptions;

        //! Path options.
        ftk::PathOptions pathOptions;

        TL_API bool operator == (const Options&) const;
        TL_API bool operator != (const Options&) const;
    };
}

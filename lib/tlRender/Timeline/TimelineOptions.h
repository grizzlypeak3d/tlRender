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

    //! Get the default number of sequence decoding threads.
    TL_API size_t getDefaultReadThreadCount();

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

        //! Find the frames of an image sequence on disk when the timeline is
        //! opened.
        //!
        //! Turn this off when the path already states the range to use. A
        //! path cannot say so itself: a range of one frame looks exactly like
        //! the frame parsed out of a file name, so a sequence stated as a
        //! single frame would be expanded back to whatever is on disk.
        bool seqExpand = true;

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

        //! How many sequence frames the timeline decodes at once.
        //!
        //! This is the one place the decoding concurrency is set. How many
        //! video requests are in flight follows from it, so that raising it
        //! is not silently undone by a separate limit.
        size_t readThreadCount = getDefaultReadThreadCount();


        //! Maximum number of audio requests in flight.
        //!
        //! Unlike video, this does not follow readThreadCount: audio is not
        //! decoded by the timeline's pool. It comes from a reader that does
        //! its own threading, so there is no thread count here for it to
        //! follow and this is the only limit on how much audio is being
        //! assembled at once.
        size_t audioRequestMax = 16;

        //! Maximum number of stateful readers held per half.
        //!
        //! Video and audio are cached separately, so this many of each: a
        //! movie with sound occupies one entry in both.
        //!
        //! What it bounds is decoder state and threads, not parallelism --
        //! readThreadCount is the knob for that. Its right value follows
        //! from how many distinct references a timeline plays across, which
        //! is a property of the timeline rather than of the machine.
        size_t readCacheMax = 10;

        //! Maximum number of image sequences held.
        //!
        //! Far larger than readCacheMax because a sequence holds no thread
        //! and no queue: only where each frame lives, so evicting one costs
        //! a header read. A timeline with a clip per shot has hundreds.
        //!
        //! Note that inside a bundle an entry carries a byte range per
        //! frame, so a long sequence is not free; a count is the wrong
        //! bound if that ever starts to matter.
        size_t seqCacheMax = 1000;

        //! I/O options.
        IOOptions ioOptions;

        //! Path options.
        ftk::PathOptions pathOptions;

        TL_API bool operator == (const Options&) const;
        TL_API bool operator != (const Options&) const;
    };
}

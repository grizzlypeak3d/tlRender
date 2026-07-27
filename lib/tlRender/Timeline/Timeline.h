// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Audio.h>
#include <tlRender/Timeline/TimelineOptions.h>
#include <tlRender/IO/SeqDecode.h>
#include <tlRender/Timeline/Video.h>

#include <tlRender/IO/Read.h>

#include <ftk/Core/FileIO.h>
#include <ftk/Core/Path.h>

#include <opentimelineio/timeline.h>
#include <opentimelineio/mediaReference.h>

#include <future>

namespace ftk
{
    class Context;
}

namespace tl
{
    //! Video request.
    struct TL_API_TYPE VideoRequest
    {
        uint64_t id = 0;
        std::future<VideoFrame> future;
    };

    //! Audio request.
    struct TL_API_TYPE AudioRequest
    {
        uint64_t id = 0;
        std::future<AudioFrame> future;
    };

    //! Timeline.
    class TL_API_TYPE Timeline : public std::enable_shared_from_this<Timeline>
    {
        FTK_NON_COPYABLE(Timeline);

    protected:
        void _init(
            const std::shared_ptr<ftk::Context>&,
            const ftk::Path& inputPath,
            const ftk::Path& inputAudioPath,
            const Options&);
        void _init(
            const std::shared_ptr<ftk::Context>&,
            const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>&,
            const Options&);

        Timeline();

    public:
        TL_API ~Timeline();

        //! Create a new timeline.
        TL_API static std::shared_ptr<Timeline> create(
            const std::shared_ptr<ftk::Context>&,
            const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>&,
            const Options& = Options());

        //! Create a new timeline from a path. The path can point to an
        //! .otio file, movie file, or image sequence.
        TL_API static std::shared_ptr<Timeline> create(
            const std::shared_ptr<ftk::Context>&,
            const ftk::Path&,
            const Options& = Options());

        //! Create a new timeline from a path and audio path. The path can
        //! point to an .otio file, movie file, or image sequence.
        TL_API static std::shared_ptr<Timeline> create(
            const std::shared_ptr<ftk::Context>&,
            const ftk::Path& path,
            const ftk::Path& audioPath,
            const Options& = Options());

        //! Create a new timeline from a file name. The file name can point
        //! to an .otio file, movie file, or image sequence.
        TL_API static std::shared_ptr<Timeline> create(
            const std::shared_ptr<ftk::Context>&,
            const std::string&,
            const Options& = Options());

        //! Create a new timeline from a file name and audio file name.
        //! The file name can point to an .otio file, movie file, or
        //! image sequence.
        TL_API static std::shared_ptr<Timeline> create(
            const std::shared_ptr<ftk::Context>&,
            const std::string& fileName,
            const std::string& audioFilename,
            const Options& = Options());

        //! Get the context.
        TL_API std::shared_ptr<ftk::Context> getContext() const;

        //! Get the timeline.
        TL_API const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>& getTimeline() const;

        //! Get the file path.
        TL_API const ftk::Path& getPath() const;

        //! Get the audio file path.
        TL_API const ftk::Path& getAudioPath() const;

        //! Get the timeline options.
        TL_API const Options& getOptions() const;

        //! Get the memory for the given media reference.
        TL_API std::vector<ftk::MemFile> getMem(
            const OTIO_NS::MediaReference*);

        //! Get the paths of the media in the timeline.
        //!
        //! A bundle's media are byte ranges rather than files on disk, so a
        //! caller that wants one of them read cannot open its path. These
        //! name the media to getMediaInfo() and readMedia() instead, which
        //! keeps the reading on the side that knows where the bytes are.
        TL_API std::vector<ftk::Path> getMediaPaths() const;

        //! Get the information for one of the media in the timeline.
        TL_API bool getMediaInfo(const ftk::Path&, IOInfo&);

        //! Read one frame of one of the media in the timeline.
        //!
        //! On a timeline with no thread the future comes back resolved.
        TL_API std::future<VideoData> readMedia(
            const ftk::Path&,
            const OTIO_NS::RationalTime&,
            const IOOptions& = IOOptions());

        //! \name Media References
        ///
        //! Clips may carry several media references, for example a proxy and
        //! a full resolution version of the same media, and one of them is
        //! active at a time. Which one is active is tracked here rather than
        //! written back to the OTIO timeline, so that the timeline can be read
        //! by the request thread without locking.
        ///@{

        //! Get the media reference keys used anywhere in the timeline, sorted
        //! and without duplicates.
        TL_API std::vector<std::string> getMediaReferenceKeys() const;

        //! Get the media reference key applied to the whole timeline. An empty
        //! key, the default, leaves every clip on the media reference that
        //! OTIO has active.
        TL_API std::string getMediaReferenceKey() const;

        //! Set the media reference key for the whole timeline. Clips that have
        //! no media reference with this key fall back to
        //! OTIO_NS::Clip::default_media_key, and then to the media reference
        //! OTIO has active.
        //!
        //! The change applies to media read after it; the caller is
        //! responsible for discarding anything already read, for example with
        //! Player::clearCache().
        TL_API void setMediaReferenceKey(const std::string&);

        //! Get the media reference key applied to the given clip, which may be
        //! empty. This is the key set for the clip alone, not the timeline
        //! wide key it falls back to.
        TL_API std::string getMediaReferenceKey(const OTIO_NS::Clip*) const;

        //! Set the media reference key for a single clip, overriding the
        //! timeline wide key. An empty key returns the clip to the timeline
        //! wide key.
        TL_API void setMediaReferenceKey(
            const OTIO_NS::Clip*,
            const std::string&);

        //! Get the media reference a clip is read from, honoring the keys set
        //! above.
        TL_API OTIO_NS::MediaReference* getMediaReference(
            const OTIO_NS::Clip*) const;

        ///@}

        //! \name Information
        ///@{

        //! Get the time range.
        TL_API const OTIO_NS::TimeRange& getTimeRange() const;

        //! Get the duration.
        TL_API OTIO_NS::RationalTime getDuration() const;

        //! Get the I/O information. This information is retrieved from
        //! the first clip in the timeline.
        //!
        //! The video information follows the media reference the clip is
        //! being read from, so that it describes the media on screen rather
        //! than the media that was active when the timeline was read.
        TL_API const IOInfo& getIOInfo() const;

        //! Get the first error encountered while reading, or an empty
        //! string. Errors are also sent to the log.
        TL_API std::string getReadError() const;

        //! Get the number of errors encountered while reading. The count
        //! is a lower bound; errors from readers that have been evicted
        //! from the internal cache may not be included.
        TL_API size_t getReadErrorCount() const;

        ///@}

        //! \name Video and Audio
        ///@{

        //! Get video.
        TL_API VideoRequest getVideo(
            const OTIO_NS::RationalTime&,
            const IOOptions& = IOOptions());

        //! Get audio.
        TL_API AudioRequest getAudio(
            double seconds,
            const IOOptions& = IOOptions());

        //! Cancel requests.
        TL_API void cancelRequests(const std::vector<uint64_t>&);

        ///@}

        //! Get the number of objects currenty instantiated.
        TL_API static size_t getObjectCount();

    private:
        // Get the sequence for a media reference, or null when the format is
        // not read as a sequence of stateless files: a movie carries a
        // demuxer position and keeps its own reader.
        // Find a media reference by its resolved path.
        OTIO_NS::MediaReference* _findMedia(const ftk::Path&);
        std::shared_ptr<SeqDecode> _getSeqDecode(
            const OTIO_NS::MediaReference*,
            const IOOptions&);
        // Get the information for a media reference from whichever of the two
        // reads it.
        bool _getIOInfo(
            const OTIO_NS::MediaReference*,
            const IOOptions&,
            IOInfo&);
        std::shared_ptr<IRead> _getRead(
            const OTIO_NS::Clip*,
            const IOOptions&);
        std::shared_ptr<IRead> _getRead(
            const OTIO_NS::MediaReference*,
            const IOOptions&);
        std::future<VideoData> _readVideo(
            const OTIO_NS::Clip*,
            const OTIO_NS::RationalTime&,
            const IOOptions&);
        std::future<AudioData> _readAudio(
            const OTIO_NS::Clip*,
            const OTIO_NS::TimeRange&,
            const IOOptions&);

        bool _getVideoInfo(const OTIO_NS::Composable*);
        bool _getAudioInfo(const OTIO_NS::Composable*);
        void _getCanvas();

        float _transitionValue(double frame, double in, double out) const;

        void _tick();
        void _requests();
        void _finishRequests();

        FTK_PRIVATE();
    };
}

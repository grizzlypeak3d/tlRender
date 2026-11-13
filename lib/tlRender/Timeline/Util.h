// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Timeline/Player.h>

#include <ftk/Core/Box.h>
#include <ftk/Core/FileIO.h>
#include <ftk/Core/Matrix.h>
#include <ftk/Core/Mesh.h>

#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/mediaReference.h>
#include <opentimelineio/timeline.h>

#include <optional>

namespace tl
{
    //! Get the timeline file extensions.
    TL_TIMELINE_API std::vector<std::string> getExts(
        const std::shared_ptr<ftk::Context>&,
        int types =
            static_cast<int>(FileType::Media) |
            static_cast<int>(FileType::Seq) |
            static_cast<int>(FileType::Audio));

    //! A named group of timeline file extensions, e.g. "Images".
    struct TL_TIMELINE_API_TYPE ExtGroup
    {
        std::string label;
        std::vector<std::string> exts;

        bool operator == (const ExtGroup&) const = default;
    };

    //! Get the timeline file extensions by kind: all of them, the
    //! timelines, images, movies, and audio, and the groups the plugins
    //! name, e.g. camera raw. Empty groups are left out.
    TL_TIMELINE_API std::vector<ExtGroup> getExtGroups(
        const std::shared_ptr<ftk::Context>&);

    //! Convert frames to ranges.
    TL_TIMELINE_API std::vector<OTIO_NS::TimeRange> toRanges(std::vector<OTIO_NS::RationalTime>);

    //! Loop a time.
    TL_TIMELINE_API OTIO_NS::RationalTime loop(
        const OTIO_NS::RationalTime&,
        const OTIO_NS::TimeRange&,
        bool* looped = nullptr);

    //! Loop seconds.
    TL_TIMELINE_API int64_t loop(
        int64_t,
        const ftk::Range<int64_t>&,
        bool* looped = nullptr);

    //! Loop a range.
    TL_TIMELINE_API std::vector<OTIO_NS::TimeRange> loop(
        const OTIO_NS::TimeRange&,
        const OTIO_NS::TimeRange&);

    //! Loop a range in seconds.
    TL_TIMELINE_API std::vector<ftk::Range<int64_t> > loop(
        const ftk::Range<int64_t>&,
        const ftk::Range<int64_t>&);

    //! Cache direction.
    enum class TL_TIMELINE_API_TYPE CacheDir
    {
        Forward,
        Reverse,

        Count,
        First = Forward
    };
    FTK_ENUM(TL_TIMELINE_API, CacheDir);

    //! Get the root (highest parent).
    TL_TIMELINE_API const OTIO_NS::Composable* getRoot(const OTIO_NS::Composable*);

    //! Get the parent of the given type.
    template<typename T>
    const T* getParent(const OTIO_NS::Item*);

    //! Get the duration of all tracks of the same kind.
    TL_TIMELINE_API std::optional<OTIO_NS::RationalTime> getDuration(
        const OTIO_NS::Timeline*,
        const std::string& kind);

    //! Get the time range of a timeline. Unset when no track has a duration.
    TL_TIMELINE_API std::optional<OTIO_NS::TimeRange> getTimeRange(const OTIO_NS::Timeline*);

    //! Get a list of paths to open from the given path.
    TL_TIMELINE_API std::vector<ftk::Path> getPaths(
        const std::shared_ptr<ftk::Context>&,
        const ftk::Path&,
        const ftk::DirListOptions&);

    //! Get an absolute path. The directory may include a protocol, so
    //! that media next to a remote timeline resolves remote. A file URL
    //! gives the path it names.
    TL_TIMELINE_API ftk::Path getPath(
        const std::string& url,
        const std::string& directory,
        const ftk::PathOptions&);

    //! Get a path for a media reference.
    TL_TIMELINE_API ftk::Path getPath(
        const OTIO_NS::MediaReference*,
        const std::string& directory,
        ftk::PathOptions);

    //! Convert from an OTIO missing frame policy.
    TL_TIMELINE_API MissingFrames fromOTIO(
        OTIO_NS::ImageSequenceReference::MissingFramePolicy);

    //! Convert to an OTIO missing frame policy.
    TL_TIMELINE_API OTIO_NS::ImageSequenceReference::MissingFramePolicy toOTIO(
        MissingFrames);

    //! Get how far an image sequence's file numbers are from its media time.
    //!
    //! OTIO puts the reference's start frame at the start of its available
    //! range, and the two need not be the same number: files 0001 to 0048
    //! with an available range from zero is a valid reference, whose time 5
    //! is file 0006. A reader counts in file numbers, so read as the media
    //! time alone it is file 0005, and every frame is one file early with
    //! nothing failing but the first. None for anything but an image sequence
    //! reference with an available range.
    TL_TIMELINE_API std::optional<OTIO_NS::RationalTime> getSequenceOffset(
        const OTIO_NS::MediaReference*);

    //! Get a clip's trimmed range in the time its media's reader counts in:
    //! file numbers for an image sequence (the offset above), and otherwise
    //! the media's own time, less the available range when compat is set and
    //! the available range starts after the media does -- an OTIO file
    //! written against a movie's timecode with the movie counting from zero
    //! (e.g., Picchu).
    TL_TIMELINE_API OTIO_NS::TimeRange getMediaTrimmedRange(
        const OTIO_NS::TimeRange& trimmedRange,
        const OTIO_NS::TimeRange& availableRange,
        const std::optional<OTIO_NS::RationalTime>& sequenceOffset,
        const std::optional<OTIO_NS::TimeRange>& mediaTime,
        bool compat);

    //! Transform track time to video media time: the frame that is showing
    //! then, which at a different rate is the one whose time it falls in
    //! rather than the nearest. Within a millionth of a frame counts as on
    //! it, for the arithmetic's sake.
    TL_TIMELINE_API OTIO_NS::RationalTime toVideoMediaTime(
        const OTIO_NS::RationalTime&,
        const OTIO_NS::TimeRange& trimmedRangeInParent,
        const OTIO_NS::TimeRange& trimmedRange,
        double rate);

    //! Transform track time to audio media time.
    TL_TIMELINE_API OTIO_NS::TimeRange toAudioMediaTime(
        const OTIO_NS::TimeRange&,
        const OTIO_NS::TimeRange& trimmedRangeInParent,
        const OTIO_NS::TimeRange& trimmedRange,
        double sampleRate);

    //! Copy audio data.
    TL_TIMELINE_API std::vector<std::shared_ptr<Audio> > audioCopy(
        const AudioInfo&,
        const std::vector<AudioFrame>&,
        Playback,
        int64_t frame,
        int64_t size);

    //! Get the part of a box that the transform lands inside the clip
    //! volume, grown by a pixel: what is actually on the screen when a
    //! view is zoomed far in. A rasterizer keeps so many bits for a
    //! triangle's edges, and a picture enlarged four hundred times is a
    //! triangle most of a million pixels across, whose edges shook on an
    //! Apple GPU. Empty when the whole box is inside, or the transform
    //! cannot be inverted.
    TL_TIMELINE_API std::optional<ftk::Box2F> getVisibleBox(
        const ftk::M44F& mvp,
        const ftk::Box2I& box);

    //! Get the mesh for a box, or for the part of it the view shows, with
    //! the texture coordinates of the whole box.
    TL_TIMELINE_API ftk::TriMesh2F getBoxMesh(
        const ftk::Box2I& box,
        const std::optional<ftk::Box2F>& part,
        bool mirrorV = false);
}

#include <tlRender/Timeline/UtilInline.h>

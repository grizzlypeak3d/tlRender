// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Timeline.h>

#include <functional>
#include <vector>

namespace tl
{
    //! Options for finding the cuts in a timeline.
    //!
    //! Much review material is an edit flattened to one movie or one image
    //! sequence, with no clips to step between. The cuts are found from the
    //! pictures: each frame is scored against the one before it, as
    //! FFmpeg's scene filter scores it. A cut is a frame scoring over a
    //! threshold, or one that stands far above the frames around it: a cut
    //! between two similar pictures (a close-up and its reverse in the
    //! same light) scores 0.15 to 0.25, while the frames on either side of it
    //! score a tenth of that, and flicker scores high frame after frame.
    //!
    //! Measured against the edits of two films: on a 13.5 minute
    //! documentary of archive film the threshold alone found all but two
    //! of 77 cuts; on the 17 minute ASC StEM2, 117 of 196 by the threshold
    //! alone and 187 of them with the frames around it as well, with three
    //! cuts that were not in the edit. Hard cuts only; a dissolve is not
    //! looked for.
    struct TL_TIMELINE_API_TYPE DetectCutsOptions
    {
        //! The width frames are scored at. Small is fast, and enough: a cut
        //! changes the whole picture.
        int width = 320;

        //! A cut is a frame scoring at least this.
        float threshold = .3F;

        //! A cut out of black scores near 0.25, which no single threshold
        //! separates from flicker, so after a black frame this lower one
        //! applies.
        float blackThreshold = .1F;

        //! A frame is black when its average luma is at most this, from 0
        //! to 1: video black is 16 of 255, with a little noise.
        float blackLevel = 24.F / 255.F;

        //! A frame under the threshold is still a cut when it scores at
        //! least this, and at least relativeRatio times the average of the
        //! frames within relativeWindow on either side of it (and half that
        //! times the highest of them). A relativeRatio of zero turns it off.
        float relativeMin = .1F;
        float relativeRatio = 4.F;
        int relativeWindow = 12;

        //! Cuts closer together than this many frames are one cut, the
        //! first of them: strobing and flicker score again and again, and
        //! what follows a cut starts at its first change. (The strongest is
        //! not taken: a flash a few frames after a cut would move the cut
        //! onto it.)
        int minLength = 8;

        bool operator == (const DetectCutsOptions&) const = default;
    };

    //! What scoring a frame gives: how different it is from the frame
    //! before, from 0 to 1, and its average luma, from 0 to 1.
    struct TL_TIMELINE_API_TYPE CutScore
    {
        float score = 0.F;
        float luma = 0.F;

        bool operator == (const CutScore&) const = default;
    };

    //! Find the cuts in a list of frame scores, by the rule in
    //! DetectCutsOptions. The result is the indexes of the frames that are
    //! cut to; the first frame never is, having nothing before it.
    TL_TIMELINE_API std::vector<size_t> findCuts(
        const std::vector<CutScore>&,
        const DetectCutsOptions& = DetectCutsOptions());

    //! Score the frames of a timeline over a range, in order. The progress
    //! function is called with each frame scored and returns false to stop,
    //! in which case the scores so far are returned. The frames are read as
    //! pixels; give it a timeline made for this, without the "Texture" I/O
    //! option, rather than the one a player is playing.
    TL_TIMELINE_API std::vector<CutScore> scoreFrames(
        const std::shared_ptr<Timeline>&,
        const OTIO_NS::TimeRange&,
        const DetectCutsOptions& = DetectCutsOptions(),
        const std::function<bool(size_t frame, size_t count)>& progress = nullptr);

    //! Find the cuts in a timeline over a range: the times of the frames
    //! that are cut to. Stopped through the progress function, it returns
    //! nothing.
    TL_TIMELINE_API std::vector<OTIO_NS::RationalTime> detectCuts(
        const std::shared_ptr<Timeline>&,
        const OTIO_NS::TimeRange&,
        const DetectCutsOptions& = DetectCutsOptions(),
        const std::function<bool(size_t frame, size_t count)>& progress = nullptr);
}

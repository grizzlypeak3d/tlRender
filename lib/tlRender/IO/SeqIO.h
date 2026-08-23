// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/Read.h>
#include <tlRender/IO/Write.h>

namespace tl
{
    //! What to do about a frame a sequence does not have.
    //!
    //! The first three match OTIO's image sequence reference policies, which is
    //! where the value comes from when the sequence is described by a timeline.
    //! They are decided per read: the sequence keeps its length and each of
    //! them says what fills a gap.
    //!
    //! The last two are different in kind. They do not fill anything; they say
    //! what the timeline is built out of, so a clip covers each run of frames
    //! that are there and no read ever asks for a frame that is not. That makes
    //! them structural: they are settled when the sequence is opened, and
    //! changing one means opening it again. Because the frames are found in
    //! order to build the clips, they only apply to an image sequence opened
    //! directly -- a timeline that was authored already says what its clips
    //! are, and those are not ours to rewrite.
    enum class TL_IO_API_TYPE MissingFrames
    {
        Error,  //!< The frame does not read.
        Hold,   //!< Repeat the nearest frame before it.
        Black,  //!< A blank frame.
        Skip,   //!< Leave it out; only the frames that are there play.
        Gaps,   //!< Leave a hole, so the frames keep the times they had.

        Count,
        First = Error
    };
    FTK_ENUM(TL_IO_API, MissingFrames);

    //! Get whether a policy is settled when the sequence is opened, by
    //! deciding the clips, rather than per read.
    TL_IO_API bool isStructural(MissingFrames);

    //! Sequence I/O options.
    struct TL_IO_API_TYPE SeqOptions
    {
        TL_IO_API SeqOptions();

        double        defaultSpeed  = 24.0;
        MissingFrames missingFrames = MissingFrames::Error;

        TL_IO_API bool operator == (const SeqOptions&) const;
        TL_IO_API bool operator != (const SeqOptions&) const;
    };

    //! Get sequence I/O options.
    TL_IO_API IOOptions getOptions(const SeqOptions&);

    //! Get the missing frame policy from the options.
    TL_IO_API MissingFrames getMissingFrames(const IOOptions&);

    //! Base class for image sequence writers.
    class TL_IO_API_TYPE ISeqWrite : public IWrite
    {
    protected:
        void _init(
            const ftk::Path&,
            const IOInfo&,
            const IOOptions&,
            const std::shared_ptr<ftk::LogSystem>&);

        ISeqWrite();

    public:
        TL_IO_API virtual ~ISeqWrite();

        TL_IO_API void writeVideo(
            const OTIO_NS::RationalTime&,
            const std::shared_ptr<ftk::Image>&,
            const IOOptions& = IOOptions()) override;

    protected:
        virtual void _writeVideo(
            const std::string& fileName,
            const OTIO_NS::RationalTime&,
            const std::shared_ptr<ftk::Image>&,
            const IOOptions&) = 0;

    private:
        FTK_PRIVATE();
    };

    //! \name Serialize
    ///@{

    TL_IO_API void to_json(nlohmann::json&, const SeqOptions&);

    TL_IO_API void from_json(const nlohmann::json&, SeqOptions&);

    ///@}
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Read.h>
#include <tlRender/IO/Write.h>

namespace tl
{
    //! What to show for a frame a sequence does not have.
    //!
    //! These match OTIO's image sequence reference policies, which is where
    //! the value comes from when the sequence is described by a timeline.
    enum class TL_API_TYPE MissingFrames
    {
        Error,  //!< The frame does not read.
        Hold,   //!< Repeat the nearest frame before it.
        Black,  //!< A blank frame.

        Count,
        First = Error
    };
    FTK_ENUM(MissingFrames);

    //! Sequence I/O options.
    struct TL_API_TYPE SeqOptions
    {
        SeqOptions();

        double        defaultSpeed  = 24.0;
        MissingFrames missingFrames = MissingFrames::Error;

        TL_API bool operator == (const SeqOptions&) const;
        TL_API bool operator != (const SeqOptions&) const;
    };

    //! Get sequence I/O options.
    TL_API IOOptions getOptions(const SeqOptions&);

    //! Get the missing frame policy from the options.
    TL_API MissingFrames getMissingFrames(const IOOptions&);

    //! Base class for image sequence writers.
    class TL_API_TYPE ISeqWrite : public IWrite
    {
    protected:
        void _init(
            const ftk::Path&,
            const IOInfo&,
            const IOOptions&,
            const std::shared_ptr<ftk::LogSystem>&);

        ISeqWrite();

    public:
        TL_API virtual ~ISeqWrite();

        TL_API void writeVideo(
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

    TL_API void to_json(nlohmann::json&, const SeqOptions&);

    TL_API void from_json(const nlohmann::json&, SeqOptions&);

    ///@}
}

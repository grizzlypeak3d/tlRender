// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Read.h>
#include <tlRender/IO/Write.h>

namespace tl
{
    //! Sequence I/O options.
    struct TL_API_TYPE SeqOptions
    {
        SeqOptions();

        double defaultSpeed = 24.0;

        TL_API bool operator == (const SeqOptions&) const;
        TL_API bool operator != (const SeqOptions&) const;
    };

    //! Get sequence I/O options.
    TL_API IOOptions getOptions(const SeqOptions&);

    //! Base class for image sequence readers.
    class TL_API_TYPE ISeqRead : public IRead
    {
    protected:
        void _init(
            const ftk::Path&,
            const std::vector<ftk::MemFile>&,
            const IOOptions&,
            const std::shared_ptr<ftk::LogSystem>&);

        ISeqRead();

    public:
        TL_API virtual ~ISeqRead();

        TL_API std::future<IOInfo> getInfo() override;
        TL_API std::future<VideoData> readVideo(
            const OTIO_NS::RationalTime&,
            const IOOptions& = IOOptions()) override;
        TL_API void cancelRequests() override;

    protected:
        virtual IOInfo _getInfo(
            const std::string& fileName,
            const ftk::MemFile*) = 0;
        virtual VideoData _readVideo(
            const std::string& fileName,
            const ftk::MemFile*,
            const OTIO_NS::RationalTime&,
            const IOOptions&) = 0;

        //! Stop and join the worker thread. This MUST be called from the
        //! most-derived destructor: the worker calls the pure-virtual _getInfo()
        //! and _readVideo(), so the thread must be joined while the derived
        //! object is still alive. Joining from ~ISeqRead() would be too late --
        //! the derived vtable and members are already gone, risking a
        //! pure-virtual call or use-after-free.
        void _finish();

        int64_t _startFrame = 0;
        int64_t _endFrame = 0;
        float _defaultSpeed = SeqOptions().defaultSpeed;

    private:
        void _thread();
        void _finishRequests();
        void _cancelRequests();

        FTK_PRIVATE();
    };

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

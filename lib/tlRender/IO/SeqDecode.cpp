// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/SeqDecode.h>

#include <tlRender/IO/SeqIO.h>

#include <sstream>

namespace tl
{
    void SeqDecode::_init(
        const ftk::Path& path,
        const std::vector<ftk::MemFile>& mem,
        const std::shared_ptr<IDecode>& decode,
        const IOOptions& options)
    {
        _path = path;
        _mem = mem;
        _decode = decode;
        _options = options;

        // Where the sequence starts and ends. Memory comes from a bundle,
        // which holds exactly the frames it holds; otherwise the path carries
        // the range it was found with.
        const std::string& num = path.getNum();
        if (!num.empty())
        {
            if (!_mem.empty())
            {
                std::stringstream ss(num);
                ss >> _startFrame;
                _endFrame = _startFrame + _mem.size() - 1;
            }
            else if (path.getFrames().has_value())
            {
                const ftk::RangeI64& frames = path.getFrames().value();
                _startFrame = frames.min();
                _endFrame = frames.max();
            }
        }

        double defaultSpeed = SeqOptions().defaultSpeed;
        const auto i = options.find("SeqIO/DefaultSpeed");
        if (i != options.end())
        {
            std::stringstream ss(i->second);
            ss >> defaultSpeed;
        }

        _info = _decode->getInfo(
            _path.getFileName(true),
            !_mem.empty() ? &_mem[0] : nullptr);
        const double speed = _decode->getSpeed(_info, defaultSpeed);
        _info.videoTime = OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
            OTIO_NS::RationalTime(_startFrame, speed),
            OTIO_NS::RationalTime(_endFrame, speed));
        addVideoTags(_info);
    }

    SeqDecode::SeqDecode()
    {}

    SeqDecode::~SeqDecode()
    {}

    std::shared_ptr<SeqDecode> SeqDecode::create(
        const ftk::Path& path,
        const std::vector<ftk::MemFile>& mem,
        const std::shared_ptr<IDecode>& decode,
        const IOOptions& options)
    {
        auto out = std::shared_ptr<SeqDecode>(new SeqDecode);
        out->_init(path, mem, decode, options);
        return out;
    }

    const ftk::Path& SeqDecode::getPath() const
    {
        return _path;
    }

    const IOInfo& SeqDecode::getInfo() const
    {
        return _info;
    }

    VideoData SeqDecode::readVideo(
        const OTIO_NS::RationalTime& time,
        const IOOptions& options) const
    {
        const bool seq = !_path.getNum().empty();
        const std::string fileName = seq ?
            _path.getFrame(static_cast<int64_t>(time.value()), true) :
            _path.getFileName(true);

        const int64_t memIndex = seq ?
            (static_cast<int64_t>(time.value()) - _startFrame) :
            0;
        const ftk::MemFile* mem =
            memIndex >= 0 && memIndex < static_cast<int64_t>(_mem.size()) ?
            &_mem[memIndex] :
            nullptr;
        if (mem)
        {
            // Faulting a mapped range in a page at a time costs several times
            // what asking for the whole range costs, and the faults contend
            // when several frames are read at once.
            ftk::prefetch(mem->p, mem->size);
        }

        return _decode->readVideo(fileName, mem, time, merge(options, _options));
    }
}

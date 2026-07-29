// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/SeqDecode.h>

#include <tlRender/IO/SeqIO.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/Math.h>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>

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

        // Skipping needs the frames up front, since it decides how long the
        // sequence is. That is a snapshot: frames rendered after this are
        // picked up by reopening, unlike the other policies where they simply
        // appear. Nothing else here looks on disk at open.
        if (!num.empty() && MissingFrames::Skip == getMissingFrames(options))
        {
            if (!_mem.empty())
            {
                for (size_t i = 0; i < _mem.size(); ++i)
                {
                    if (_mem[i].p)
                    {
                        _skip.push_back(_startFrame + static_cast<int64_t>(i));
                    }
                }
            }
            else
            {
                for (int64_t frame : ftk::toFrames(
                    ftk::findSeq(_path, _path.getOptions())))
                {
                    if (frame >= _startFrame && frame <= _endFrame)
                    {
                        _skip.push_back(frame);
                    }
                }
            }
        }

        _info = _probeInfo();
        const double speed = _decode->getSpeed(_info, defaultSpeed);
        if (!_skip.empty())
        {
            // As many frames as there are, starting where the first one does,
            // so that a sequence with no gaps reads the same either way.
            _info.videoTime = OTIO_NS::TimeRange(
                OTIO_NS::RationalTime(_skip.front(), speed),
                OTIO_NS::RationalTime(
                    static_cast<double>(_skip.size()), speed));
        }
        else
        {
            _info.videoTime = OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                OTIO_NS::RationalTime(_startFrame, speed),
                OTIO_NS::RationalTime(_endFrame, speed));
        }
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

    int64_t SeqDecode::getFrame(const OTIO_NS::RationalTime& time) const
    {
        int64_t out = static_cast<int64_t>(time.value());
        if (!_skip.empty())
        {
            const int64_t i = out - _skip.front();
            out = _skip[ftk::clamp(
                i,
                static_cast<int64_t>(0),
                static_cast<int64_t>(_skip.size()) - 1)];
        }
        return out;
    }

    OTIO_NS::RationalTime SeqDecode::getTime(int64_t frame) const
    {
        int64_t out = frame;
        if (!_skip.empty())
        {
            // The nearest frame at or before, so that the frame shown and the
            // frame landed on are the same one, as they are for Hold. Snapping
            // to whichever is nearest would go backwards for one frame typed
            // and forwards for the next.
            const auto i = std::upper_bound(_skip.begin(), _skip.end(), frame);
            const size_t index = i == _skip.begin() ?
                0 :
                static_cast<size_t>((i - _skip.begin()) - 1);
            out = _skip.front() + static_cast<int64_t>(index);
        }
        return OTIO_NS::RationalTime(
            static_cast<double>(out), _info.videoTime.duration().rate());
    }

    IOInfo SeqDecode::_probeInfo() const
    {
        if (_path.getNum().empty())
        {
            // Not a sequence, so there is one file to probe -- and it still
            // comes from the bundle when there is one.
            return _decode->getInfo(_path.getFileName(true), _memFile(0));
        }

        // The first frame that is actually there. Usually that is the frame
        // the path names, but it need not be: a bundle can be missing frames,
        // and a sequence can be opened over a range that begins before the
        // frames rendered so far.
        std::exception_ptr error;
        for (int64_t frame = _startFrame; frame <= _endFrame; ++frame)
        {
            const std::string fileName = _path.getFrame(frame, true);
            const ftk::MemFile* mem = nullptr;
            if (!_mem.empty())
            {
                mem = _memFile(frame);
                if (!mem)
                {
                    continue;
                }
            }
            else if (!std::filesystem::exists(
                std::filesystem::u8path(fileName)))
            {
                continue;
            }
            try
            {
                return _decode->getInfo(fileName, mem);
            }
            catch (const std::exception&)
            {
                // There but half written, most likely. Keep looking, and
                // keep the first failure in case none of them read.
                if (!error)
                {
                    error = std::current_exception();
                }
            }
        }
        if (error)
        {
            std::rethrow_exception(error);
        }

        // Nothing at all, so fail against the frame the path names rather
        // than inventing a message about one the caller never asked for.
        return _decode->getInfo(_path.getFileName(true), nullptr);
    }

    const ftk::MemFile* SeqDecode::_memFile(int64_t frame) const
    {
        const int64_t i = !_path.getNum().empty() ?
            (frame - _startFrame) :
            0;
        const ftk::MemFile* out =
            i >= 0 && i < static_cast<int64_t>(_mem.size()) ?
            &_mem[i] :
            nullptr;
        return out && out->p ? out : nullptr;
    }

    int64_t SeqDecode::_holdFrame(int64_t frame) const
    {
        for (int64_t i = frame - 1; i >= _startFrame; --i)
        {
            const bool exists = !_mem.empty() ?
                _memFile(i) != nullptr :
                // Only reached once a frame has already failed to read, so
                // this costs nothing while a sequence is complete, and while
                // it is not it costs one test per frame of the gap. Asking
                // the file system each time rather than caching what was
                // there at open is deliberate: a render in progress gains
                // frames while it is being watched.
                std::filesystem::exists(
                    std::filesystem::u8path(_path.getFrame(i, true)));
            if (exists)
            {
                return i;
            }
        }
        return frame;
    }

    VideoData SeqDecode::_missingVideo(
        const OTIO_NS::RationalTime& time,
        MissingFrames missingFrames) const
    {
        std::shared_ptr<ftk::Image> image;
        if (MissingFrames::Black == missingFrames && !_info.video.empty())
        {
            image = ftk::Image::create(_info.video[0]);
            image->zero();
        }
        return VideoData(time, 0, image);
    }

    VideoData SeqDecode::readVideo(
        const OTIO_NS::RationalTime& time,
        const IOOptions& options) const
    {
        const IOOptions merged = merge(options, _options);
        // Taken per read rather than held from when this was created, so that
        // changing the policy takes effect on what is already open. The
        // options this was created with are the base of the merge, so a
        // caller that says nothing still gets what the media asked for.
        const MissingFrames missingFrames = getMissingFrames(merged);
        const bool seq = !_path.getNum().empty();
        const int64_t frame = getFrame(time);

        if (!_mem.empty())
        {
            // Memory is indexed by frame number, and a frame the bundle does
            // not hold leaves an empty entry. Reading that frame from its path
            // would be reading a different file than the bundle describes, so
            // it is missing whatever is on disk.
            int64_t readFrame = frame;
            const ftk::MemFile* mem = _memFile(frame);
            if (!mem && seq && MissingFrames::Hold == missingFrames)
            {
                readFrame = _holdFrame(frame);
                mem = _memFile(readFrame);
            }
            if (!mem)
            {
                if (MissingFrames::Error == missingFrames)
                {
                    // The same as a frame that will not read from disk, so
                    // that the policy means one thing everywhere.
                    throw std::runtime_error(
                        ftk::Format("Frame not in the bundle: \"{0}\"").
                        arg(_path.getFrame(frame, true)).str());
                }
                return _missingVideo(time, missingFrames);
            }

            // Faulting a mapped range in a page at a time costs several times
            // what asking for the whole range costs, and the faults contend
            // when several frames are read at once.
            ftk::prefetch(mem->p, mem->size);

            return _decode->readVideo(
                seq ? _path.getFrame(readFrame, true) : _path.getFileName(true),
                mem,
                time,
                merged);
        }

        if (!seq)
        {
            return _decode->readVideo(
                _path.getFileName(true), nullptr, time, merged);
        }

        try
        {
            return _decode->readVideo(
                _path.getFrame(frame, true), nullptr, time, merged);
        }
        catch (const std::exception&)
        {
            // The frame did not read. Which frames a sequence has is only
            // discovered by trying, so that a complete sequence pays nothing
            // for this. A frame that is there but half written counts as
            // missing too, which is the case while it is being rendered.
            switch (missingFrames)
            {
            case MissingFrames::Hold:
            {
                // Walk back a frame at a time rather than holding whichever
                // one is nearest: with several frames being written at once,
                // more than one of them can fail to read.
                for (int64_t i = frame; ; )
                {
                    const int64_t prev = _holdFrame(i);
                    if (prev == i)
                    {
                        break;
                    }
                    try
                    {
                        return _decode->readVideo(
                            _path.getFrame(prev, true), nullptr, time, merged);
                    }
                    catch (const std::exception&)
                    {}
                    i = prev;
                }
                return _missingVideo(time, missingFrames);
            }
            case MissingFrames::Black:
                return _missingVideo(time, missingFrames);
            default:
                throw;
            }
        }
    }
}

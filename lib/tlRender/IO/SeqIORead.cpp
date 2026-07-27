// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/SeqIOReadPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <cstring>
#include <sstream>

namespace tl
{
    namespace
    {
        const std::chrono::milliseconds requestTimeout(5);
    }

    void ISeqRead::_init(
        const ftk::Path& path,
        const std::vector<ftk::MemFile>& mem,
        const IOOptions& options,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        IRead::_init(path, mem, options, logSystem);
        FTK_P();

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

        auto i = options.find("SeqIO/DefaultSpeed");
        if (i != options.end())
        {
            std::stringstream ss(i->second);
            ss >> _defaultSpeed;
        }

        p.thread.running = true;
        p.thread.thread = std::thread(
            [this]
            {
                FTK_P();
                try
                {
                    p.info = _getInfo(
                        _path.getFileName(true),
                        !_mem.empty() ? &_mem[0] : nullptr);
                    p.addTags(p.info);
                    _thread();
                }
                catch (const std::exception& e)
                {
                    if (auto logSystem = _logSystem.lock())
                    {
                        logSystem->print(
                            "tl::io::ISeqRead",
                            e.what(),
                            ftk::LogType::Error);
                    }
                }
                _finishRequests();
                {
                    std::unique_lock<std::mutex> lock(p.mutex.mutex);
                    p.mutex.stopped = true;
                }
                _cancelRequests();
            });
    }

    ISeqRead::ISeqRead() :
        _p(new Private)
    {}

    ISeqRead::~ISeqRead()
    {}

    std::future<IOInfo> ISeqRead::getInfo()
    {
        FTK_P();
        auto request = std::make_shared<Private::InfoRequest>();
        auto future = request->promise.get_future();
        bool valid = false;
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            if (!p.mutex.stopped)
            {
                valid = true;
                p.mutex.infoRequests.push_back(request);
            }
        }
        if (valid)
        {
            p.thread.cv.notify_one();
        }
        else
        {
            request->promise.set_value(IOInfo());
        }
        return future;
    }

    std::future<VideoData> ISeqRead::readVideo(
        const OTIO_NS::RationalTime& time,
        const IOOptions& options)
    {
        FTK_P();
        auto request = std::make_shared<Private::VideoRequest>();
        request->time = time;
        request->options = merge(options, _options);
        auto future = request->promise.get_future();
        bool valid = false;
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            if (!p.mutex.stopped)
            {
                valid = true;
                p.mutex.videoRequests.push_back(request);
            }
        }
        if (valid)
        {
            p.thread.cv.notify_one();
        }
        else
        {
            request->promise.set_value(VideoData());
        }
        return future;
    }

    void ISeqRead::cancelRequests()
    {
        _cancelRequests();
    }

    void ISeqRead::_finish()
    {
        FTK_P();
        p.thread.running = false;
        if (p.thread.thread.joinable())
        {
            p.thread.thread.join();
        }
    }

    void ISeqRead::_thread()
    {
        FTK_P();
        p.thread.logTimer = std::chrono::steady_clock::now();
        while (p.thread.running)
        {
            // Check requests.
            std::list<std::shared_ptr<Private::InfoRequest> > infoRequests;
            std::list<std::shared_ptr<Private::VideoRequest> > videoRequests;
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                if (p.thread.cv.wait_for(
                    lock,
                    requestTimeout,
                    [this]
                    {
                        return
                            !_p->mutex.infoRequests.empty() ||
                            !_p->mutex.videoRequests.empty();
                    }))
                {
                    infoRequests = std::move(p.mutex.infoRequests);
                    while (!p.mutex.videoRequests.empty() &&
                        (p.thread.videoRequestsInProgress.size() + videoRequests.size()) < p.threadCount)
                    {
                        videoRequests.push_back(p.mutex.videoRequests.front());
                        p.mutex.videoRequests.pop_front();
                    }
                }
            }

            // Information rquests.
            for (const auto& request : infoRequests)
            {
                request->promise.set_value(p.info);
            }

            // Initialize video requests.
            while (!videoRequests.empty())
            {
                auto request = videoRequests.front();
                videoRequests.pop_front();

                bool seq = false;
                std::string fileName;
                if (!_path.getNum().empty())
                {
                    seq = true;
                    fileName = _path.getFrame(static_cast<int>(request->time.value()), true);
                }
                else
                {
                    fileName = _path.getFileName(true);
                }
                const OTIO_NS::RationalTime time = request->time;
                const IOOptions options = request->options;

                const int64_t memIndex = seq ?
                    (static_cast<int64_t>(time.value()) - _startFrame) :
                    0;
                const ftk::MemFile* mem =
                    memIndex >= 0 && memIndex < static_cast<int64_t>(_mem.size()) ?
                    &_mem[memIndex] :
                    nullptr;

                if (mem)
                {
                    ftk::prefetch(mem->p, mem->size);
                }

                request->future = std::async(
                    std::launch::async,
                    [this, fileName, mem, time, options]
                    {
                        VideoData out;
                        try
                        {
                            out = _readVideo(fileName, mem, time, options);
                        }
                        catch (const std::exception&)
                        {
                            //! \todo How should this be handled?
                        }
                        return out;
                    });
                p.thread.videoRequestsInProgress.push_back(request);
            }

            // Check for finished video requests.
            //if (!p.thread.videoRequestsInProgress.empty())
            //{
            //    std::cout << "in progress: " << p.thread.videoRequestsInProgress.size() << std::endl;
            //}
            auto requestIt = p.thread.videoRequestsInProgress.begin();
            while (requestIt != p.thread.videoRequestsInProgress.end())
            {
                if ((*requestIt)->future.valid() &&
                    (*requestIt)->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
                {
                    //std::cout << "finished: " << requestIt->time << std::endl;
                    auto videoData = (*requestIt)->future.get();
                    (*requestIt)->promise.set_value(videoData);
                    requestIt = p.thread.videoRequestsInProgress.erase(requestIt);
                    continue;
                }
                ++requestIt;
            }

            // Logging.
            /*if (auto logSystem = _logSystem.lock())
            {
                const auto now = std::chrono::steady_clock::now();
                const std::chrono::duration<float> diff = now - p.thread.logTimer;
                if (diff.count() > 10.F)
                {
                    p.thread.logTimer = now;
                    const std::string id = ftk::Format("tl::io::ISeqRead {0}").arg(this);
                    size_t requestsSize = 0;
                    {
                        std::unique_lock<std::mutex> lock(p.mutex.mutex);
                        requestsSize = p.mutex.videoRequests.size();
                    }
                    logSystem->print(id, ftk::Format(
                        "\n"
                        "    Path: {0}\n"
                        "    Requests: {1}, {2} in progress\n"
                        "    Thread count: {3}").
                        arg(_path.get()).
                        arg(requestsSize).
                        arg(p.thread.videoRequestsInProgress.size()).
                        arg(p.threadCount));
                }
            }*/
        }
    }

    void ISeqRead::_finishRequests()
    {
        FTK_P();
        for (auto& request : p.thread.videoRequestsInProgress)
        {
            VideoData data;
            data.time = request->time;
            if (request->future.valid())
            {
                data = request->future.get();
            }
            request->promise.set_value(data);
        }
        p.thread.videoRequestsInProgress.clear();
    }

    void ISeqRead::_cancelRequests()
    {
        FTK_P();
        std::list<std::shared_ptr<Private::InfoRequest> > infoRequests;
        std::list<std::shared_ptr<Private::VideoRequest> > videoRequests;
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            infoRequests = std::move(p.mutex.infoRequests);
            videoRequests = std::move(p.mutex.videoRequests);
        }
        for (auto& request : infoRequests)
        {
            request->promise.set_value(IOInfo());
        }
        for (auto& request : videoRequests)
        {
            request->promise.set_value(VideoData());
        }
    }

    void ISeqRead::Private::addTags(IOInfo& info)
    {
        addVideoTags(info);
    }
}

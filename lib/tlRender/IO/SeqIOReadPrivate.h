// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/SeqIO.h>

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

namespace tl
{
    struct ISeqRead::Private
    {
        void addTags(IOInfo&);

        // This reader is on its way out; what remains of it reads
        // information rather than streams of frames, so the limit is only
        // here to bound the queue.
        size_t threadCount = 4;

        IOInfo info;

        struct InfoRequest
        {
            InfoRequest() {}
            InfoRequest(InfoRequest&&) = default;

            std::promise<IOInfo> promise;
        };

        struct VideoRequest
        {
            VideoRequest() {}
            VideoRequest(VideoRequest&&) = default;

            OTIO_NS::RationalTime time = invalidTime;
            IOOptions options;
            std::promise<VideoData> promise;
            std::future<VideoData> future;
        };

        struct Mutex
        {
            std::list<std::shared_ptr<InfoRequest> > infoRequests;
            std::list<std::shared_ptr<VideoRequest> > videoRequests;
            bool stopped = false;
            std::mutex mutex;
        };
        Mutex mutex;

        struct Thread
        {
            std::list<std::shared_ptr<VideoRequest> > videoRequestsInProgress;
            std::chrono::steady_clock::time_point logTimer;
            std::condition_variable cv;
            std::thread thread;
            std::atomic<bool> running{ false };
        };
        Thread thread;
    };
}

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/Timeline.h>

#include <tlRender/IO/Read.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

namespace tl
{
    struct TimelineInitCancellation::Private
    {
        std::atomic_bool cancelled{ false };
        std::mutex mutex;
        std::vector<std::shared_ptr<IRead> > reads;
    };

    TimelineInitCancellation::TimelineInitCancellation() :
        _p(new Private)
    {}

    TimelineInitCancellation::~TimelineInitCancellation()
    {
        cancel();
    }

    void TimelineInitCancellation::cancel()
    {
        _p->cancelled.store(true, std::memory_order_release);
        std::vector<std::shared_ptr<IRead> > reads;
        {
            std::lock_guard<std::mutex> lock(_p->mutex);
            reads = _p->reads;
        }
        for (const auto& read : reads)
        {
            if (read)
            {
                read->cancelIO();
            }
        }
    }

    bool TimelineInitCancellation::isCancelled() const
    {
        return _p->cancelled.load(std::memory_order_acquire);
    }

    void TimelineInitCancellation::_bind(const std::shared_ptr<IRead>& read)
    {
        {
            std::lock_guard<std::mutex> lock(_p->mutex);
            if (read &&
                std::find(_p->reads.begin(), _p->reads.end(), read) ==
                _p->reads.end())
            {
                _p->reads.push_back(read);
            }
        }
        if (isCancelled() && read)
        {
            read->cancelIO();
        }
    }

    void TimelineInitCancellation::_unbind(const std::shared_ptr<IRead>& read)
    {
        std::lock_guard<std::mutex> lock(_p->mutex);
        _p->reads.erase(
            std::remove(_p->reads.begin(), _p->reads.end(), read),
            _p->reads.end());
    }
}

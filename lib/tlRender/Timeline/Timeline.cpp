// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/TimelinePrivate.h>

#include <filesystem>

#include <tlRender/Timeline/Util.h>
#include <tlRender/Timeline/ZipPrivate.h>

#include <tlRender/IO/SeqIO.h>
#include <tlRender/IO/System.h>

#include <tlRender/Core/URL.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/Time.h>
#include <ftk/Core/Path.h>

#include <opentimelineio/externalReference.h>
#include <opentimelineio/gap.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/transition.h>

#include <algorithm>

namespace tl
{
    namespace
    {
        std::atomic<size_t> objectCount = 0;
    }

    Timeline::Timeline() :
        _p(new Private)
    {
        ++objectCount;
    }

    Timeline::~Timeline()
    {
        FTK_P();
        if (auto logSystem = p.logSystem.lock())
        {
            logSystem->print(
                ftk::Format("tl::~Timeline {0}").arg(p.logId),
                p.path.get());
        }

        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.thread.running = false;
        }
        p.thread.cv.notify_one();
        if (p.thread.thread.joinable())
        {
            p.thread.thread.join();
        }
        p.stopReadPool();

        --objectCount;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>& timeline,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(context, timeline, options);
        return out;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const ftk::Path& path,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(context, path, ftk::Path(), options);
        return out;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const ftk::Path& path,
        const ftk::Path& audioPath,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(context, path, audioPath, options);
        return out;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const std::string& fileName,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(
            context,
            ftk::Path(fileName, options.pathOptions),
            ftk::Path(),
            options);
        return out;
    }

    std::shared_ptr<Timeline> Timeline::create(
        const std::shared_ptr<ftk::Context>& context,
        const std::string& fileName,
        const std::string& audioFileName,
        const Options& options)
    {
        auto out = std::shared_ptr<Timeline>(new Timeline);
        out->_init(
            context,
            ftk::Path(fileName, options.pathOptions),
            ftk::Path(audioFileName, options.pathOptions),
            options);
        return out;
    }

    std::shared_ptr<ftk::Context> Timeline::getContext() const
    {
        return _p->context.lock();
    }
        
    const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline>& Timeline::getOTIOTimeline() const
    {
        return _p->otioTimeline;
    }

    const ftk::Path& Timeline::getPath() const
    {
        return _p->path;
    }

    const ftk::Path& Timeline::getAudioPath() const
    {
        return _p->audioPath;
    }

    const Options& Timeline::getOptions() const
    {
        return _p->options;
    }
    
    size_t Timeline::getVideoRequestMax() const
    {
        // At least one, whatever the options say. Zero here would not mean
        // "no limit", it would mean no request is ever picked up, and a
        // timeline without a thread would wait for one that never came.
        return std::max(_p->options.readThreadCount, size_t(1)) * 2;
    }

    size_t Timeline::getReadThreadCount() const
    {
        return _p->readPool.threads.size();
    }

    const OTIO_NS::TimeRange& Timeline::getTimeRange() const
    {
        return _p->timeRange;
    }

    OTIO_NS::RationalTime Timeline::getDuration() const
    {
        return _p->timeRange.duration();
    }

    const IOInfo& Timeline::getIOInfo() const
    {
        FTK_P();
        // Follow the media reference being read, so that the information
        // describes the media on screen rather than the media that happened to
        // be active when the timeline was read. The entries are fixed once the
        // timeline has been read, so returning a reference to one is safe.
        if (p.videoInfoClip)
        {
            const auto i = p.videoInfoByReference.find(
                getMediaReference(p.videoInfoClip));
            if (i != p.videoInfoByReference.end())
            {
                return i->second;
            }
        }
        return p.ioInfo;
    }

    std::string Timeline::getReadError() const
    {
        std::unique_lock<std::mutex> lock(_p->mutex.mutex);
        return _p->mutex.readError;
    }

    size_t Timeline::getReadErrorCount() const
    {
        std::unique_lock<std::mutex> lock(_p->mutex.mutex);
        return _p->mutex.readErrorCount;
    }

    size_t Timeline::getObjectCount()
    {
        return objectCount;
    }
}

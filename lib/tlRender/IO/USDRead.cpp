// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/USDPrivate.h>

namespace tl
{
    namespace usd
    {        
        struct VideoRead::Private
        {
            int64_t id = -1;
            std::shared_ptr<Render> render;
        };
                
        void VideoRead::_init(
            int64_t id,
            const std::shared_ptr<Render>& render,
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& memory,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IRead::_init(path, memory, options, logSystem);
            FTK_P();
            p.id = id;
            p.render = render;
        }

        VideoRead::VideoRead() :
            _p(new Private)
        {}

        VideoRead::~VideoRead()
        {}

        std::shared_ptr<VideoRead> VideoRead::create(
            int64_t id,
            const std::shared_ptr<Render>& render,
            const ftk::Path& path,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<VideoRead>(new VideoRead);
            out->_init(id, render, path, {}, options, logSystem);
            return out;
        }

        std::future<IOInfo> VideoRead::getInfo()
        {
            FTK_P();
            return p.render->getInfo(p.id, _path, _options);
        }
        
        std::future<VideoData> VideoRead::readVideo(
            const OTIO_NS::RationalTime& time,
            const IOOptions& options)
        {
            FTK_P();
            return p.render->render(p.id, _path, time, merge(options, _options));
        }
        
        void VideoRead::cancelRequests()
        {
            FTK_P();
            p.render->cancelRequests(p.id);
        }
    }
}


// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/FileBrowserThumbnails.h>

#include <tlRender/UI/ThumbnailSystem.h>

#include <tlRender/Timeline/Util.h>

#include <ftk/Core/Context.h>

namespace tl
{
    namespace ui
    {
        struct FileBrowserThumbnails::Private
        {
            std::weak_ptr<ThumbnailSystem> thumbnailSystem;

            // The readable extensions, gathered once: isSupported() is asked
            // of every entry in a directory.
            std::vector<std::string> exts;

            IOOptions ioOptions;
        };

        FileBrowserThumbnails::FileBrowserThumbnails(
            const std::shared_ptr<ftk::Context>& context) :
            _p(new Private)
        {
            FTK_P();
            p.thumbnailSystem = context->getSystem<ThumbnailSystem>();

            // Audio is left out: a waveform is not a thumbnail, and asking
            // for one of an audio file only spends a decode to get nothing.
            p.exts = getExts(
                context,
                static_cast<int>(FileType::Media) |
                static_cast<int>(FileType::Seq));
        }

        FileBrowserThumbnails::~FileBrowserThumbnails()
        {}

        std::shared_ptr<FileBrowserThumbnails> FileBrowserThumbnails::create(
            const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<FileBrowserThumbnails>(
                new FileBrowserThumbnails(context));
        }

        const IOOptions& FileBrowserThumbnails::getIOOptions() const
        {
            return _p->ioOptions;
        }

        void FileBrowserThumbnails::setIOOptions(const IOOptions& value)
        {
            _p->ioOptions = value;
        }

        bool FileBrowserThumbnails::isSupported(const ftk::Path& path) const
        {
            FTK_P();
            // testExt() takes an empty list to mean everything, which is the
            // opposite of what an empty list means here.
            return !p.exts.empty() && path.testExt(p.exts);
        }

        ftk::FileBrowserThumbnailRequest FileBrowserThumbnails::request(
            const ftk::Path& path,
            int height)
        {
            FTK_P();
            ftk::FileBrowserThumbnailRequest out;
            if (auto thumbnailSystem = p.thumbnailSystem.lock())
            {
                // One frame per file: keeping the timelines open held
                // every file in the folder, and the browser never comes
                // back for a second frame.
                ThumbnailRequest request = thumbnailSystem->getThumbnail(
                    path,
                    height,
                    std::nullopt,
                    p.ioOptions,
                    ThumbnailType::Browser);
                out.id = request.id;
                out.future = std::move(request.future);
            }
            return out;
        }

        void FileBrowserThumbnails::cancelRequests(
            const std::vector<uint64_t>& ids)
        {
            FTK_P();
            if (auto thumbnailSystem = p.thumbnailSystem.lock())
            {
                thumbnailSystem->cancelRequests(ids);
            }
        }
    }
}

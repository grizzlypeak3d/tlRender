// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/IO.h>

#include <ftk/UI/FileBrowser.h>

namespace tl
{
    namespace ui
    {
        //! File browser thumbnails.
        //!
        //! ftk has the file browser but not the image I/O to fill it. An
        //! application registers this with ftk::FileBrowserSystem and the
        //! browser shows media in place of the file icons:
        //!
        //!     context->getSystem<ftk::FileBrowserSystem>()->setThumbnails(
        //!         tl::ui::FileBrowserThumbnails::create(context));
        //!
        //! Only the built-in browser draws them; the native file dialog is
        //! the operating system's and shows whatever it shows.
        class TL_API_TYPE FileBrowserThumbnails : public ftk::IFileBrowserThumbnails
        {
        protected:
            FileBrowserThumbnails(const std::shared_ptr<ftk::Context>&);

        public:
            TL_API virtual ~FileBrowserThumbnails();

            //! Create new thumbnails.
            TL_API static std::shared_ptr<FileBrowserThumbnails> create(
                const std::shared_ptr<ftk::Context>&);

            //! Get the I/O options.
            TL_API const IOOptions& getIOOptions() const;

            //! Set the I/O options.
            TL_API void setIOOptions(const IOOptions&);

            TL_API bool isSupported(const ftk::Path&) const override;
            TL_API ftk::FileBrowserThumbnailRequest request(
                const ftk::Path&,
                int height) override;
            TL_API void cancelRequests(const std::vector<uint64_t>&) override;

        private:
            FTK_PRIVATE();
        };
    }
}

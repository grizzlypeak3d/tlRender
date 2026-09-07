// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/USD.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include <pxr/usd/usd/stage.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>

namespace tl
{
    namespace usd
    {
        //! USD renderer.
        class Render : public std::enable_shared_from_this<Render>
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);

            Render();

        public:
            ~Render();

            //! Create a new renderer.
            static std::shared_ptr<Render> create(
                const std::shared_ptr<ftk::LogSystem>&);
            
            //! Get information.
            std::future<IOInfo> getInfo(
                int64_t id,
                const ftk::Path& path,
                const IOOptions&);
            
            //! Render an image.
            std::future<VideoData> render(
                int64_t id,
                const ftk::Path& path,
                const OTIO_NS::RationalTime& time,
                const IOOptions&);

            //! Cancel requests.
            void cancelRequests(int64_t id);

        private:
            void _open(
                const std::string&,
                PXR_NS::UsdStageRefPtr&,
                std::shared_ptr<PXR_NS::UsdImagingGLEngine>&);
            void _run();
            void _finish();

            FTK_PRIVATE();
        };
    }
}


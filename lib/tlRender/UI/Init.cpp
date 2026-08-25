// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/Init.h>

#include <tlRender/UI/FileBrowserThumbnails.h>
#include <tlRender/UI/ThumbnailSystem.h>

#include <tlRender/GL/Render.h>

#include <tlRender/Timeline/Init.h>

#include <ftk/UI/Init.h>
#include <ftk/UI/FileBrowser.h>
#include <ftk/UI/IconSystem.h>

#include <tl_resource/IconResources.h>
#include <ftk/GL/System.h>


namespace tl
{
    namespace ui
    {
        void init(const std::shared_ptr<ftk::Context>& context)
        {
            ftk::uiInit(context);
            tl::init(context);
            context->getSystem<ftk::gl::System>()->setRenderFactory(std::make_shared<gl::RenderFactory>());
            ThumbnailSystem::create(context);

            // The file browser draws thumbnails of whatever it is handed the
            // I/O for, which is here rather than in ftk.
            context->getSystem<ftk::FileBrowserSystem>()->setThumbnails(
                FileBrowserThumbnails::create(context));

            auto iconSystem = context->getSystem<ftk::IconSystem>();
            // Every icon compiled into the library; the map is generated
            // from the files in etc/Icons.
            for (const auto& i : tl_resource::getIconResources())
            {
                iconSystem->add(i.first, *i.second);
            }
        }
    }
}

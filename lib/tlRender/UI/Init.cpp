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
#include <ftk/GL/System.h>

namespace tl_resource
{
    extern std::vector<uint8_t> ColorControls;
    extern std::vector<uint8_t> ColorPicker;
    extern std::vector<uint8_t> CompareA;
    extern std::vector<uint8_t> CompareB;
    extern std::vector<uint8_t> CompareButterfly;
    extern std::vector<uint8_t> CompareDifference;
    extern std::vector<uint8_t> CompareHorizontal;
    extern std::vector<uint8_t> CompareOverlay;
    extern std::vector<uint8_t> CompareTile;
    extern std::vector<uint8_t> CompareVertical;
    extern std::vector<uint8_t> CompareWipe;
    extern std::vector<uint8_t> Devices;
    extern std::vector<uint8_t> Export;
    extern std::vector<uint8_t> FileOpenAudio;
    extern std::vector<uint8_t> Files;
    extern std::vector<uint8_t> Hidden;
    extern std::vector<uint8_t> Info;
    extern std::vector<uint8_t> Magnify;
    extern std::vector<uint8_t> Messages;
    extern std::vector<uint8_t> PlaybackLoop;
    extern std::vector<uint8_t> PlaybackOnce;
    extern std::vector<uint8_t> PlaybackPingPong;
    extern std::vector<uint8_t> View;
    extern std::vector<uint8_t> Visible;
    extern std::vector<uint8_t> WindowSecondary;
    extern std::vector<uint8_t> tlRender;
}

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
            iconSystem->add("ColorControls", tl_resource::ColorControls);
            iconSystem->add("ColorPicker", tl_resource::ColorPicker);
            iconSystem->add("CompareA", tl_resource::CompareA);
            iconSystem->add("CompareB", tl_resource::CompareB);
            iconSystem->add("CompareButterfly", tl_resource::CompareButterfly);
            iconSystem->add("CompareDifference", tl_resource::CompareDifference);
            iconSystem->add("CompareHorizontal", tl_resource::CompareHorizontal);
            iconSystem->add("CompareOverlay", tl_resource::CompareOverlay);
            iconSystem->add("CompareTile", tl_resource::CompareTile);
            iconSystem->add("CompareVertical", tl_resource::CompareVertical);
            iconSystem->add("CompareWipe", tl_resource::CompareWipe);
            iconSystem->add("Devices", tl_resource::Devices);
            iconSystem->add("Export", tl_resource::Export);
            iconSystem->add("FileOpenAudio", tl_resource::FileOpenAudio);
            iconSystem->add("Files", tl_resource::Files);
            iconSystem->add("Hidden", tl_resource::Hidden);
            iconSystem->add("Info", tl_resource::Info);
            iconSystem->add("Magnify", tl_resource::Magnify);
            iconSystem->add("Messages", tl_resource::Messages);
            iconSystem->add("PlaybackLoop", tl_resource::PlaybackLoop);
            iconSystem->add("PlaybackOnce", tl_resource::PlaybackOnce);
            iconSystem->add("PlaybackPingPong", tl_resource::PlaybackPingPong);
            iconSystem->add("View", tl_resource::View);
            iconSystem->add("Visible", tl_resource::Visible);
            iconSystem->add("WindowSecondary", tl_resource::WindowSecondary);
            iconSystem->add("tlRender", tl_resource::tlRender);
        }
    }
}

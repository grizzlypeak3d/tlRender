// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/USDPrivate.h>

#include <ftk/Core/Error.h>
#include <ftk/Core/Format.h>

namespace tl
{
    namespace usd
    {
        TL_ENUM_IMPL(
            DrawMode,
            "Points",
            "Wireframe",
            "WireframeOnSurface",
            "ShadedFlat",
            "ShadedSmooth",
            "GeomOnly",
            "GeomFlat",
            "GeomSmooth");

        bool Options::operator == (const Options& other) const
        {
            return
                renderWidth == other.renderWidth &&
                complexity == other.complexity &&
                drawMode == other.drawMode &&
                enableLighting == other.enableLighting &&
                sRGB == other.sRGB &&
                stageCacheCount == other.stageCacheCount &&
                diskCacheGB == other.diskCacheGB;
        }

        bool Options::operator != (const Options& other) const
        {
            return !(*this == other);
        }

        IOOptions getOptions(const Options& value)
        {
            IOOptions out;
            out["USD/RenderWidth"] = ftk::Format("{0}").arg(value.renderWidth);
            out["USD/Complexity"] = ftk::Format("{0}").arg(value.complexity);
            out["USD/DrawMode"] = ftk::Format("{0}").arg(value.drawMode);
            out["USD/EnableLighting"] = ftk::Format("{0}").arg(value.enableLighting);
            out["USD/sRGB"] = ftk::Format("{0}").arg(value.sRGB);
            out["USD/StageCacheCount"] = ftk::Format("{0}").arg(value.stageCacheCount);
            out["USD/DiskCacheGB"] = ftk::Format("{0}").arg(value.diskCacheGB);
            return out;
        }

        struct ReadPlugin::Private
        {
            int64_t id = -1;
            std::mutex mutex;
            std::shared_ptr<Render> render;
        };
        
        void ReadPlugin::_init(const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            IReadPlugin::_init(
                "USD",
                {
                    { ".usd", FileType::Seq },
                    { ".usda", FileType::Seq },
                    { ".usdc", FileType::Seq },
                    { ".usdz", FileType::Seq }
                },
                logSystem);
            FTK_P();
            p.render = Render::create(logSystem);
        }
        
        ReadPlugin::ReadPlugin() :
            _p(new Private)
        {}
        
        ReadPlugin::~ReadPlugin()
        {}

        std::shared_ptr<ReadPlugin> ReadPlugin::create(
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<ReadPlugin>(new ReadPlugin);
            out->_init(logSystem);
            return out;
        }

        std::shared_ptr<IVideoRead> ReadPlugin::videoRead(
            const ftk::Path& path,
            const IOOptions& options)
        {
            FTK_P();
            int64_t id = -1;
            {
                std::unique_lock<std::mutex> lock(p.mutex);
                ++(p.id);
                id = p.id;
            }
            return VideoRead::create(id, p.render, path, options, _logSystem.lock());
        }

        std::shared_ptr<IVideoRead> ReadPlugin::videoRead(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& memory,
            const IOOptions& options)
        {
            // The renderer opens the stage itself; a memory location is not
            // something it can be handed.
            return videoRead(path, options);
        }

        std::string ReadPlugin::getPluginInfo(const IOOptions&) const
        {
            return ftk::Format("{0}.{1}.{2}").
                arg(PXR_MAJOR_VERSION).
                arg(PXR_MINOR_VERSION).
                arg(PXR_PATCH_VERSION);
        }

        void to_json(nlohmann::json& json, const Options& value)
        {
            json["RenderWidth"] = value.renderWidth;
            json["Complexity"] = value.complexity;
            json["DrawMode"] = to_string(value.drawMode);
            json["EnableLighting"] = value.enableLighting;
            json["sRGB"] = value.sRGB;
            json["StageCacheCount"] = value.stageCacheCount;
            json["DiskCacheGB"] = value.diskCacheGB;
        }

        void from_json(const nlohmann::json& json, Options& value)
        {
            json.at("RenderWidth").get_to(value.renderWidth);
            json.at("Complexity").get_to(value.complexity);
            from_string(json.at("DrawMode").get<std::string>(), value.drawMode);
            json.at("EnableLighting").get_to(value.enableLighting);
            json.at("sRGB").get_to(value.sRGB);
            json.at("StageCacheCount").get_to(value.stageCacheCount);
            json.at("DiskCacheGB").get_to(value.diskCacheGB);
        }
    }
}


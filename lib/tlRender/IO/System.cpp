// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/System.h>

#if defined(TLRENDER_FFMPEG_PLUGIN)
#include <tlRender/IO/FFmpeg.h>
#endif // TLRENDER_FFMPEG_PLUGIN
#if defined(TLRENDER_FFMPEG_CMD)
#include <tlRender/IO/FFmpegCmd.h>
#endif // TLRENDER_FFMPEG_CMD
#if defined(TLRENDER_EXR)
#include <tlRender/IO/EXR.h>
#endif // TLRENDER_EXR
#if defined(TLRENDER_OIIO)
#include <tlRender/IO/OIIO.h>
#endif // TLRENDER_OIIO
#if defined(TLRENDER_SVG)
#include <tlRender/IO/SVG.h>
#endif // TLRENDER_SVG
#if defined(TLRENDER_USD)
#include <tlRender/IO/USD.h>
#endif // TLRENDER_USD

#include <ftk/Core/Context.h>
#include <ftk/Core/String.h>

#include <iomanip>
#include <sstream>

namespace tl
{
    struct ReadSystem::Private
    {
        std::vector<std::string> names;
    };

    ReadSystem::ReadSystem(const std::shared_ptr<ftk::Context>& context) :
        ISystem(context, "tl::ReadSystem"),
        _p(new Private)
    {
        FTK_P();

        auto logSystem = context->getLogSystem();
        if (auto context = _context.lock())
        {
#if defined(TLRENDER_EXR)
            _plugins.push_back(exr::ReadPlugin::create(logSystem));
#endif // TLRENDER_EXR
            // Before OIIO, which claims a long list of extensions from
            // whatever it was built with.
#if defined(TLRENDER_SVG)
            _plugins.push_back(svg::ReadPlugin::create(logSystem));
#endif // TLRENDER_SVG
#if defined(TLRENDER_OIIO)
            _plugins.push_back(oiio::ReadPlugin::create(logSystem));
#endif // TLRENDER_OIIO
#if defined(TLRENDER_FFMPEG_CMD)
            _plugins.push_back(ffmpeg_cmd::ReadPlugin::create(logSystem));
#endif // TLRENDER_FFMPEG_CMD
#if defined(TLRENDER_FFMPEG_PLUGIN)
            _plugins.push_back(ffmpeg::ReadPlugin::create(logSystem));
#endif // TLRENDER_FFMPEG_PLUGIN
#if defined(TLRENDER_USD)
            _plugins.push_back(usd::ReadPlugin::create(logSystem));
#endif // TLRENDER_USD
        }

        for (const auto& plugin : _plugins)
        {
            p.names.push_back(plugin->getPluginName());
        }
        logSystem->print("tl::ReadSystem", "Plugins: " + ftk::join(p.names, ", "));
    }

    ReadSystem::~ReadSystem()
    {}

    std::shared_ptr<ReadSystem> ReadSystem::create(const std::shared_ptr<ftk::Context>& context)
    {
        auto out = context->getSystem<ReadSystem>();
        if (!out)
        {
            out = std::shared_ptr<ReadSystem>(new ReadSystem(context));
            context->addSystem(out);
        }
        return out;
    }

    std::shared_ptr<IReadPlugin> ReadSystem::getPlugin(const ftk::Path& path) const
    {
        const std::string ext = ftk::toLower(path.getExt());
        for (const auto& i : _plugins)
        {
            const auto& exts = i->getExts();
            if (exts.find(ext) != exts.end())
            {
                return i;
            }
        }
        return nullptr;
    }

    void ReadSystem::addPlugin(const std::shared_ptr<IReadPlugin>& plugin)
    {
        _plugins.push_back(plugin);
    }

    void ReadSystem::removePlugin(const std::shared_ptr<IReadPlugin>& plugin)
    {
        const auto i = std::find(_plugins.begin(), _plugins.end(), plugin);
        if (i != _plugins.end())
        {
            _plugins.erase(i);
        }
    }

    const std::vector<std::string>& ReadSystem::getNames() const
    {
        return _p->names;
    }

    std::set<std::string> ReadSystem::getExts(int types) const
    {
        std::set<std::string> out;
        for (const auto& i : _plugins)
        {
            const auto& exts = i->getExts(types);
            out.insert(exts.begin(), exts.end());
        }
        return out;
    }

    FileType ReadSystem::getFileType(const std::string& ext) const
    {
        FileType out = FileType::Unknown;
        const std::string lower = ftk::toLower(ext);
        for (const auto& plugin : _plugins)
        {
            for (auto fileType : { FileType::Media, FileType::Seq, FileType::Audio })
            {
                const auto& exts = plugin->getExts(static_cast<int>(fileType));
                if (const auto i = exts.find(lower); i != exts.end())
                {
                    out = fileType;
                    break;
                }
            }
        }
        return out;
    }

    namespace
    {
        // Hand the path to the first plugin that claims its extension. If
        // that plugin throws, keep trying the others and report the last
        // error only when none of them produced a reader.
        template<typename T, typename Create>
        std::shared_ptr<T> createRead(
            const std::vector<std::shared_ptr<IReadPlugin> >& plugins,
            const ftk::Path& path,
            Create&& create)
        {
            const std::string ext = ftk::toLower(path.getExt());
            std::string err;
            for (const auto& i : plugins)
            {
                try
                {
                    const auto& exts = i->getExts();
                    if (exts.find(ext) != exts.end())
                    {
                        return create(i);
                    }
                }
                catch (const std::exception& e)
                {
                    err = e.what();
                }
            }
            if (!err.empty())
            {
                throw std::runtime_error(err);
            }
            return nullptr;
        }
    }

    std::shared_ptr<IVideoRead> ReadSystem::videoRead(
        const ftk::Path& path,
        const IOOptions& options)
    {
        return createRead<IVideoRead>(
            _plugins,
            path,
            [&](const std::shared_ptr<IReadPlugin>& plugin)
            {
                return plugin->videoRead(path, options);
            });
    }

    std::shared_ptr<IVideoRead> ReadSystem::videoRead(
        const ftk::Path& path,
        const std::vector<ftk::MemFile>& memory,
        const IOOptions& options)
    {
        return createRead<IVideoRead>(
            _plugins,
            path,
            [&](const std::shared_ptr<IReadPlugin>& plugin)
            {
                return plugin->videoRead(path, memory, options);
            });
    }

    std::shared_ptr<IAudioRead> ReadSystem::audioRead(
        const ftk::Path& path,
        const IOOptions& options)
    {
        return createRead<IAudioRead>(
            _plugins,
            path,
            [&](const std::shared_ptr<IReadPlugin>& plugin)
            {
                return plugin->audioRead(path, options);
            });
    }

    std::shared_ptr<IAudioRead> ReadSystem::audioRead(
        const ftk::Path& path,
        const std::vector<ftk::MemFile>& memory,
        const IOOptions& options)
    {
        return createRead<IAudioRead>(
            _plugins,
            path,
            [&](const std::shared_ptr<IReadPlugin>& plugin)
            {
                return plugin->audioRead(path, memory, options);
            });
    }

    struct WriteSystem::Private
    {
        std::vector<std::string> names;
    };

    WriteSystem::WriteSystem(const std::shared_ptr<ftk::Context>& context) :
        ISystem(context, "tl::WriteSystem"),
        _p(new Private)
    {
        FTK_P();

        auto logSystem = context->getLogSystem();
        if (auto context = _context.lock())
        {
#if defined(TLRENDER_EXR)
            _plugins.push_back(exr::WritePlugin::create(logSystem));
#endif // TLRENDER_EXR
#if defined(TLRENDER_OIIO)
            _plugins.push_back(oiio::WritePlugin::create(logSystem));
#endif // TLRENDER_OIIO
#if defined(TLRENDER_FFMPEG_PLUGIN)
            _plugins.push_back(ffmpeg::WritePlugin::create(logSystem));
#endif // TLRENDER_FFMPEG_PLUGIN
        }

        for (const auto& plugin : _plugins)
        {
            p.names.push_back(plugin->getPluginName());
        }
        logSystem->print("tl::WriteSystem", "Plugins: " + ftk::join(p.names, ", "));
    }

    WriteSystem::~WriteSystem()
    {}

    std::shared_ptr<WriteSystem> WriteSystem::create(const std::shared_ptr<ftk::Context>& context)
    {
        auto out = context->getSystem<WriteSystem>();
        if (!out)
        {
            out = std::shared_ptr<WriteSystem>(new WriteSystem(context));
            context->addSystem(out);
        }
        return out;
    }

    std::shared_ptr<IWritePlugin> WriteSystem::getPlugin(const ftk::Path& path) const
    {
        const std::string ext = ftk::toLower(path.getExt());
        for (const auto& i : _plugins)
        {
            const auto& exts = i->getExts();
            if (exts.find(ext) != exts.end())
            {
                return i;
            }
        }
        return nullptr;
    }

    void WriteSystem::addPlugin(const std::shared_ptr<IWritePlugin>& plugin)
    {
        _plugins.push_back(plugin);
    }

    void WriteSystem::removePlugin(const std::shared_ptr<IWritePlugin>& plugin)
    {
        const auto i = std::find(_plugins.begin(), _plugins.end(), plugin);
        if (i != _plugins.end())
        {
            _plugins.erase(i);
        }
    }

    const std::vector<std::string>& WriteSystem::getNames() const
    {
        return _p->names;
    }

    std::set<std::string> WriteSystem::getExts(int types) const
    {
        std::set<std::string> out;
        for (const auto& i : _plugins)
        {
            const auto& exts = i->getExts(types);
            out.insert(exts.begin(), exts.end());
        }
        return out;
    }

    FileType WriteSystem::getFileType(const std::string& ext) const
    {
        FileType out = FileType::Unknown;
        const std::string lower = ftk::toLower(ext);
        for (const auto& plugin : _plugins)
        {
            for (auto fileType : { FileType::Media, FileType::Seq, FileType::Audio })
            {
                const auto& exts = plugin->getExts(static_cast<int>(fileType));
                if (const auto i = exts.find(lower); i != exts.end())
                {
                    out = fileType;
                    break;
                }
            }
        }
        return out;
    }

    std::shared_ptr<IWrite> WriteSystem::write(
        const ftk::Path& path,
        const IOInfo& info,
        const IOOptions& options)
    {
        const std::string ext = ftk::toLower(path.getExt());
        for (const auto& i : _plugins)
        {
            const auto& exts = i->getExts();
            if (exts.find(ext) != exts.end())
            {
                return i->write(path, info, options);
            }
        }
        return nullptr;
    }
}

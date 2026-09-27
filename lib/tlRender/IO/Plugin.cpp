// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/Plugin.h>

#include <ftk/Core/LogSystem.h>

namespace tl
{
    void IIO::_init(
        const ftk::Path& path,
        const IOOptions& options,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        _path = path;
        _options = options;
        _logSystem = logSystem;
    }

    namespace
    {
        std::atomic<size_t> objectCount = 0;
    }

    IIO::IIO()
    {
        ++objectCount;
    }

    IIO::~IIO()
    {
        --objectCount;
    }

    const ftk::Path& IIO::getPath() const
    {
        return _path;
    }

    size_t IIO::getObjectCount()
    {
        return objectCount;
    }

    struct IIOPlugin::Private
    {
        std::string pluginName;
        std::map<std::string, FileType> exts;
    };

    void IIOPlugin::_init(
        const std::string& name,
        const std::map<std::string, FileType>& exts,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        FTK_P();
        _logSystem = logSystem;
        p.pluginName = name;
        p.exts = exts;
    }

    IIOPlugin::IIOPlugin() :
        _p(new Private)
    {}

    IIOPlugin::~IIOPlugin()
    {}

    const std::string& IIOPlugin::getPluginName() const
    {
        return _p->pluginName;
    }

    std::string IIOPlugin::getPluginInfo(const IOOptions&) const
    {
        return std::string();
    }

    std::map<std::string, std::set<std::string> > IIOPlugin::getExtGroups() const
    {
        return {};
    }

    std::set<std::string> IIOPlugin::getExts(int types) const
    {
        std::set<std::string> out;
        for (const auto& i : _p->exts)
        {
            if (static_cast<int>(i.second) & types)
            {
                out.insert(i.first);
            }
        }
        return out;
    }
}

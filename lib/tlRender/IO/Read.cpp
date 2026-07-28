// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/Read.h>

#include <ftk/Core/LogSystem.h>

namespace tl
{
    void IRead::_init(
        const ftk::Path& path,
        const std::vector<ftk::MemFile>& mem,
        const IOOptions& options,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        IIO::_init(path, options, logSystem);
        _mem = mem;
    }

    IRead::IRead()
    {}

    IRead::~IRead()
    {}

    std::string IRead::getError() const
    {
        return std::string();
    }

    size_t IRead::getErrorCount() const
    {
        return 0;
    }

    IVideoRead::~IVideoRead()
    {}

    IAudioRead::~IAudioRead()
    {}

    struct IReadPlugin::Private
    {
    };

    void IReadPlugin::_init(
        const std::string& name,
        const std::map<std::string, FileType>& extensions,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        IIOPlugin::_init(name, extensions, logSystem);
    }

    IReadPlugin::IReadPlugin() :
        _p(new Private)
    {}

    IReadPlugin::~IReadPlugin()
    {}

    std::shared_ptr<IVideoRead> IReadPlugin::videoRead(
        const ftk::Path& path,
        const IOOptions& options)
    {
        return videoRead(path, {}, options);
    }

    std::shared_ptr<IVideoRead> IReadPlugin::videoRead(
        const ftk::Path&,
        const std::vector<ftk::MemFile>&,
        const IOOptions&)
    {
        return nullptr;
    }

    std::shared_ptr<IAudioRead> IReadPlugin::audioRead(
        const ftk::Path& path,
        const IOOptions& options)
    {
        return audioRead(path, {}, options);
    }

    std::shared_ptr<IAudioRead> IReadPlugin::audioRead(
        const ftk::Path&,
        const std::vector<ftk::MemFile>&,
        const IOOptions&)
    {
        return nullptr;
    }

    std::shared_ptr<IDecode> IReadPlugin::decode(const IOOptions&)
    {
        return nullptr;
    }
}
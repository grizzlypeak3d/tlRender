// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/Write.h>

#include <ftk/Core/LogSystem.h>

namespace tl
{
    void IWrite::_init(
        const ftk::Path& path,
        const IOOptions& options,
        const IOInfo& info,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        IIO::_init(path, options, logSystem);
        _info = info;
    }

    IWrite::IWrite()
    {}

    IWrite::~IWrite()
    {}

    void IWrite::writeAudio(
        const OTIO_NS::TimeRange&,
        const std::shared_ptr<Audio>&,
        const IOOptions&)
    {}

    void IWrite::finish()
    {}

    struct IWritePlugin::Private
    {
    };

    void IWritePlugin::_init(
        const std::string& name,
        const std::map<std::string, FileType>& extensions,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        IIOPlugin::_init(name, extensions, logSystem);
    }

    IWritePlugin::IWritePlugin() :
        _p(new Private)
    {}

    IWritePlugin::~IWritePlugin()
    {}

    bool IWritePlugin::_isCompatible(const ftk::ImageInfo& info, const IOOptions& options) const
    {
        return info.type != ftk::ImageType::None && info == getInfo(info, options);
    }

    ftk::ImageType getWriteType(ftk::ImageType value)
    {
        ftk::ImageType out = ftk::ImageType::RGBA_U8;
        if (ftk::getBitDepth(value) > 8)
        {
            switch (ftk::getChannelCount(value))
            {
            case 1: out = ftk::ImageType::L_U16; break;
            case 3: out = ftk::ImageType::RGB_U16; break;
            default: out = ftk::ImageType::RGBA_U16; break;
            }
        }
        return out;
    }
}
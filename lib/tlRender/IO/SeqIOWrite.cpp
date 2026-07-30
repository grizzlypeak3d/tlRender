// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/SeqIO.h>

#include <cstring>
#include <sstream>

namespace tl
{
    struct ISeqWrite::Private
    {
        std::string path;
        std::string baseName;
        std::string number;
        int pad = 0;
        std::string extension;

        float defaultSpeed = SeqOptions().defaultSpeed;
    };

    void ISeqWrite::_init(
        const ftk::Path& path,
        const IOInfo& info,
        const IOOptions& options,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        IWrite::_init(path, options, info, logSystem);

        FTK_P();

        if (const auto i = options.find("SeqIO/DefaultSpeed");
            i != options.end())
        {
            std::stringstream ss(i->second);
            ss >> p.defaultSpeed;
        }
    }

    ISeqWrite::ISeqWrite() :
        _p(new Private)
    {}

    ISeqWrite::~ISeqWrite()
    {}

    void ISeqWrite::writeVideo(
        const OTIO_NS::RationalTime& time,
        const std::shared_ptr<ftk::Image>& image,
        const IOOptions& options)
    {
        _writeVideo(
            _path.getFrame(static_cast<int>(time.value()), true),
            time,
            image,
            merge(options, _options));
    }
}
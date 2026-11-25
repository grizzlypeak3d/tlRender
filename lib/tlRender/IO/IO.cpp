// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/IO.h>

#include <sstream>

namespace tl
{
    ftk::ImageType getIntImageType(std::size_t channelCount, std::size_t bitDepth)
    {
        ftk::ImageType out = ftk::ImageType::None;
        switch (channelCount)
        {
        case 1:
            switch (bitDepth)
            {
            case 8: out = ftk::ImageType::L_U8; break;
            case 16: out = ftk::ImageType::L_U16; break;
            case 32: out = ftk::ImageType::L_U32; break;
            }
            break;
        case 2:
            switch (bitDepth)
            {
            case 8: out = ftk::ImageType::LA_U8; break;
            case 16: out = ftk::ImageType::LA_U16; break;
            case 32: out = ftk::ImageType::LA_U32; break;
            }
            break;
        case 3:
            switch (bitDepth)
            {
            case 8: out = ftk::ImageType::RGB_U8; break;
            case 10: out = ftk::ImageType::RGB_U10; break;
            case 16: out = ftk::ImageType::RGB_U16; break;
            case 32: out = ftk::ImageType::RGB_U32; break;
            }
            break;
        case 4:
            switch (bitDepth)
            {
            case 8: out = ftk::ImageType::RGBA_U8; break;
            case 16: out = ftk::ImageType::RGBA_U16; break;
            case 32: out = ftk::ImageType::RGBA_U32; break;
            }
            break;
        }
        return out;
    }

    namespace
    {
        bool parseBox(const std::string& value, ftk::Box2I& out)
        {
            std::stringstream ss(value);
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            if (!(ss >> x0 >> y0 >> x1 >> y1))
            {
                return false;
            }
            out = ftk::Box2I(ftk::V2I(x0, y0), ftk::V2I(x1, y1));
            return true;
        }

        const std::string* findPartTag(
            const ftk::ImageTags& tags,
            const std::string& prefix,
            const std::string& key)
        {
            if (!prefix.empty())
            {
                const auto i = tags.find(prefix + key);
                if (i != tags.end())
                {
                    return &i->second;
                }
            }
            const auto i = tags.find(key);
            return i != tags.end() ? &i->second : nullptr;
        }
    }

    bool isSourceTag(const std::string& value)
    {
        return
            0 == value.compare(0, 5, "File/") ||
            0 == value.compare(0, 6, "Video/") ||
            0 == value.compare(0, 6, "Audio/");
    }

    bool getDataWindow(
        const ftk::ImageTags& tags,
        ftk::Box2I& dataWindow,
        ftk::Box2I& displayWindow)
    {
        // The first part's are not prefixed, nor are another part's that
        // are the same as the first's.
        std::string prefix;
        const auto i = tags.find("Part");
        if (i != tags.end() && i->second != "0")
        {
            prefix = "Part " + i->second + "/";
        }
        const std::string* data = findPartTag(tags, prefix, "Data Window");
        const std::string* display = findPartTag(tags, prefix, "Display Window");
        return
            data && display &&
            parseBox(*data, dataWindow) &&
            parseBox(*display, displayWindow);
    }

    ftk::ImageType getFloatImageType(std::size_t channelCount, std::size_t bitDepth)
    {
        ftk::ImageType out = ftk::ImageType::None;
        switch (channelCount)
        {
        case 1:
            switch (bitDepth)
            {
            case 16: out = ftk::ImageType::L_F16; break;
            case 32: out = ftk::ImageType::L_F32; break;
            }
            break;
        case 2:
            switch (bitDepth)
            {
            case 16: out = ftk::ImageType::LA_F16; break;
            case 32: out = ftk::ImageType::LA_F32; break;
            }
            break;
        case 3:
            switch (bitDepth)
            {
            case 16: out = ftk::ImageType::RGB_F16; break;
            case 32: out = ftk::ImageType::RGB_F32; break;
            }
            break;
        case 4:
            switch (bitDepth)
            {
            case 16: out = ftk::ImageType::RGBA_F16; break;
            case 32: out = ftk::ImageType::RGBA_F32; break;
            }
            break;
        }
        return out;
    }

    IOOptions merge(const IOOptions& a, const IOOptions& b)
    {
        IOOptions out = b;
        for (const auto& i : a)
        {
            out[i.first] = i.second;
        }
        return out;
    }

    const ftk::ImageInfo& getVideoInfo(const IOInfo& info, int layer)
    {
        const size_t index =
            layer >= 0 && static_cast<size_t>(layer) < info.video.size() ?
            static_cast<size_t>(layer) :
            0;
        return info.video[index];
    }

    IOInfo merge(const IOInfo& video, const IOInfo& audio)
    {
        IOInfo out = video;
        out.audio = audio.audio;
        out.audioTime = audio.audioTime;
        out.audioSource = audio.audioSource;
        for (const auto& tag : audio.tags)
        {
            out.tags[tag.first] = tag.second;
        }
        return out;
    }
}

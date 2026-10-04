// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/RenderPrivate.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <list>
#include <mutex>
#include <stdexcept>

#if defined(TLRENDER_OCIO)
namespace OCIO = OCIO_NAMESPACE;
#endif // TLRENDER_OCIO

namespace tl
{
    ftk::Box2I xform(const ftk::Box2I& box, const ftk::M44F& vm)
    {
        ftk::Box2I out;
        const ftk::V3F v0 = vm * ftk::V3F(box.min.x, box.min.y, 0.F);
        const ftk::V3F v1 = vm * ftk::V3F(box.max.x + 1, box.max.y + 1, 0.F);
        out.min.x = std::round(v0.x);
        out.min.y = std::round(v0.y);
        out.max.x = std::round(v1.x - 1);
        out.max.y = std::round(v1.y - 1);
        return out;
    }

#if defined(TLRENDER_OCIO)
    namespace
    {
        // What a configuration that has been read is remembered by.
        struct OCIOConfigKey
        {
            OCIOConfig  kind = OCIOConfig::BuiltIn;
            std::string fileName;
            uintmax_t   size = 0;
            int64_t     time = 0;

            bool operator == (const OCIOConfigKey&) const = default;
        };
    }

    OCIO::ConstConfigRcPtr getOCIOConfig(const OCIOOptions& options)
    {
        OCIOConfigKey key;
        key.kind = options.config;
        switch (options.config)
        {
        case OCIOConfig::EnvVar:
            if (const char* env = std::getenv("OCIO"))
            {
                key.fileName = env;
            }
            break;
        case OCIOConfig::File:
            key.fileName = options.fileName;
            break;
        default: break;
        }
        if (!key.fileName.empty())
        {
            std::error_code ec;
            const std::filesystem::path path(key.fileName);
            const auto size = std::filesystem::file_size(path, ec);
            if (!ec)
            {
                key.size = size;
            }
            const auto time = std::filesystem::last_write_time(path, ec);
            if (!ec)
            {
                key.time = time.time_since_epoch().count();
            }
        }

        // Most recently used first, and bounded: what is in play is
        // the configuration the viewport draws through and whatever
        // the timeline items use.
        static std::mutex mutex;
        static std::list<
            std::pair<OCIOConfigKey, OCIO::ConstConfigRcPtr> > cache;
        std::unique_lock<std::mutex> lock(mutex);
        for (auto i = cache.begin(); i != cache.end(); ++i)
        {
            if (i->first == key)
            {
                cache.splice(cache.begin(), cache, i);
                return cache.front().second;
            }
        }

        OCIO::ConstConfigRcPtr out;
        switch (options.config)
        {
        case OCIOConfig::BuiltIn:
            out = OCIO::Config::CreateFromFile("ocio://default");
            break;
        case OCIOConfig::EnvVar:
            if (hasOCIOEnvVar())
            {
                out = OCIO::Config::CreateFromEnv();
            }
            break;
        case OCIOConfig::File:
            if (!options.fileName.empty())
            {
                out = OCIO::Config::CreateFromFile(options.fileName.c_str());
            }
            break;
        default: break;
        }
        if (out)
        {
            cache.push_front(std::make_pair(key, out));
            while (cache.size() > 4)
            {
                cache.pop_back();
            }
        }
        return out;
    }

    std::string getOCIOOptionsKey(const OCIOOptions& options)
    {
        return
            std::string(options.enabled ? "1" : "0") + '\n' +
            std::to_string(static_cast<int>(options.config)) + '\n' +
            options.fileName + '\n' +
            options.input + '\n' +
            options.display + '\n' +
            options.view + '\n' +
            options.look;
    }

    OCIOProcessors getOCIOProcessors(const OCIOOptions& options)
    {
        OCIOProcessors out;
        out.config = getOCIOConfig(options);
        if (!out.config)
        {
            throw std::runtime_error("Cannot get OCIO configuration");
        }

        std::string displaySrc = options.input;
        if (out.config->hasRole(OCIO::ROLE_SCENE_LINEAR))
        {
            out.toLinear = out.config->getProcessor(
                options.input.c_str(),
                OCIO::ROLE_SCENE_LINEAR);
            if (!out.toLinear)
            {
                throw std::runtime_error("Cannot get OCIO processor");
            }
            displaySrc = OCIO::ROLE_SCENE_LINEAR;
        }

        out.transform = OCIO::DisplayViewTransform::Create();
        if (!out.transform)
        {
            throw std::runtime_error("Cannot create OCIO transform");
        }
        out.transform->setSrc(displaySrc.c_str());
        out.transform->setDisplay(options.display.c_str());
        out.transform->setView(options.view.c_str());

        out.lvp = OCIO::LegacyViewingPipeline::Create();
        if (!out.lvp)
        {
            throw std::runtime_error("Cannot create OCIO viewing pipeline");
        }
        out.lvp->setDisplayViewTransform(out.transform);
        out.lvp->setLooksOverrideEnabled(true);
        out.lvp->setLooksOverride(options.look.c_str());

        out.display = out.lvp->getProcessor(
            out.config,
            out.config->getCurrentContext());
        if (!out.display)
        {
            throw std::runtime_error("Cannot get OCIO processor");
        }
        return out;
    }

    OCIOLUTProcessor getOCIOLUTProcessor(const LUTOptions& options)
    {
        OCIOLUTProcessor out;
        out.config = OCIO::Config::CreateRaw();
        if (!out.config)
        {
            throw std::runtime_error("Cannot create OCIO configuration");
        }
        out.transform = OCIO::FileTransform::Create();
        if (!out.transform)
        {
            throw std::runtime_error("Cannot create OCIO transform");
        }
        out.transform->setSrc(options.fileName.c_str());
        out.transform->setDirection(
            LUTDirection::Inverse == options.direction ?
                OCIO::TRANSFORM_DIR_INVERSE :
                OCIO::TRANSFORM_DIR_FORWARD);
        out.transform->validate();
        out.processor = out.config->getProcessor(out.transform);
        if (!out.processor)
        {
            throw std::runtime_error("Cannot get OCIO processor");
        }
        return out;
    }
#endif // TLRENDER_OCIO
}

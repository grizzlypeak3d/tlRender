// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/System.h>

#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/Player.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>
#include <ftk/Core/LogSystem.h>
#include <ftk/Core/Timer.h>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorIO.h>
#endif // TLRENDER_OCIO

#include <mutex>

#if defined(TLRENDER_OCIO)
namespace OCIO = OCIO_NAMESPACE;
#endif // TLRENDER_OCIO

namespace tl
{
    namespace
    {
#if defined(TLRENDER_OCIO)
        // OpenColorIO writes to stderr, where nothing else here writes and
        // where a person running the application never looks. Its messages
        // go to the log with everything else: what a configuration says
        // about itself belongs beside what was made of it.
        //
        // The function it is given is global and outlives any one system,
        // so the log is held weakly and the function put back on the way
        // out.
        std::mutex ocioLogMutex;
        std::weak_ptr<ftk::LogSystem> ocioLogSystem;

        void ocioLog(const char* message)
        {
            std::shared_ptr<ftk::LogSystem> logSystem;
            {
                std::unique_lock<std::mutex> lock(ocioLogMutex);
                logSystem = ocioLogSystem.lock();
            }
            if (!logSystem || !message)
            {
                return;
            }
            // "[OpenColorIO Info]: ..." says both where it came from and
            // how much it matters; the log has its own places for those.
            std::string s = message;
            ftk::LogType type = ftk::LogType::Message;
            const std::string prefix = "[OpenColorIO ";
            if (0 == s.compare(0, prefix.size(), prefix))
            {
                const size_t end = s.find("]: ");
                if (end != std::string::npos)
                {
                    const std::string level = s.substr(prefix.size(), end - prefix.size());
                    if ("Warning" == level)
                    {
                        type = ftk::LogType::Warning;
                    }
                    else if ("Error" == level)
                    {
                        type = ftk::LogType::Error;
                    }
                    s = s.substr(end + 3);
                }
            }
            ftk::removeTrailingNewlines(s);
            logSystem->print("OpenColorIO", s, type);
        }
#endif // TLRENDER_OCIO
    }

    struct System::Private
    {
        std::vector<std::weak_ptr<Player> > players;
        std::shared_ptr<ftk::Timer> logTimer;
    };

    System::System(const std::shared_ptr<ftk::Context>& context) :
        ISystem(context, "tl::System"),
        _p(new Private)
    {
        FTK_P();

        std::vector<std::string> s;
        for (const auto& ext : getLUTFormatExts())
        {
            s.push_back(ext);
        }
        _log(ftk::Format("\n    LUT formats: {0}").arg(ftk::join(s, ", ")));

#if defined(TLRENDER_OCIO)
        {
            std::unique_lock<std::mutex> lock(ocioLogMutex);
            ocioLogSystem = context->getLogSystem();
        }
        OCIO::SetLoggingFunction(ocioLog);
#endif // TLRENDER_OCIO

        p.logTimer = ftk::Timer::create(context);
        p.logTimer->setRepeating(true);
        p.logTimer->start(
            std::chrono::milliseconds(10000),
            [this]
            {
                std::vector<std::string> s;
                s.push_back(ftk::Format("    * Audio count: {0}").arg(Audio::getObjectCount()));
                s.push_back(ftk::Format("    * Audio byte count: {0}").arg(Audio::getTotalByteCount()));
                s.push_back(ftk::Format("    * I/O count: {0}").arg(IIO::getObjectCount()));
                s.push_back(ftk::Format("    * Timeline count: {0}").arg(Timeline::getObjectCount()));
                s.push_back(ftk::Format("    * Player count: {0}").arg(Player::getObjectCount()));
                _log("\n" + ftk::join(s, "\n"));
            });
    }

    System::~System()
    {
#if defined(TLRENDER_OCIO)
        OCIO::ResetToDefaultLoggingFunction();
        std::unique_lock<std::mutex> lock(ocioLogMutex);
        ocioLogSystem.reset();
#endif // TLRENDER_OCIO
    }

    std::shared_ptr<System> System::create(const std::shared_ptr<ftk::Context>& context)
    {
        auto out = context->getSystem<System>();
        if (!out)
        {
            out = std::shared_ptr<System>(new System(context));
            context->addSystem(out);
        }
        return out;
    }

    void System::tick()
    {
        FTK_P();

        // Delete the expired players.
        auto players = p.players;
        auto i = players.begin();
        while (i != players.end())
        {
            if (i->expired())
            {
                i = players.erase(i);
            }
            else
            {
                ++i;
            }
        }

        // Tick the active players.
        for (i = players.begin(); i != players.end(); ++i)
        {
            if (auto player = i->lock())
            {
                player->_tick();
            }
        }
    }

    std::chrono::milliseconds System::getTickTime() const
    {
        return std::chrono::milliseconds(1);
    }

    void System::_addPlayer(const std::shared_ptr<Player>& player)
    {
        _p->players.push_back(player);
    }
}

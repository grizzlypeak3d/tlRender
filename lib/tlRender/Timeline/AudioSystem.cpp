// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/AudioSystem.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>
#include <ftk/Core/Time.h>

#if defined(FTK_SDL2)
#include <SDL2/SDL.h>
#endif // FTK_SDL2
#if defined(FTK_SDL3)
#include <SDL3/SDL.h>
#endif // FTK_SDL3

#include <array>
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

namespace tl
{
    namespace
    {
        const std::chrono::seconds timeout(3);
    }

    bool AudioDeviceID::operator == (const AudioDeviceID& other) const
    {
        return
            number == other.number &&
            name == other.name;
    }

    bool AudioDeviceID::operator != (const AudioDeviceID& other) const
    {
        return !(*this == other);
    }

    bool AudioDeviceInfo::operator == (const AudioDeviceInfo& other) const
    {
        return
            id == other.id &&
            info == other.info;
    }

    bool AudioDeviceInfo::operator != (const AudioDeviceInfo& other) const
    {
        return !(*this == other);
    }

    struct AudioSystem::Private
    {
        bool init = false;
        std::vector<std::string> drivers;
        std::string currentDriver;
        std::shared_ptr<ftk::ObservableList<AudioDeviceInfo> > devices;
        std::shared_ptr<ftk::Observable<AudioDeviceInfo> > defaultDevice;

        struct Mutex
        {
            std::vector<AudioDeviceInfo> devices;
            AudioDeviceInfo defaultDevice;
            std::mutex mutex;
        };
        Mutex mutex;
        struct Thread
        {
            std::vector<AudioDeviceInfo> devices;
            AudioDeviceInfo defaultDevice;
            std::thread thread;
            std::atomic<bool> running;
            std::condition_variable stop;
            std::mutex stopMutex;
        };
        Thread thread;
    };

    AudioSystem::AudioSystem(const std::shared_ptr<ftk::Context>& context) :
        ISystem(context, "tl::AudioSystem"),
        _p(new Private)
    {
        FTK_P();

#if defined(FTK_SDL2) || defined(FTK_SDL3)
#if defined(FTK_SDL2)
        p.init = SDL_Init(SDL_INIT_AUDIO) >= 0;
#elif defined(FTK_SDL3)
        p.init = SDL_Init(SDL_INIT_AUDIO);
#endif // FTK_SDL2
        if (!p.init)
        {
            std::stringstream ss;
            ss << "Cannot initialize SDL: " << SDL_GetError();
            _log(ss.str(), ftk::LogType::Error);
        }
        if (p.init)
        {
            const int count = SDL_GetNumAudioDrivers();
            for (int i = 0; i < count; ++i)
            {
                p.drivers.push_back(SDL_GetAudioDriver(i));
            }
            {
                std::stringstream ss;
                ss << "Audio drivers: " << ftk::join(p.drivers, ", ");
                _log(ss.str());
            }
            p.currentDriver = SDL_GetCurrentAudioDriver();
            {
                std::stringstream ss;
                ss << "Current audio driver: " << p.currentDriver;
                _log(ss.str());
            }
        }
#endif // FTK_SDL2

        const std::vector<AudioDeviceInfo> devices = _getDevices();
        const AudioDeviceInfo defaultDevice = _getDefaultDevice();

        p.devices = ftk::ObservableList<AudioDeviceInfo>::create(devices);
        p.defaultDevice = ftk::Observable<AudioDeviceInfo>::create(defaultDevice);

        p.mutex.devices = devices;
        p.mutex.defaultDevice = defaultDevice;

#if defined(FTK_SDL2) || defined(FTK_SDL3)
        if (p.init)
        {
            p.thread.running = true;
            p.thread.thread = std::thread(
                [this]
                {
                    while (_p->thread.running)
                    {
                        const auto t0 = std::chrono::steady_clock::now();
                        _run();
                        // Wait out the rest of the tick, but wake as soon as
                        // the destructor clears the flag. Sleeping instead
                        // made shutdown wait for the current tick to expire,
                        // and this one is three seconds long.
                        const auto t1 = std::chrono::steady_clock::now();
                        const auto period =
                            std::chrono::duration_cast<std::chrono::microseconds>(timeout);
                        const auto elapsed =
                            std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
                        std::unique_lock<std::mutex> lock(_p->thread.stopMutex);
                        _p->thread.stop.wait_for(
                            lock,
                            period > elapsed ? period - elapsed : std::chrono::microseconds(0),
                            [this] { return !_p->thread.running; });
                    }
                });
        }
#endif // FTK_SDL2
    }

    AudioSystem::~AudioSystem()
    {
        FTK_P();
        {
            // Under the mutex so the thread cannot test the flag and begin
            // waiting between the store and the notify.
            std::unique_lock<std::mutex> lock(p.thread.stopMutex);
            p.thread.running = false;
        }
        p.thread.stop.notify_one();
        if (p.thread.thread.joinable())
        {
            p.thread.thread.join();
        }
    }

    std::shared_ptr<AudioSystem> AudioSystem::create(const std::shared_ptr<ftk::Context>& context)
    {
        auto out = context->getSystem<AudioSystem>();
        if (!out)
        {
            out = std::shared_ptr<AudioSystem>(new AudioSystem(context));
            context->addSystem(out);
        }
        return out;
    }

    const std::vector<std::string>& AudioSystem::getDrivers() const
    {
        return _p->drivers;
    }

    const std::string& AudioSystem::getCurrentDriver() const
    {
        return _p->currentDriver;
    }

    const std::vector<AudioDeviceInfo>& AudioSystem::getDevices() const
    {
        return _p->devices->get();
    }

    std::shared_ptr<ftk::IObservableList<AudioDeviceInfo> > AudioSystem::observeDevices() const
    {
        return _p->devices;
    }

    AudioDeviceInfo AudioSystem::getDefaultDevice() const
    {
        return _p->defaultDevice->get();
    }

    std::shared_ptr<ftk::IObservable<AudioDeviceInfo> > AudioSystem::observeDefaultDevice() const
    {
        return _p->defaultDevice;
    }

    void AudioSystem::tick()
    {
        FTK_P();
        std::vector<AudioDeviceInfo> devices;
        AudioDeviceInfo defaultDevice;
        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            devices = p.mutex.devices;
            defaultDevice = p.mutex.defaultDevice;
        }
        p.devices->setIfChanged(devices);
        p.defaultDevice->setIfChanged(defaultDevice);
    }

    std::chrono::milliseconds AudioSystem::getTickTime() const
    {
        return timeout;
    }

    namespace
    {
#if defined(FTK_SDL2) || defined(FTK_SDL3)
        //! \todo This is duplicated in AudioSystem.cpp and PlayerAudio.cpp
        [[maybe_unused]] AudioType fromSDL(SDL_AudioFormat value)
        {
            AudioType out = AudioType::F32;
            if (SDL_AUDIO_BITSIZE(value) == 8 &&
                SDL_AUDIO_ISSIGNED(value) &&
                !SDL_AUDIO_ISFLOAT(value))
            {
                out = AudioType::S8;
            }
            else if (SDL_AUDIO_BITSIZE(value) == 16 &&
                SDL_AUDIO_ISSIGNED(value) &&
                !SDL_AUDIO_ISFLOAT(value))
            {
                out = AudioType::S16;
            }
            else if (SDL_AUDIO_BITSIZE(value) == 32 &&
                SDL_AUDIO_ISSIGNED(value) &&
                !SDL_AUDIO_ISFLOAT(value))
            {
                out = AudioType::S32;
            }
            else if (SDL_AUDIO_BITSIZE(value) == 32 &&
                SDL_AUDIO_ISSIGNED(value) &&
                SDL_AUDIO_ISFLOAT(value))
            {
                out = AudioType::F32;
            }
            return out;
        }
#endif // FTK_SDL2
    }

    std::vector<AudioDeviceInfo> AudioSystem::_getDevices()
    {
        FTK_P();
        std::vector<AudioDeviceInfo> out;
#if defined(FTK_SDL2)
        const int count = SDL_GetNumAudioDevices(0);
        for (int i = 0; i < count; ++i)
        {
            AudioDeviceInfo device;
            device.id.number = i;
            device.id.name = SDL_GetAudioDeviceName(i, 0);
            device.info.channelCount = 2;
            device.info.type = AudioType::F32;
            device.info.sampleRate = 48000;
            out.push_back(device);
        }
#elif defined(FTK_SDL3)
        int count = 0;
        SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
        for (int i = 0; i < count; ++i)
        {
            AudioDeviceInfo device;
            device.id.number = ids[i];
            device.id.name = SDL_GetAudioDeviceName(ids[i]);
            SDL_AudioSpec spec;
            int sampleFrames = 0;
            SDL_GetAudioDeviceFormat(ids[i], &spec, &sampleFrames);
            device.info.channelCount = spec.channels;
            device.info.type = fromSDL(spec.format);
            device.info.sampleRate = spec.freq;
            out.push_back(device);
        }
        SDL_free(ids);
#endif // FTK_SDL2
        return out;
    }

    AudioDeviceInfo AudioSystem::_getDefaultDevice()
    {
        AudioDeviceInfo out;
#if defined(FTK_SDL2)
        out.info.channelCount = 2;
        out.info.type = AudioType::F32;
        out.info.sampleRate = 48000;
#elif defined(FTK_SDL3)
        out.id.name = "Default";
        SDL_AudioSpec spec;
        int sampleFrames = 0;
        SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &sampleFrames);
        out.info.channelCount = spec.channels;
        out.info.type = fromSDL(spec.format);
        out.info.sampleRate = spec.freq;
#endif // FTK_SDL2
        return out;
    }

    void AudioSystem::_run()
    {
        FTK_P();
#if defined(FTK_SDL2) || defined(FTK_SDL3)

        const std::vector<AudioDeviceInfo> devices = _getDevices();
        const AudioDeviceInfo defaultDevice = _getDefaultDevice();
        if (devices != p.thread.devices)
        {
            p.thread.devices = devices;

            std::vector<std::string> log;
            log.push_back(std::string());
            for (size_t i = 0; i < devices.size(); ++i)
            {
                const auto& device = devices[i];
                {
                    std::stringstream ss;
                    ss << "    Device " << device.id.number << ": " << device.id.name << "\n" <<
                        "    * Channels: " << device.info.channelCount << "\n" <<
                        "    * Type: " << device.info.type << "\n" <<
                        "    * Sample rate: " << device.info.sampleRate;
                    log.push_back(ss.str());
                }
            }
            _log(ftk::join(log, "\n"));
        }
        if (defaultDevice != p.thread.defaultDevice)
        {
            p.thread.defaultDevice = defaultDevice;

            std::stringstream ss;
            ss << "\n" <<
                "    Default device " << defaultDevice.id.number << ": " << defaultDevice.id.name << "\n" <<
                "    * Channels: " << defaultDevice.info.channelCount << "\n" <<
                "    * Type: " << defaultDevice.info.type << "\n" <<
                "    * Sample rate: " << defaultDevice.info.sampleRate;
            _log(ss.str());
        }

        {
            std::unique_lock<std::mutex> lock(p.mutex.mutex);
            p.mutex.devices = p.thread.devices;
            p.mutex.defaultDevice = p.thread.defaultDevice;
        }
#endif // FTK_SDL2
    }
}

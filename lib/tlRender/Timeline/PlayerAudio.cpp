// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/PlayerPrivate.h>

#include <tlRender/Timeline/Util.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>

namespace tl
{
    const AudioDeviceID& Player::getAudioDevice() const
    {
        return _p->audioDevice->get();
    }

    std::shared_ptr<ftk::IObservable<AudioDeviceID> > Player::observeAudioDevice() const
    {
        return _p->audioDevice;
    }

    void Player::setAudioDevice(const AudioDeviceID& value)
    {
        FTK_P();
        if (p.audioDevice->setIfChanged(value))
        {
            if (auto context = getContext())
            {
                p.audioInit(context);
            }
        }
    }

    float Player::getVolume() const
    {
        return _p->volume->get();
    }

    std::shared_ptr<ftk::IObservable<float> > Player::observeVolume() const
    {
        return _p->volume;
    }

    void Player::setVolume(float value)
    {
        FTK_P();
        if (p.volume->setIfChanged(ftk::clamp(value, 0.F, 1.F)))
        {
            std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
            p.audioMutex.state.volume = value;
        }
    }

    bool Player::isMuted() const
    {
        return _p->mute->get();
    }

    std::shared_ptr<ftk::IObservable<bool> > Player::observeMute() const
    {
        return _p->mute;
    }

    void Player::setMute(bool value)
    {
        FTK_P();
        if (p.mute->setIfChanged(value))
        {
            std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
            p.audioMutex.state.mute = value;
        }
    }

    const std::vector<bool>& Player::getChannelMute() const
    {
        return _p->channelMute->get();
    }

    std::shared_ptr<ftk::IObservableList<bool> > Player::observeChannelMute() const
    {
        return _p->channelMute;
    }

    void Player::setChannelMute(const std::vector<bool>& value)
    {
        FTK_P();
        if (p.channelMute->setIfChanged(value))
        {
            std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
            p.audioMutex.state.channelMute = value;
        }
    }

    double Player::getAudioOffset() const
    {
        return _p->audioOffset->get();
    }

    std::shared_ptr<ftk::IObservable<double> > Player::observeAudioOffset() const
    {
        return _p->audioOffset;
    }

    void Player::setAudioOffset(double value)
    {
        FTK_P();
        if (p.audioOffset->setIfChanged(value))
        {
            {
                std::unique_lock<std::mutex> lock(p.mutex.mutex);
                p.mutex.state.audioOffset = value;
            }
            {
                std::unique_lock<std::mutex> lock(p.audioMutex.mutex);
                p.audioMutex.state.audioOffset = value;
            }
        }
    }

    const std::vector<AudioFrame>& Player::getCurrentAudio() const
    {
        return _p->currentAudioFrame->get();
    }

    std::shared_ptr<ftk::IObservableList<AudioFrame> > Player::observeCurrentAudio() const
    {
        return _p->currentAudioFrame;
    }

    bool Player::Private::hasAudio() const
    {
        bool out = false;
#if defined(FTK_SDL2) || defined(FTK_SDL3)
        out = audioDevices && sourceAudioInfo.isValid();
#endif // FTK_SDL2
        return out;
    }

    namespace
    {
#if defined(FTK_SDL2)
        SDL_AudioFormat toSDL(AudioType value)
        {
            SDL_AudioFormat out = 0;
            switch (value)
            {
            case AudioType::S8: out = AUDIO_S8; break;
            case AudioType::S16: out = AUDIO_S16; break;
            case AudioType::S32: out = AUDIO_S32; break;
            case AudioType::F32: out = AUDIO_F32; break;
            default: break;
            }
            return out;
        }
#elif defined(FTK_SDL3)
        SDL_AudioFormat toSDL(AudioType value)
        {
            SDL_AudioFormat out = SDL_AUDIO_UNKNOWN;
            switch (value)
            {
            case AudioType::S8: out = SDL_AUDIO_S8; break;
            case AudioType::S16: out = SDL_AUDIO_S16; break;
            case AudioType::S32: out = SDL_AUDIO_S32; break;
            case AudioType::F32: out = SDL_AUDIO_F32; break;
            default: break;
            }
            return out;
        }
#endif // FTK_SDL2

#if defined(FTK_SDL2) || defined(FTK_SDL3)
        //! \todo This is duplicated in AudioSystem.cpp and PlayerAudio.cpp
        AudioType fromSDL(SDL_AudioFormat value)
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

    void Player::Private::audioInit(const std::shared_ptr<ftk::Context>& context)
    {
#if defined(FTK_SDL2) || defined(FTK_SDL3)

#if defined(FTK_SDL2)
        if (sdlID > 0)
        {
            SDL_CloseAudioDevice(sdlID);
            sdlID = 0;
        }
#elif defined(FTK_SDL3)
        if (sdlStream)
        {
            SDL_DestroyAudioStream(sdlStream);
            sdlStream = nullptr;
        }
#endif // FTK_SDL2

        AudioDeviceID id = audioDevice->get();
        auto audioSystem = context->getSystem<AudioSystem>();
        auto devices = audioSystem->getDevices();
        auto i = std::find_if(
            devices.begin(),
            devices.end(),
            [id](const AudioDeviceInfo& value)
            {
                return id == value.id;
            });
        audioDevices = !devices.empty();
        audioInfo = i != devices.end() ? i->info : audioSystem->getDefaultDevice().info;
        if (audioInfo.isValid())
        {
            {
                std::stringstream ss;
                ss << "Opening audio device " << id.number << ": " << id.name << "\n" <<
                    "    * Buffer frames: " << playerOptions.audioBufferFrameCount << "\n" <<
                    "    * Channels: " << audioInfo.channelCount << "\n" <<
                    "    * Type: " << audioInfo.type << "\n" <<
                    "    * Sample rate: " << audioInfo.sampleRate;
                context->log("tl::Player", ss.str());
            }

            // These are OK to modify since the audio thread is stopped.
            audioMutex.reset = true;
            audioMutex.position = toAudioSamples(currentTime->get());
            audioMutex.loops = 0;
            audioThread.info = audioInfo;
            audioThread.resample.reset();

            SDL_AudioSpec spec;
            spec.freq = audioInfo.sampleRate;
            spec.format = toSDL(audioInfo.type);
            spec.channels = audioInfo.channelCount;
#if defined(FTK_SDL2)
            spec.samples = playerOptions.audioBufferFrameCount;
            spec.padding = 0;
            spec.callback = sdl2Callback;
            spec.userdata = this;
            SDL_AudioSpec outSpec;
            sdlID = SDL_OpenAudioDevice(
                !id.name.empty() ? id.name.c_str() : nullptr,
                0,
                &spec,
                &outSpec,
                SDL_AUDIO_ALLOW_ANY_CHANGE);
            if (sdlID > 0)
            {
                audioInfo.channelCount = outSpec.channels;
                audioInfo.type = fromSDL(outSpec.format);
                audioInfo.sampleRate = outSpec.freq;
#elif defined(FTK_SDL3)
            sdlStream = SDL_OpenAudioDeviceStream(
                -1 == id.number ? SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK : id.number,
                &spec,
                sdl3Callback,
                this);
            if (sdlStream)
            {
#endif // FTK_SDL2
                {
                    std::stringstream ss;
                    ss << "Audio device " << id.number << ": " << id.name << "\n" <<
                    "    * Channels: " << audioInfo.channelCount << "\n" <<
                    "    * Type: " << audioInfo.type << "\n" <<
                    "    * Sample rate: " << audioInfo.sampleRate;
                    context->log("tl::Player", ss.str());
                }

#if defined(FTK_SDL2)
                SDL_PauseAudioDevice(sdlID, 0);
#elif defined(FTK_SDL3)
                SDL_ResumeAudioStreamDevice(sdlStream);
#endif // FTK_SDL2
            }
            else
            {
                std::stringstream ss;
                ss << "Cannot open audio device: " << SDL_GetError();
                context->log("tl::Player", ss.str(), ftk::LogType::Error);
            }
        }
#endif // FTK_SDL2
    }

    int64_t Player::Private::toAudioSamples(const OTIO_NS::RationalTime& time) const
    {
        return sourceAudioInfo.sampleRate > 0 ?
            time.rescaled_to(sourceAudioInfo.sampleRate).floor().value() :
            0;
    }

    void Player::Private::audioReset(const OTIO_NS::RationalTime& time)
    {
        // A reset moves the position and throws away what the callback had
        // buffered ahead of it: it is for seeks and for the changes that
        // invalidate the buffer, not for the loop point, which the callback
        // now passes through without a gap.
        audioMutex.reset = true;
        audioMutex.position = toAudioSamples(time);
        audioMutex.loops = 0;
    }

#if defined(FTK_SDL2) || defined(FTK_SDL3)
    void Player::Private::sdlCallback(
        uint8_t* outputBuffer,
        int len)
    {
        // Get mutex protected values.
        AudioState state;
        bool reset = false;
        int64_t position = 0;
        {
            std::unique_lock<std::mutex> lock(audioMutex.mutex);
            state = audioMutex.state;
            reset = audioMutex.reset;
            audioMutex.reset = false;
            position = audioMutex.position;
        }

        // Zero output audio data.
        const AudioInfo& outputInfo = audioThread.info;
        const size_t outputSamples = len / outputInfo.getByteCount();
        std::memset(outputBuffer, 0, outputSamples * outputInfo.getByteCount());

        const AudioInfo& inputInfo = sourceAudioInfo;
        if (state.playback != Playback::Stop && inputInfo.sampleRate > 0)
        {
            // Initialize on reset.
            if (reset)
            {
                audioThread.position = position;
                if (audioThread.resample)
                {
                    audioThread.resample->flush();
                }
                audioThread.buffer.clear();
            }

            // Create the audio resampler.
            if (!audioThread.resample ||
                (audioThread.resample && audioThread.resample->getInputInfo() != inputInfo))
            {
                audioThread.resample = AudioResample::create(inputInfo, outputInfo);
            }

            // The in/out range in source samples. Only Loop::Loop wraps
            // here: the other loop modes stop or turn around at the
            // boundary, which is the main thread's decision, and it needs
            // to see the position run past the range to make it.
            int64_t rangeStart = 0;
            int64_t rangeEnd = 0;
            if (Loop::Loop == state.loop)
            {
                rangeStart = state.inOutRange.start_time().
                    rescaled_to(inputInfo.sampleRate).floor().value();
                rangeEnd = state.inOutRange.end_time_exclusive().
                    rescaled_to(inputInfo.sampleRate).floor().value();
            }
            const int64_t rangeSize = rangeEnd - rangeStart;
            const bool canWrap = rangeSize > 0;
            int64_t loops = 0;

            // Wrap the read position into the in/out range. Forward
            // playback reads [start, end) and lands on the end; reverse
            // reads (start, end] and lands on the start; a position
            // anywhere else is a range that changed while playing.
            const auto wrapPosition = [&]()
            {
                if (canWrap)
                {
                    const int64_t prev = audioThread.position;
                    audioThread.position = Playback::Forward == state.playback ?
                        rangeStart + (((prev - rangeStart) % rangeSize) + rangeSize) % rangeSize :
                        rangeEnd - (((rangeEnd - prev) % rangeSize) + rangeSize) % rangeSize;
                    if (audioThread.position != prev)
                    {
                        ++loops;
                    }
                }
            };

            // Fill the audio buffer.
            const double speedMult = std::max(timeRange.duration().rate() > 0.0 ? (state.speed / timeRange.duration().rate()) : 1.0, 1.0);
            const double bufferMax = outputSamples * 2 * speedMult;
            bool copied = false;
            while (getSampleCount(audioThread.buffer) < bufferMax)
            {
                const size_t bufferBefore = getSampleCount(audioThread.buffer);
                wrapPosition();

                // Get audio from the cache.
                const int64_t t = audioThread.position -
                    OTIO_NS::RationalTime(state.audioOffset, 1.0).rescaled_to(inputInfo.sampleRate).value();
                std::vector<AudioFrame> audioFrameList;
                {
                    const int64_t seconds = std::floor(t / static_cast<double>(inputInfo.sampleRate));
                    std::unique_lock<std::mutex> lock(audioMutex.mutex);
                    // Gather the buckets audioCopy may read from. Forward
                    // playback reads { seconds, seconds + 1 }; reverse reads
                    // { seconds - 1, seconds }. Supplying all three covers
                    // either direction (audioCopy ignores buckets it doesn't
                    // need), and matches the window used when filling the
                    // current-audio-frame display.
                    for (int64_t s : { seconds - 1, seconds, seconds + 1 })
                    {
                        if (const auto j = audioMutex.cache.find(s);
                            j != audioMutex.cache.end())
                        {
                            audioFrameList.push_back(j->second);
                        }
                    }
                }
                int64_t copySize = OTIO_NS::RationalTime(
                    bufferMax - static_cast<double>(getSampleCount(audioThread.buffer)),
                    outputInfo.sampleRate).
                    rescaled_to(inputInfo.sampleRate).value();

                // Stop the copy at the loop point. The wrap then lands on
                // the sample instead of in the middle of a buffer, and what
                // follows it is read on the next time around this loop.
                bool cutAtBoundary = false;
                if (canWrap)
                {
                    const int64_t boundary = Playback::Forward == state.playback ?
                        rangeEnd - audioThread.position :
                        audioThread.position - rangeStart;
                    if (boundary < copySize)
                    {
                        copySize = boundary;
                        cutAtBoundary = true;
                    }
                }

                std::vector<std::shared_ptr<Audio> > audioLayers;
                if (copySize > 0)
                {
                    audioLayers = audioCopy(
                        inputInfo,
                        audioFrameList,
                        state.playback,
                        t,
                        copySize);
                }
                if (!audioLayers.empty())
                {
                    // Mix the audio layers.
                    const auto now = std::chrono::steady_clock::now();
                    if (state.mute || now < state.muteTimeout)
                    {
                        state.volume = 0.F;
                    }
                    auto audio = mixAudio(audioLayers, state.volume, state.channelMute);

                    // Reverse the audio.
                    if (Playback::Reverse == state.playback)
                    {
                        audio = reverseAudio(audio);
                    }

                    // Change the audio speed.
                    if (state.speed != timeRange.duration().rate() && state.speed > 0.0)
                    {
                        audio = changeAudioSpeed(audio, timeRange.duration().rate() / state.speed);
                    }

                    // Resample the audio and add it to the buffer.
                    audioThread.buffer.push_back(audioThread.resample->process(audio));

                    // Advance the read position.
                    const int64_t frames = audioLayers[0]->getSampleCount();
                    audioThread.position += Playback::Forward == state.playback ? frames : -frames;
                    copied = true;
                }
                else
                {
                    // Nothing to read at this position, either because the
                    // buffer is already as full as a whole sample can make
                    // it or because the cache has not filled yet. Move the
                    // clock through a cache miss so playback carries on
                    // rather than stopping on a frame -- but only if this
                    // buffer got nothing at all, since skipping past audio
                    // that has just been read is a hole in the sound.
                    if (!copied && copySize > 0)
                    {
                        const int64_t frames = OTIO_NS::RationalTime(outputSamples, outputInfo.sampleRate).
                            rescaled_to(inputInfo.sampleRate).value();
                        audioThread.position += Playback::Forward == state.playback ? frames : -frames;
                    }
                    break;
                }

                // Fill again only to carry on past the loop point, and only
                // while the buffer is growing: playing at a speed the audio
                // has to be stretched to can turn a handful of samples into
                // none, and asking for the same handful again never ends.
                if (!cutAtBoundary || getSampleCount(audioThread.buffer) <= bufferBefore)
                {
                    break;
                }
            }
            wrapPosition();

            // Send the audio data to the device.
            const size_t bufferSampleCount = getSampleCount(audioThread.buffer);
            if (outputSamples <= bufferSampleCount)
            {
                moveAudio(audioThread.buffer, outputBuffer, outputSamples);
            }

            // Publish the playback clock.
            {
                std::unique_lock<std::mutex> lock(audioMutex.mutex);
                audioMutex.position = audioThread.position;
                audioMutex.loops += loops;
            }
        }
    }

#if defined(FTK_SDL2)
    void Player::Private::sdl2Callback(
        void* userData,
        Uint8* outputBuffer,
        int len)
    {
        auto p = reinterpret_cast<Player::Private*>(userData);
        if (len > 0)
        {
            p->sdlCallback(outputBuffer, len);
        }
    }
#elif defined(FTK_SDL3)
    void Player::Private::sdl3Callback(
        void* userData,
        SDL_AudioStream *stream,
        int additional_amount,
        int total_amount)
    {
        auto p = reinterpret_cast<Player::Private*>(userData);
        if (additional_amount > 0)
        {
            // additional_amount is already in bytes. Scaling it by the
            // frame size again fed several times too much audio per
            // callback, which ran the audio clock in leaps and throttled
            // playback to a fraction of the frame rate.
            std::vector<uint8_t> buf(additional_amount);
            p->sdlCallback(buf.data(), buf.size());
            SDL_PutAudioStreamData(stream, buf.data(), buf.size());
        }
    }
#endif // FTK_SDL2
#endif // FTK_SDL2 || FTK_SDL3
}

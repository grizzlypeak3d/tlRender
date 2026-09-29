// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/PlayerPrivate.h>

#include <tlRender/Timeline/Util.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>

#include <algorithm>
#include <cmath>

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
        // Make a buffer at least this big. The audio thread's buffers only
        // grow, so once they are as big as the blocks it is asked for, it
        // allocates nothing more.
        void growBuffer(std::vector<uint8_t>& buffer, size_t size)
        {
            if (buffer.size() < size)
            {
                buffer.resize(size);
            }
        }

        // One sample of the device's format, as a value from -1 to 1.
        double getSample(const uint8_t* p, AudioType type)
        {
            double out = 0.0;
            switch (type)
            {
            case AudioType::S8: out = *reinterpret_cast<const int8_t*>(p) / 128.0; break;
            case AudioType::S16: out = *reinterpret_cast<const int16_t*>(p) / 32768.0; break;
            case AudioType::S32: out = *reinterpret_cast<const int32_t*>(p) / 2147483648.0; break;
            case AudioType::F32: out = *reinterpret_cast<const float*>(p); break;
            case AudioType::F64: out = *reinterpret_cast<const double*>(p); break;
            default: break;
            }
            return out;
        }

        void setSample(uint8_t* p, AudioType type, double value)
        {
            const double v = std::clamp(value, -1.0, 1.0);
            switch (type)
            {
            case AudioType::S8:
                *reinterpret_cast<int8_t*>(p) = static_cast<int8_t>(
                    std::clamp(std::lround(v * 128.0), -128L, 127L));
                break;
            case AudioType::S16:
                *reinterpret_cast<int16_t*>(p) = static_cast<int16_t>(
                    std::clamp(std::lround(v * 32768.0), -32768L, 32767L));
                break;
            case AudioType::S32:
                *reinterpret_cast<int32_t*>(p) = static_cast<int32_t>(
                    std::clamp(std::llround(v * 2147483648.0), -2147483648LL, 2147483647LL));
                break;
            case AudioType::F32: *reinterpret_cast<float*>(p) = static_cast<float>(v); break;
            case AudioType::F64: *reinterpret_cast<double*>(p) = v; break;
            default: break;
            }
        }

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
            audioMutex.positionDuration = 0.0;
            ++audioMutex.generation;
            audioThread.info = audioInfo;
            audioThread.resample.reset();
            audioThread.fifoByteCount = 0;
            audioThread.lastFrame.clear();

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
        audioMutex.positionDuration = 0.0;
        ++audioMutex.generation;
    }

    int64_t Player::Private::audioRead(
        const AudioInfo& info,
        Playback playback,
        int64_t frame,
        int64_t size)
    {
        // Copied out under the lock rather than by holding on to the cache
        // entries: the cache thread can drop one at any time, and then the
        // last hold on it, and the freeing of a second of audio, would be
        // this thread's.
        if (Playback::Reverse == playback)
        {
            frame -= size;
        }
        const int64_t seconds = std::floor(frame / static_cast<double>(info.sampleRate));
        const int64_t offset = frame - seconds * info.sampleRate;
        const size_t byteCount = info.getByteCount();
        std::unique_lock<std::mutex> lock(audioMutex.mutex);
        const auto first = audioMutex.cache.find(seconds);
        if (first == audioMutex.cache.end() || first->second.layers.empty())
        {
            return -1;
        }
        const auto second = audioMutex.cache.find(seconds + 1);
        const AudioFrame& a = first->second;
        const AudioFrame* b = second != audioMutex.cache.end() ? &second->second : nullptr;

        // Bound the copy by the samples actually available: what remains in
        // the first second after the offset, and the next second if it is
        // there. Only the first layer's lengths are counted, as audioCopy()
        // does.
        const int64_t firstCount = a.layers[0].audio ?
            static_cast<int64_t>(a.layers[0].audio->getSampleCount()) : 0;
        const int64_t avail0 = std::max<int64_t>(0, firstCount - offset);
        const int64_t avail1 = b && !b->layers.empty() && b->layers[0].audio ?
            static_cast<int64_t>(b->layers[0].audio->getSampleCount()) : 0;
        const int64_t outSize = std::min(size, avail0 + avail1);

        const size_t layerCount = a.layers.size();
        if (audioThread.layers.size() < layerCount)
        {
            audioThread.layers.resize(layerCount);
        }
        audioThread.layerData.resize(layerCount);
        for (size_t i = 0; i < layerCount; ++i)
        {
            growBuffer(audioThread.layers[i], std::max<int64_t>(outSize, 1) * byteCount);
            std::memset(audioThread.layers[i].data(), 0, outSize * byteCount);
            audioThread.layerData[i] = audioThread.layers[i].data();
        }

        const int64_t sizeTmp = std::min(outSize, avail0);
        for (size_t i = 0; i < layerCount; ++i)
        {
            const auto& audio = a.layers[i].audio;
            if (audio && audio->getInfo() == info)
            {
                const int64_t n = std::min(
                    sizeTmp,
                    static_cast<int64_t>(audio->getSampleCount()) - offset);
                if (n > 0)
                {
                    std::memcpy(
                        audioThread.layers[i].data(),
                        audio->getData() + offset * byteCount,
                        n * byteCount);
                }
            }
        }
        if (sizeTmp < outSize && b)
        {
            for (size_t i = 0; i < layerCount && i < b->layers.size(); ++i)
            {
                const auto& audio = b->layers[i].audio;
                if (audio && audio->getInfo() == info)
                {
                    const int64_t n = std::min(
                        outSize - sizeTmp,
                        static_cast<int64_t>(audio->getSampleCount()));
                    if (n > 0)
                    {
                        std::memcpy(
                            audioThread.layers[i].data() + sizeTmp * byteCount,
                            audio->getData(),
                            n * byteCount);
                    }
                }
            }
        }
        return outSize;
    }

    // Smooth over a step in what the device is given. Called with every
    // block, whether it holds audio or silence: a block of silence after
    // audio ramps down from where the audio left off, and the first block
    // of audio after a reset or a silence ramps from where the device is.
    void Player::Private::declick(
        uint8_t* data,
        size_t sampleCount,
        const AudioInfo& info,
        bool audio)
    {
        auto& thread = audioThread;
        const size_t channelCount = info.channelCount;
        const size_t sampleByteCount = getByteCount(info.type);
        const size_t frameByteCount = channelCount * sampleByteCount;
        if (0 == sampleCount || 0 == frameByteCount)
        {
            return;
        }
        if (thread.lastFrame.size() != channelCount)
        {
            thread.lastFrame.assign(channelCount, 0.0);
            thread.declickStep.assign(channelCount, 0.0);
            thread.declickPos = thread.declickLength = 0;
        }

        if (!audio || thread.declickPending || thread.silent)
        {
            // Starting over from wherever an earlier ramp had got to is
            // still continuous: the step is taken from what the device
            // was last given, ramp included.
            bool step = false;
            for (size_t c = 0; c < channelCount; ++c)
            {
                thread.declickStep[c] =
                    thread.lastFrame[c] - getSample(data + c * sampleByteCount, info.type);
                step |= thread.declickStep[c] != 0.0;
            }
            if (step)
            {
                thread.declickPos = 0;
                thread.declickLength = std::max(
                    static_cast<size_t>(info.sampleRate * .005), static_cast<size_t>(1));
            }
            if (audio)
            {
                thread.declickPending = false;
            }
        }
        thread.silent = !audio;

        for (size_t i = 0;
            i < sampleCount && thread.declickPos < thread.declickLength;
            ++i, ++thread.declickPos)
        {
            const double k = 1.0 - thread.declickPos / static_cast<double>(thread.declickLength);
            for (size_t c = 0; c < channelCount; ++c)
            {
                uint8_t* p = data + i * frameByteCount + c * sampleByteCount;
                setSample(p, info.type, getSample(p, info.type) + thread.declickStep[c] * k);
            }
        }

        const uint8_t* last = data + (sampleCount - 1) * frameByteCount;
        for (size_t c = 0; c < channelCount; ++c)
        {
            thread.lastFrame[c] = getSample(last + c * sampleByteCount, info.type);
        }
    }

#if defined(FTK_SDL2) || defined(FTK_SDL3)
    void Player::Private::sdlCallback(
        uint8_t* outputBuffer,
        int len)
    {
        // Get mutex protected values.
        AudioState& state = audioThread.state;
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
        bool audio = false;
        if (state.playback != Playback::Stop && inputInfo.sampleRate > 0)
        {
            // Initialize on reset.
            if (reset)
            {
                audioThread.declickPending = true;
                audioThread.position = position;
                if (audioThread.resample)
                {
                    audioThread.resample->flush();
                }
                audioThread.fifoByteCount = 0;
            }

            // Create the audio resampler. Playing at another speed plays
            // the audio faster or slower, pitch and all, which is a change
            // of sample rate: the resampler takes the source as running at
            // its rate times the speed. It filters, and it carries its state
            // from one buffer to the next, where changing the speed of each
            // buffer on its own crackled -- a nearest sample repeated or
            // skipped every few samples, and a step at every buffer's end.
            AudioInfo resampleInfo = inputInfo;
            const double timelineRate = timeRange.duration().rate();
            if (state.speed > 0.0 && timelineRate > 0.0 && state.speed != timelineRate)
            {
                resampleInfo.sampleRate = std::max(1, static_cast<int>(std::lround(
                    inputInfo.sampleRate * state.speed / timelineRate)));
            }
            if (!audioThread.resample ||
                (audioThread.resample && audioThread.resample->getInputInfo() != resampleInfo))
            {
                audioThread.resample = AudioResample::create(resampleInfo, outputInfo);
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
            const size_t outputByteCount = outputInfo.getByteCount();
            const auto fifoSampleCount = [&]()
            {
                return audioThread.fifoByteCount / outputByteCount;
            };
            const double speedMult = std::max(timeRange.duration().rate() > 0.0 ? (state.speed / timeRange.duration().rate()) : 1.0, 1.0);
            const double bufferMax = outputSamples * 2 * speedMult;
            bool copied = false;
            while (fifoSampleCount() < bufferMax)
            {
                const size_t bufferBefore = fifoSampleCount();
                wrapPosition();

                // Get audio from the cache.
                const int64_t t = audioThread.position -
                    OTIO_NS::RationalTime(state.audioOffset, 1.0).rescaled_to(inputInfo.sampleRate).value();
                int64_t copySize = OTIO_NS::RationalTime(
                    bufferMax - static_cast<double>(fifoSampleCount()),
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

                const int64_t frames = copySize > 0 ?
                    audioRead(inputInfo, state.playback, t, copySize) :
                    -1;
                if (frames >= 0)
                {
                    // Mix the audio layers.
                    float volume = state.volume;
                    const auto now = std::chrono::steady_clock::now();
                    if (state.mute || now < state.muteTimeout)
                    {
                        volume = 0.F;
                    }
                    const size_t inputByteCount = inputInfo.getByteCount();
                    const size_t byteCount = frames * inputByteCount;
                    growBuffer(audioThread.mix, byteCount);
                    audioThread.channelVolumes.resize(inputInfo.channelCount);
                    for (size_t c = 0; c < audioThread.channelVolumes.size(); ++c)
                    {
                        audioThread.channelVolumes[c] =
                            c < state.channelMute.size() && state.channelMute[c] ?
                            0.F :
                            volume;
                    }
                    mixAudio(
                        audioThread.layerData.data(),
                        audioThread.layerData.size(),
                        audioThread.mix.data(),
                        audioThread.channelVolumes.data(),
                        inputInfo,
                        frames);
                    const uint8_t* audio = audioThread.mix.data();

                    // Reverse the audio.
                    if (Playback::Reverse == state.playback)
                    {
                        growBuffer(audioThread.reverse, byteCount);
                        reverseAudio(audio, audioThread.reverse.data(), inputInfo, frames);
                        audio = audioThread.reverse.data();
                    }

                    // Resample the audio, to the device's rate and the
                    // playback speed, and add it to the buffer.
                    const size_t resampleMax = audioThread.resample->getOutputSampleCountMax(frames);
                    growBuffer(audioThread.fifo, audioThread.fifoByteCount + resampleMax * outputByteCount);
                    audioThread.fifoByteCount += outputByteCount * audioThread.resample->process(
                        audio,
                        frames,
                        audioThread.fifo.data() + audioThread.fifoByteCount,
                        resampleMax);

                    // Advance the read position.
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
                if (!cutAtBoundary || fifoSampleCount() <= bufferBefore)
                {
                    break;
                }
            }
            wrapPosition();

            // Send the audio data to the device.
            if (outputSamples <= fifoSampleCount())
            {
                const size_t byteCount = outputSamples * outputByteCount;
                std::memcpy(outputBuffer, audioThread.fifo.data(), byteCount);
                audioThread.fifoByteCount -= byteCount;
                std::memmove(
                    audioThread.fifo.data(),
                    audioThread.fifo.data() + byteCount,
                    audioThread.fifoByteCount);
                audio = true;
            }

            // Publish the playback clock, and what the main thread needs
            // to move it on until the next callback: the block just handed
            // over plays for its length, and the position moves through the
            // source at the playback speed.
            const double rate = timeRange.duration().rate();
            const double speedRatio = rate > 0.0 && state.speed > 0.0 ?
                (state.speed / rate) :
                1.0;
            {
                std::unique_lock<std::mutex> lock(audioMutex.mutex);
                audioMutex.position = audioThread.position;
                audioMutex.loops += loops;
                audioMutex.positionTime = std::chrono::steady_clock::now();
                audioMutex.positionDuration = outputInfo.sampleRate > 0 ?
                    (outputSamples / static_cast<double>(outputInfo.sampleRate)) :
                    0.0;
                audioMutex.positionRate =
                    inputInfo.sampleRate * speedRatio *
                    (Playback::Forward == state.playback ? 1.0 : -1.0);
            }
        }

        declick(outputBuffer, outputSamples, outputInfo, audio);
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
            growBuffer(p->audioThread.sdlBuffer, additional_amount);
            p->sdlCallback(p->audioThread.sdlBuffer.data(), additional_amount);
            SDL_PutAudioStreamData(stream, p->audioThread.sdlBuffer.data(), additional_amount);
        }
    }
#endif // FTK_SDL2
#endif // FTK_SDL2 || FTK_SDL3
}

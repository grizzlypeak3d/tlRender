// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/PlayerAudioTest.h>

#include <tlRender/Timeline/AudioSystem.h>
#include <tlRender/Timeline/Player.h>
#include <tlRender/Timeline/Timeline.h>

#include <tlRender/IO/System.h>

#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/Time.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace tl
{
    namespace timeline_tests
    {
        namespace
        {
            // A tone that stays in phase across a seek to a whole second:
            // 480 cycles a second, 20 to a frame at 24 frames a second.
            const double toneFrequency = 480.0;
            const double toneAmplitude = .25;
            const int toneSampleRate = 48000;
            const int toneSeconds = 10;
            const double pi = 3.14159265358979323846;

            void writeTone(const std::filesystem::path& path)
            {
                const int channels = 2;
                const int bytes = 2;
                const uint32_t sampleCount = toneSampleRate * toneSeconds;
                const uint32_t dataSize = sampleCount * channels * bytes;
                std::ofstream f(path, std::ios::binary);
                const auto u32 = [&f](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
                const auto u16 = [&f](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
                f.write("RIFF", 4);
                u32(36 + dataSize);
                f.write("WAVEfmt ", 8);
                u32(16);
                u16(1);
                u16(channels);
                u32(toneSampleRate);
                u32(toneSampleRate * channels * bytes);
                u16(channels * bytes);
                u16(bytes * 8);
                f.write("data", 4);
                u32(dataSize);
                for (uint32_t i = 0; i < sampleCount; ++i)
                {
                    const int16_t v = static_cast<int16_t>(std::lround(
                        toneAmplitude * 32767.0 *
                        std::sin(2.0 * pi * toneFrequency * i / toneSampleRate)));
                    for (int c = 0; c < channels; ++c)
                    {
                        f.write(reinterpret_cast<const char*>(&v), 2);
                    }
                }
            }
        }

        PlayerAudioTest::PlayerAudioTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::PlayerAudioTest")
        {}

        std::shared_ptr<PlayerAudioTest> PlayerAudioTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<PlayerAudioTest>(new PlayerAudioTest(context));
        }

        void PlayerAudioTest::run()
        {
            _tone();
        }

        void PlayerAudioTest::_tone()
        {
#if defined(_WIN32)
            // SDL opens the output file on Windows without sharing it, and the
            // audio system keeps the device open, so it cannot be read here.
            _print("Skipped: the output cannot be read while SDL has it open");
            return;
#endif // _WIN32
            auto audioSystem = _context->getSystem<AudioSystem>();
            const char* outputEnv = std::getenv("SDL_AUDIO_DISK_OUTPUT_FILE");
            if (!audioSystem || audioSystem->getCurrentDriver() != "disk" || !outputEnv)
            {
                _print("Skipped: needs SDL_AUDIO_DRIVER=disk and SDL_AUDIO_DISK_OUTPUT_FILE");
                return;
            }
            const std::filesystem::path output(outputEnv);
            const AudioInfo deviceInfo = audioSystem->getDefaultDevice().info;
            // The audio system and the player open the device as floating
            // point whatever it says; see openKeepalive().
            const size_t channels = deviceInfo.channelCount;
            const double rate = deviceInfo.sampleRate;
            _print(ftk::Format("Device: {0} channels, {1}Hz").arg(channels).arg(rate));
            FTK_CHECK(channels > 0 && rate > 0);

            const std::filesystem::path wav = _getTempDir() / "PlayerAudioTest.wav";
            const ftk::Path wavPath(wav.string());
            if (!_context->getSystem<ReadSystem>()->getPlugin(wavPath))
            {
                _print("Skipped: no WAV reader");
                return;
            }
            writeTone(wav);
            auto timeline = Timeline::create(_context, wavPath);
            auto player = Player::create(_context, timeline);
            FTK_CHECK(player->getIOInfo().audio.isValid());

            const auto wait = [this](double seconds)
            {
                const auto t0 = std::chrono::steady_clock::now();
                while (std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() < seconds)
                {
                    _context->tick();
                    ftk::sleep(std::chrono::milliseconds(5));
                }
            };

            // Let the audio cache fill, so what follows is not the silence of
            // a cache miss.
            const OTIO_NS::TimeRange& timeRange = player->getTimeRange();
            for (int i = 0; i < 200; ++i)
            {
                wait(.05);
                double cached = 0.0;
                for (const auto& range : player->observeCacheInfo()->get().audio)
                {
                    cached += range.duration().rescaled_to(1.0).value();
                }
                if (cached >= toneSeconds - 1)
                {
                    break;
                }
            }

            // Play with seeks to whole seconds, then stop.
            const size_t begin = std::filesystem::file_size(output);
            player->forward();
            wait(1.5);
            player->seek(timeRange.start_time() + OTIO_NS::RationalTime(5.0, 1.0));
            wait(1.5);
            player->seek(timeRange.start_time() + OTIO_NS::RationalTime(2.0, 1.0));
            wait(1.5);
            player->stop();
            wait(.5);
            const size_t seekCount = 2;
            const size_t end = std::filesystem::file_size(output);

            // What the device was given, the first channel.
            const size_t frameBytes = channels * sizeof(float);
            const size_t frames = (end - begin) / frameBytes;
            std::vector<float> data(frames * channels);
            {
                std::ifstream f(output, std::ios::binary);
                f.seekg(begin);
                f.read(reinterpret_cast<char*>(data.data()), frames * frameBytes);
            }
            std::vector<double> x(frames);
            for (size_t i = 0; i < frames; ++i)
            {
                x[i] = data[i * channels];
            }

            // Where the tone is.
            size_t first = 0;
            while (first < frames && std::fabs(x[first]) < 1.0e-4)
            {
                ++first;
            }
            size_t last = frames;
            while (last > first && std::fabs(x[last - 1]) < 1.0e-4)
            {
                --last;
            }
            const double seconds = (last - first) / rate;
            _print(ftk::Format("Tone: {0} seconds").arg(seconds, 2));
            FTK_CHECK(seconds > 3.0 && seconds < 6.0);

            // No dropouts: a sine is only this close to zero for a sample or
            // two at a time.
            size_t dropouts = 0;
            size_t run = 0;
            for (size_t i = first; i < last; ++i)
            {
                run = std::fabs(x[i]) < 1.0e-6 ? run + 1 : 0;
                if (32 == run)
                {
                    ++dropouts;
                }
            }
            _print(ftk::Format("Dropouts: {0}").arg(dropouts));
            FTK_CHECK(0 == dropouts);

            // No clicks: nothing steeper than the tone itself, which a seek or
            // a stop without the declick is.
            const double w = 2.0 * pi * toneFrequency / rate;
            double stepMax = 0.0;
            for (size_t i = first + 1; i < last; ++i)
            {
                stepMax = std::max(stepMax, std::fabs(x[i] - x[i - 1]));
            }
            _print(ftk::Format("Largest step: {0}, the tone's: {1}").
                arg(stepMax, 4).arg(toneAmplitude * w, 4));
            FTK_CHECK(stepMax < 1.5 * toneAmplitude * w);

            // Breaks in the tone only where it was sought: a sine keeps
            // x[n-1] + x[n+1] = 2 cos(w) x[n], and a seek to another phase
            // bends it for as long as the declick lasts. The stop at the end
            // is left out.
            const double c = 2.0 * std::cos(w);
            const size_t tail = static_cast<size_t>(.02 * rate);
            const size_t gap = static_cast<size_t>(.01 * rate);
            size_t clusters = 0;
            size_t clusterStart = 0;
            size_t lastBreak = 0;
            size_t longest = 0;
            bool inCluster = false;
            for (size_t i = first + 1; i + 1 + tail < last; ++i)
            {
                if (std::fabs(x[i - 1] + x[i + 1] - c * x[i]) > .01 * toneAmplitude)
                {
                    if (!inCluster || i - lastBreak > gap)
                    {
                        if (inCluster)
                        {
                            longest = std::max(longest, lastBreak - clusterStart);
                        }
                        ++clusters;
                        clusterStart = i;
                        inCluster = true;
                    }
                    lastBreak = i;
                }
            }
            if (inCluster)
            {
                longest = std::max(longest, lastBreak - clusterStart);
            }
            _print(ftk::Format("Breaks: {0}, the longest {1}ms").
                arg(clusters).arg(longest / rate * 1000.0, 1));
            FTK_CHECK(clusters <= seekCount);
            FTK_CHECK(longest / rate < .01);
        }
    }
}

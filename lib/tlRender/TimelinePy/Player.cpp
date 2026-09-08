// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/Player.h>

#include <ftk/CorePy/Bindings.h>
#include <ftk/Core/Context.h>

#include <tlRender/TimelinePy/OTIOCasters.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/list.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/set.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/function.h>
#include <nanobind/operators.h>

namespace nb = nanobind;

namespace tl
{
    namespace python
    {
        void player(nb::module_& m)
        {
            nb::class_<PlayerCacheInfo>(m, "PlayerCacheInfo")
                .def(nb::init())
                .def_rw("videoPercentage", &PlayerCacheInfo::videoPercentage)
                .def_rw("audioPercentage", &PlayerCacheInfo::audioPercentage)
                .def_rw("video", &PlayerCacheInfo::video)
                .def_rw("audio", &PlayerCacheInfo::audio)
                .def(nanobind::self == nanobind::self)
                .def(nanobind::self != nanobind::self);

            nb::enum_<Playback>(m, "Playback")
                .value("Stop", Playback::Stop)
                .value("Forward", Playback::Forward)
                .value("Reverse", Playback::Reverse);
            FTK_ENUM_BIND(m, Playback);

            nb::enum_<Loop>(m, "Loop")
                .value("Loop", Loop::Loop)
                .value("Once", Loop::Once)
                .value("PingPong", Loop::PingPong);
            FTK_ENUM_BIND(m, Loop);

            nb::enum_<TimeAction>(m, "TimeAction")
                .value("Start", TimeAction::Start)
                .value("End", TimeAction::End)
                .value("FramePrev", TimeAction::FramePrev)
                .value("FramePrevX10", TimeAction::FramePrevX10)
                .value("FramePrevX100", TimeAction::FramePrevX100)
                .value("FrameNext", TimeAction::FrameNext)
                .value("FrameNextX10", TimeAction::FrameNextX10)
                .value("FrameNextX100", TimeAction::FrameNextX100)
                .value("JumpBack1s", TimeAction::JumpBack1s)
                .value("JumpBack10s", TimeAction::JumpBack10s)
                .value("JumpForward1s", TimeAction::JumpForward1s)
                .value("JumpForward10s", TimeAction::JumpForward10s);
            FTK_ENUM_BIND(m, TimeAction);

            ftk::python::observable<Playback>(m, "Playback");
            ftk::python::observable<Loop>(m, "Loop");
            ftk::python::observable<PlayerCacheOptions>(m, "PlayerCacheOptions");
            ftk::python::observable<PlayerCacheInfo>(m, "PlayerCacheInfo");
            ftk::python::observable<OTIO_NS::RationalTime>(m, "RationalTime");
            ftk::python::observable<OTIO_NS::TimeRange>(m, "TimeRange");
            ftk::python::observable<std::shared_ptr<Player> >(m, "Player");

            nb::class_<Player>(
                m, "Player",
                // The Python examples hold this through weakref, which
                // nanobind classes opt into.
                nb::is_weak_referenceable())
                .def(
                    nb::new_(nb::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<Timeline>&,
                        const PlayerOptions&>(&Player::create)),
                    nb::arg("context"),
                    nb::arg("timeline"),
                    nb::arg("options") = PlayerOptions())

                .def_prop_ro("context", &Player::getContext)
                .def_prop_ro("timeline", &Player::getTimeline)
                .def_prop_ro("path", &Player::getPath, nb::rv_policy::copy)
                .def_prop_ro("audioPath", &Player::getAudioPath, nb::rv_policy::copy)
                .def_prop_ro("playerOptions", &Player::getPlayerOptions, nb::rv_policy::copy)
                .def_prop_ro("options", &Player::getOptions, nb::rv_policy::copy)
                .def_prop_ro("timeRange", &Player::getTimeRange, nb::rv_policy::copy)
                .def_prop_ro("duration", &Player::getDuration)
                .def_prop_ro("ioInfo", &Player::getIOInfo, nb::rv_policy::copy)

                .def_prop_ro("defaultSpeed", &Player::getDefaultSpeed)
                .def_prop_rw("speed", &Player::getSpeed, &Player::setSpeed)
                .def_prop_ro("observeSpeed", &Player::observeSpeed)
                .def_prop_ro("observeActualSpeed", &Player::observeActualSpeed)
                .def_prop_rw("speedMult", &Player::getSpeedMult, &Player::setSpeedMult)
                .def_prop_ro("observeSpeedMult", &Player::observeSpeedMult)
                .def_prop_rw("playback", &Player::getPlayback, &Player::setPlayback)
                .def_prop_ro("observePlayback", &Player::observePlayback)
                .def("togglePlayback", &Player::togglePlayback)
                .def_prop_ro("isStopped", &Player::isStopped)
                .def("stop", &Player::stop)
                .def("forward", &Player::forward)
                .def("reverse", &Player::reverse)
                .def_prop_rw("loop", &Player::getLoop, &Player::setLoop)
                .def_prop_ro("observeLoop", &Player::observeLoop)

                .def_prop_rw("currentTime", &Player::getCurrentTime, &Player::seek, nb::rv_policy::copy)
                .def_prop_ro("observeCurrentTime", &Player::observeCurrentTime)
                .def_prop_ro("observeSeek", &Player::observeSeek)
                .def("timeAction", &Player::timeAction)
                .def("gotoStart", &Player::gotoStart)
                .def("gotoEnd", &Player::gotoEnd)
                .def("framePrev", &Player::framePrev)
                .def("frameNext", &Player::frameNext)

                .def_prop_rw("inOutRange", &Player::getInOutRange, &Player::setInOutRange, nb::rv_policy::copy)
                .def_prop_ro("observeInOutRange", &Player::observeInOutRange)
                .def("setInPoint", &Player::setInPoint)
                .def("resetInPoint", &Player::resetInPoint)
                .def("setOutPoint", &Player::setOutPoint)
                .def("resetOutPoint", &Player::resetOutPoint)

                .def_prop_rw("compare", &Player::getCompare, &Player::setCompare)
                .def_prop_ro("observeCompare", &Player::observeCompare)
                .def_prop_rw("compareTime", &Player::getCompareTime, &Player::setCompareTime)
                .def_prop_ro("observeCompareTime", &Player::observeCompareTime)

                .def_prop_rw("ioOptions", &Player::getIOOptions, &Player::setIOOptions)
                .def_prop_ro("observeIOOptions", &Player::observeIOOptions)

                .def_prop_rw("mediaReferenceKey", &Player::getMediaReferenceKey, &Player::setMediaReferenceKey, nb::rv_policy::copy)
                .def_prop_ro("observeMediaReferenceKey", &Player::observeMediaReferenceKey)
                .def_prop_ro("mediaReferenceKeys", &Player::getMediaReferenceKeys)

                .def_prop_rw("videoLayer", &Player::getVideoLayer, &Player::setVideoLayer)
                .def_prop_ro("observeVideoLayer", &Player::observeVideoLayer)
                .def_prop_rw("compareVideoLayers", &Player::getCompareVideoLayers, &Player::setCompareVideoLayers)
                .def_prop_ro("observeCompareVideoLayers", &Player::observeCompareVideoLayers)
                .def_prop_ro("currentVideo", &Player::getCurrentVideo, nb::rv_policy::copy)
                .def_prop_ro("observeCurrentVideo", &Player::observeCurrentVideo)

                .def_prop_rw("audioDevice", &Player::getAudioDevice, &Player::setAudioDevice, nb::rv_policy::copy)
                .def_prop_ro("observeAudioDevice", &Player::observeAudioDevice)
                .def_prop_rw("volume", &Player::getVolume, &Player::setVolume)
                .def_prop_ro("observeVolume", &Player::observeVolume)
                .def_prop_rw("mute", &Player::isMuted, &Player::setMute)
                .def_prop_ro("observeMute", &Player::observeMute)
                .def_prop_rw("channelMute", &Player::getChannelMute, &Player::setChannelMute)
                .def_prop_ro("observeChannelMute", &Player::observeChannelMute)
                .def_prop_rw("audioOffset", &Player::getAudioOffset, &Player::setAudioOffset)
                .def_prop_ro("observeAudioOffset", &Player::observeAudioOffset)
                .def_prop_ro("getCurrentAudio", &Player::getCurrentAudio)
                .def_prop_ro("observeCurrentAudio", &Player::observeCurrentAudio)

                .def_prop_rw("cacheOptions", &Player::getCacheOptions, &Player::setCacheOptions, nb::rv_policy::copy)
                .def_prop_ro("observeCacheOptions", &Player::observeCacheOptions)
                .def_prop_ro("observeCacheInfo", &Player::observeCacheInfo)
                .def("clearCache", &Player::clearCache);
        }
    }
}


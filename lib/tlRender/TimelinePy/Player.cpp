// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelinePy/Bindings.h>

#include <tlRender/Timeline/Player.h>

#include <ftk/CorePy/Bindings.h>
#include <ftk/Core/Context.h>

#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/operators.h>

namespace py = pybind11;

namespace tl
{
    namespace python
    {
        void player(py::module_& m)
        {
            py::class_<PlayerCacheInfo>(m, "PlayerCacheInfo")
                .def(py::init())
                .def_readwrite("videoPercentage", &PlayerCacheInfo::videoPercentage)
                .def_readwrite("audioPercentage", &PlayerCacheInfo::audioPercentage)
                .def_readwrite("video", &PlayerCacheInfo::video)
                .def_readwrite("audio", &PlayerCacheInfo::audio)
                .def(pybind11::self == pybind11::self)
                .def(pybind11::self != pybind11::self);

            py::enum_<Playback>(m, "Playback")
                .value("Stop", Playback::Stop)
                .value("Forward", Playback::Forward)
                .value("Reverse", Playback::Reverse);
            FTK_ENUM_BIND(m, Playback);

            py::enum_<Loop>(m, "Loop")
                .value("Loop", Loop::Loop)
                .value("Once", Loop::Once)
                .value("PingPong", Loop::PingPong);
            FTK_ENUM_BIND(m, Loop);

            py::enum_<TimeAction>(m, "TimeAction")
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

            py::class_<Player, std::shared_ptr<Player> >(m, "Player")
                .def(
                    py::init(py::overload_cast<
                        const std::shared_ptr<ftk::Context>&,
                        const std::shared_ptr<Timeline>&,
                        const PlayerOptions&>(&Player::create)),
                    py::arg("context"),
                    py::arg("timeline"),
                    py::arg("options") = PlayerOptions())

                .def_property_readonly("context", &Player::getContext)
                .def_property_readonly("timeline", &Player::getTimeline)
                .def_property_readonly("path", &Player::getPath, py::return_value_policy::copy)
                .def_property_readonly("audioPath", &Player::getAudioPath, py::return_value_policy::copy)
                .def_property_readonly("playerOptions", &Player::getPlayerOptions, py::return_value_policy::copy)
                .def_property_readonly("options", &Player::getOptions, py::return_value_policy::copy)
                .def_property_readonly("timeRange", &Player::getTimeRange, py::return_value_policy::copy)
                .def_property_readonly("duration", &Player::getDuration)
                .def_property_readonly("ioInfo", &Player::getIOInfo, py::return_value_policy::copy)

                .def_property_readonly("defaultSpeed", &Player::getDefaultSpeed)
                .def_property("speed", &Player::getSpeed, &Player::setSpeed)
                .def_property_readonly("observeSpeed", &Player::observeSpeed)
                .def_property_readonly("observeActualSpeed", &Player::observeActualSpeed)
                .def_property("speedMult", &Player::getSpeedMult, &Player::setSpeedMult)
                .def_property_readonly("observeSpeedMult", &Player::observeSpeedMult)
                .def_property("playback", &Player::getPlayback, &Player::setPlayback)
                .def_property_readonly("observePlayback", &Player::observePlayback)
                .def("togglePlayback", &Player::togglePlayback)
                .def_property_readonly("isStopped", &Player::isStopped)
                .def("stop", &Player::stop)
                .def("forward", &Player::forward)
                .def("reverse", &Player::reverse)
                .def_property("loop", &Player::getLoop, &Player::setLoop)
                .def_property_readonly("observeLoop", &Player::observeLoop)

                .def_property("currentTime", &Player::getCurrentTime, &Player::seek, py::return_value_policy::copy)
                .def_property_readonly("observeCurrentTime", &Player::observeCurrentTime)
                .def_property_readonly("observeSeek", &Player::observeSeek)
                .def("timeAction", &Player::timeAction)
                .def("gotoStart", &Player::gotoStart)
                .def("gotoEnd", &Player::gotoEnd)
                .def("framePrev", &Player::framePrev)
                .def("frameNext", &Player::frameNext)

                .def_property("inOutRange", &Player::getInOutRange, &Player::setInOutRange, py::return_value_policy::copy)
                .def_property_readonly("observeInOutRange", &Player::observeInOutRange)
                .def("setInPoint", &Player::setInPoint)
                .def("resetInPoint", &Player::resetInPoint)
                .def("setOutPoint", &Player::setOutPoint)
                .def("resetOutPoint", &Player::resetOutPoint)

                .def_property("compare", &Player::getCompare, &Player::setCompare)
                .def_property_readonly("observeCompare", &Player::observeCompare)
                .def_property("compareTime", &Player::getCompareTime, &Player::setCompareTime)
                .def_property_readonly("observeCompareTime", &Player::observeCompareTime)

                .def_property("ioOptions", &Player::getIOOptions, &Player::setIOOptions)
                .def_property_readonly("observeIOOptions", &Player::observeIOOptions)

                .def_property("mediaReferenceKey", &Player::getMediaReferenceKey, &Player::setMediaReferenceKey, py::return_value_policy::copy)
                .def_property_readonly("observeMediaReferenceKey", &Player::observeMediaReferenceKey)
                .def_property_readonly("mediaReferenceKeys", &Player::getMediaReferenceKeys)

                .def_property("videoLayer", &Player::getVideoLayer, &Player::setVideoLayer)
                .def_property_readonly("observeVideoLayer", &Player::observeVideoLayer)
                .def_property("compareVideoLayers", &Player::getCompareVideoLayers, &Player::setCompareVideoLayers)
                .def_property_readonly("observeCompareVideoLayers", &Player::observeCompareVideoLayers)
                .def_property_readonly("currentVideo", &Player::getCurrentVideo, py::return_value_policy::copy)
                .def_property_readonly("observeCurrentVideo", &Player::observeCurrentVideo)

                .def_property("audioDevice", &Player::getAudioDevice, &Player::setAudioDevice, py::return_value_policy::copy)
                .def_property_readonly("observeAudioDevice", &Player::observeAudioDevice)
                .def_property("volume", &Player::getVolume, &Player::setVolume)
                .def_property_readonly("observeVolume", &Player::observeVolume)
                .def_property("mute", &Player::isMuted, &Player::setMute)
                .def_property_readonly("observeMute", &Player::observeMute)
                .def_property("channelMute", &Player::getChannelMute, &Player::setChannelMute)
                .def_property_readonly("observeChannelMute", &Player::observeChannelMute)
                .def_property("audioOffset", &Player::getAudioOffset, &Player::setAudioOffset)
                .def_property_readonly("observeAudioOffset", &Player::observeAudioOffset)
                .def_property_readonly("getCurrentAudio", &Player::getCurrentAudio)
                .def_property_readonly("observeCurrentAudio", &Player::observeCurrentAudio)

                .def_property("cacheOptions", &Player::getCacheOptions, &Player::setCacheOptions, py::return_value_policy::copy)
                .def_property_readonly("observeCacheOptions", &Player::observeCacheOptions)
                .def_property_readonly("observeCacheInfo", &Player::observeCacheInfo)
                .def("clearCache", &Player::clearCache);
        }
    }
}


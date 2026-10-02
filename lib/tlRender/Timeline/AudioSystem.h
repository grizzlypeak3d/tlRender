// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/Core/Audio.h>

#include <ftk/Core/ISystem.h>
#include <ftk/Core/ObservableList.h>
#include <ftk/Core/Observable.h>

namespace tl
{
    //! Audio device ID.
    struct TL_TIMELINE_API_TYPE AudioDeviceID
    {
        int         number = -1;
        std::string name;

        bool operator == (const AudioDeviceID&) const = default;
    };

    //! Audio device information.
    struct TL_TIMELINE_API_TYPE AudioDeviceInfo
    {
        AudioDeviceID id;
        AudioInfo     info;

        bool operator == (const AudioDeviceInfo&) const = default;
    };

    //! Audio system.
    class TL_TIMELINE_API_TYPE AudioSystem : public ftk::ISystem
    {
        FTK_NON_COPYABLE(AudioSystem);

    protected:
        AudioSystem(const std::shared_ptr<ftk::Context>&);

    public:
        TL_TIMELINE_API virtual ~AudioSystem();

        //! Create a new system.
        TL_TIMELINE_API static std::shared_ptr<AudioSystem> create(const std::shared_ptr<ftk::Context>&);

        //! Get the list of audio drivers.
        TL_TIMELINE_API const std::vector<std::string>& getDrivers() const;

        //! Get the current audio driver.
        TL_TIMELINE_API const std::string& getCurrentDriver() const;

        //! Get the list of audio devices.
        TL_TIMELINE_API const std::vector<AudioDeviceInfo>& getDevices() const;

        //! Observe the list of audio devices.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservableList<AudioDeviceInfo> > observeDevices() const;

        //! Get the default audio device.
        TL_TIMELINE_API AudioDeviceInfo getDefaultDevice() const;

        //! Observe the default audio device.
        TL_TIMELINE_API std::shared_ptr<ftk::IObservable<AudioDeviceInfo> > observeDefaultDevice() const;

        //! Get the size of the buffer the audio device is asked for, in
        //! sample frames. Zero is the device's own default.
        TL_TIMELINE_API size_t getBufferFrameCount() const;

        //! Set the size of the buffer the audio device is asked for. A
        //! device already open keeps the size it was opened with, and it
        //! stays open for as long as a player has it; the new size is used
        //! the next time it is opened, which with no players is at once.
        TL_TIMELINE_API void setBufferFrameCount(size_t);

        TL_TIMELINE_API void tick() override;
        TL_TIMELINE_API std::chrono::milliseconds getTickTime() const override;

    private:
        std::vector<AudioDeviceInfo> _getDevices();
        AudioDeviceInfo _getDefaultDevice();

        void _run();

        FTK_PRIVATE();
    };
}
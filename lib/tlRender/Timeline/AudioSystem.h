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

        TL_TIMELINE_API bool operator == (const AudioDeviceID&) const;
        TL_TIMELINE_API bool operator != (const AudioDeviceID&) const;
    };

    //! Audio device information.
    struct TL_TIMELINE_API_TYPE AudioDeviceInfo
    {
        AudioDeviceID id;
        AudioInfo     info;

        TL_TIMELINE_API bool operator == (const AudioDeviceInfo&) const;
        TL_TIMELINE_API bool operator != (const AudioDeviceInfo&) const;
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

        TL_TIMELINE_API void tick() override;
        TL_TIMELINE_API std::chrono::milliseconds getTickTime() const override;

    private:
        std::vector<AudioDeviceInfo> _getDevices();
        AudioDeviceInfo _getDefaultDevice();

        void _run();

        FTK_PRIVATE();
    };
}
// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace timeline_tests
    {
        //! What the player gives the audio device, checked sample by sample.
        //!
        //! Needs SDL's disk audio driver, which writes the device's output to
        //! a file rather than to hardware, so it runs where there is no sound
        //! card: SDL_AUDIO_DRIVER=disk, with SDL_AUDIO_DISK_OUTPUT_FILE naming
        //! the file. Without them it says so and checks nothing; the test is
        //! run with them on its own, see tests/tl-test/CMakeLists.txt.
        class PlayerAudioTest : public ftk::test::ITest
        {
        protected:
            PlayerAudioTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<PlayerAudioTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _tone();
        };
    }
}

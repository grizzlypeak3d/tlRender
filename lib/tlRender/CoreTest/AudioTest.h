// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Audio.h>

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace core_tests
    {
        class AudioTest : public ftk::test::ITest
        {
        protected:
            AudioTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<AudioTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            // Members rather than free helpers so they can report a
            // failed check, which goes through the test.
            template<AudioType DT, typename T>
            void _mixI();
            template<AudioType DT, typename T>
            void _mixF();

            void _enums();
            void _types();
            void _audio();
            void _combine();
            void _mix();
            void _reverse();
            void _convert();
            void _move();
            void _resample();
        };
    }
}

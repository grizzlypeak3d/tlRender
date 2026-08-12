// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

#include <tlRender/Timeline/Player.h>

namespace tl
{
    namespace timeline_tests
    {
        class PlayerTest : public ftk::test::ITest
        {
        protected:
            PlayerTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<PlayerTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _enums();
            void _player();
            void _player(const std::shared_ptr<Player>&);
            void _seqAndAudio();
            void _compare();
        };
    }
}

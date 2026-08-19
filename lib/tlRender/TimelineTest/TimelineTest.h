// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/TestLib/ITest.h>

#include <tlRender/Timeline/Timeline.h>

namespace tl
{
    namespace timeline_tests
    {
        class TimelineTest : public ftk::test::ITest
        {
        protected:
            TimelineTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<TimelineTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            void _enums();
            void _options();
            void _util();
            void _transitions();
            void _videoData();
            void _timeline();
            void _synchronous();
            void _media();
            void _readers();
            void _memLifetime();
            void _shutdown();
            void _timeline(const std::shared_ptr<Timeline>&);
            void _path();
            void _seqOnDisk();
            void _separateAudio();
            void _spatial();
            void _mediaReferences();
        };
    }
}

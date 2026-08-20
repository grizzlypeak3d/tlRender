// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include "tl-test.h"

#include <tlRender/UITest/ThumbnailSystemTest.h>

#include <tlRender/TimelineTest/AudioSystemTest.h>
#include <tlRender/TimelineTest/BackgroundOptionsTest.h>
#include <tlRender/TimelineTest/ColorOptionsTest.h>
#include <tlRender/TimelineTest/CompareOptionsTest.h>
#include <tlRender/TimelineTest/DisplayOptionsTest.h>
#include <tlRender/TimelineTest/ForegroundOptionsTest.h>
#include <tlRender/TimelineTest/PlayerOptionsTest.h>
#include <tlRender/TimelineTest/PlayerTest.h>
#include <tlRender/TimelineTest/TimeUnitsTest.h>
#include <tlRender/TimelineTest/TimelineTest.h>
#include <tlRender/TimelineTest/UtilTest.h>

#include <tlRender/GLTest/RenderTest.h>

#include <tlRender/IOTest/IOTest.h>
#include <tlRender/IOTest/RequestQueueTest.h>
#if defined(TLRENDER_FFMPEG_PLUGIN)
#include <tlRender/IOTest/FFmpegTest.h>
#endif // TLRENDER_FFMPEG_PLUGIN
#if defined(TLRENDER_EXR)
#include <tlRender/IOTest/EXRTest.h>
#endif // TLRENDER_EXR
#if defined(TLRENDER_OIIO)
#include <tlRender/IOTest/OIIOTest.h>
#endif // TLRENDER_OIIO
#if defined(TLRENDER_SVG)
#include <tlRender/IOTest/SVGTest.h>
#endif // TLRENDER_SVG

#include <tlRender/CoreTest/AudioTest.h>
#include <tlRender/CoreTest/HDRTest.h>
#include <tlRender/CoreTest/TimeTest.h>
#include <tlRender/CoreTest/URLTest.h>

#include <tlRender/UI/Init.h>
#include <tlRender/Timeline/Init.h>

#include <ftk/Core/CmdLine.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>
#include <ftk/Core/Time.h>

#include <algorithm>
#include <iostream>

using namespace tl;

namespace tl
{
    namespace tests
    {
        struct App::Private
        {
            std::shared_ptr<ftk::CmdLineListArg<std::string> > testNames;
            std::shared_ptr<ftk::CmdLineFlag> noGL;
            std::vector<std::shared_ptr<ftk::test::ITest> > tests;
            std::chrono::steady_clock::time_point startTime;
        };

        void App::_init(
            const std::shared_ptr<ftk::Context>& context,
            std::vector<std::string>& argv)
        {
            FTK_P();
            p.testNames = ftk::CmdLineListArg<std::string>::create(
                "Test",
                "Names of the tests to run.",
                true);
            p.noGL = ftk::CmdLineFlag::create(
                { "-noGL" },
                "Run only the tests that do not need OpenGL.");
            IApp::_init(
                context,
                argv,
                "tl-test",
                "Test application",
                { p.testNames },
                { p.noGL });
            p.startTime = std::chrono::steady_clock::now();

            // Only what the tests to be run need. The GL tests make a
            // context, and so does the thumbnail system that ui::init()
            // creates, so a binary that always called it needed OpenGL to
            // start whatever was being run -- which is why the platforms
            // whose runners have no working OpenGL run none of the suite.
            const bool gl = !p.noGL->found() && _needsGL(p.testNames->getList());
            if (gl)
            {
                ui::init(context);
            }
            else
            {
                tl::init(context);
            }

            // Core tests.
            p.tests.push_back(core_tests::AudioTest::create(context));
            p.tests.push_back(core_tests::HDRTest::create(context));
            p.tests.push_back(core_tests::TimeTest::create(context));
            p.tests.push_back(core_tests::URLTest::create(context));

            // I/O tests.
            p.tests.push_back(io_tests::IOTest::create(context));
            p.tests.push_back(io_tests::RequestQueueTest::create(context));
#if defined(TLRENDER_FFMPEG_PLUGIN)
            p.tests.push_back(io_tests::FFmpegTest::create(context));
#endif // TLRENDER_FFMPEG_PLUGIN
#if defined(TLRENDER_OIIO)
            p.tests.push_back(io_tests::OIIOTest::create(context));
#endif // TLRENDER_OIIO
#if defined(TLRENDER_SVG)
            p.tests.push_back(io_tests::SVGTest::create(context));
#endif // TLRENDER_SVG
#if defined(TLRENDER_EXR)
            p.tests.push_back(io_tests::EXRTest::create(context));
#endif // TLRENDER_EXR

            // GL tests.
            if (gl)
            {
                p.tests.push_back(gl_test::RenderTest::create(context));
            }

            // Timeline tests.
            p.tests.push_back(timeline_tests::AudioSystemTest::create(context));
            p.tests.push_back(timeline_tests::BackgroundOptionsTest::create(context));
            p.tests.push_back(timeline_tests::ColorOptionsTest::create(context));
            p.tests.push_back(timeline_tests::CompareOptionsTest::create(context));
            p.tests.push_back(timeline_tests::DisplayOptionsTest::create(context));
            p.tests.push_back(timeline_tests::ForegroundOptionsTest::create(context));
            p.tests.push_back(timeline_tests::PlayerOptionsTest::create(context));
            p.tests.push_back(timeline_tests::PlayerTest::create(context));
            p.tests.push_back(timeline_tests::TimeUnitsTest::create(context));
            p.tests.push_back(timeline_tests::TimelineTest::create(context));
            p.tests.push_back(timeline_tests::UtilTest::create(context));

            // UI tests.
            if (gl)
            {
                p.tests.push_back(ui_tests::ThumbnailSystemTest::create(context));
            }
        }

        bool App::_needsGL(const std::vector<std::string>& testNames)
        {
            // Nothing named means the whole suite, which includes them.
            if (testNames.empty())
            {
                return true;
            }
            // Matched against the names themselves, the way run() matches
            // them, so that a group name and a test name both work.
            for (const auto& name : testNames)
            {
                for (const std::string& glName : {
                    "gl_test::RenderTest",
                    "ui_tests::ThumbnailSystemTest" })
                {
                    if (ftk::contains(glName, name, ftk::CaseCompare::Insensitive))
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        App::App() :
            _p(new Private)
        {}

        App::~App()
        {}

        std::shared_ptr<App> App::create(
            const std::shared_ptr<ftk::Context>& context,
            std::vector<std::string>& argv)
        {
            auto out = std::shared_ptr<App>(new App);
            out->_init(context, argv);
            return out;
        }

        int App::run()
        {
            FTK_P();

            // Get the tests to run.
            std::vector<std::shared_ptr<ftk::test::ITest> > runTests;
            std::vector<std::string> unmatched;
            const auto& cmdLineTests = p.testNames->getList();
            if (!cmdLineTests.empty())
            {
                // Every test whose name contains the argument, not just the
                // first: a group name such as "io_tests" is the useful way to
                // ask for part of the suite.
                for (const auto& test : cmdLineTests)
                {
                    size_t matched = 0;
                    for (const auto& other : p.tests)
                    {
                        if (ftk::contains(other->getName(), test, ftk::CaseCompare::Insensitive))
                        {
                            ++matched;
                            if (std::find(runTests.begin(), runTests.end(), other) ==
                                runTests.end())
                            {
                                runTests.push_back(other);
                            }
                        }
                    }
                    if (0 == matched)
                    {
                        unmatched.push_back(test);
                    }
                }
            }
            else
            {
                for (const auto& test : p.tests)
                {
                    runTests.push_back(test);
                }
            }

            // A filter that matched nothing used to run zero tests and exit
            // successfully, which reads exactly like a suite that passed.
            if (!unmatched.empty())
            {
                for (const auto& name : unmatched)
                {
                    _print(ftk::Format("ERROR: no tests match: {0}").arg(name));
                }
                return 1;
            }

            // Run the tests.
            size_t failureCount = 0;
            for (const auto& test : runTests)
            {
                _context->tick();
                _print(ftk::Format("Running test: {0}").arg(test->getName()));
                test->run();
                failureCount += test->getFailureCount();
            }

            const auto now = std::chrono::steady_clock::now();
            const std::chrono::duration<float> diff = now - p.startTime;
            _print(ftk::Format("Seconds elapsed: {0}").arg(diff.count(), 2));
            _print(ftk::Format("Tests run: {0}").arg(runTests.size()));
            _print(ftk::Format("Failures: {0}").arg(failureCount));

            // The count is printed rather than returned: exit codes are
            // truncated to eight bits, so a run with a multiple of 256
            // failures would report success.
            return failureCount > 0 ? 1 : 0;
        }
    }
}

int main(int argc, char* argv[])
{
    try
    {
        auto context = ftk::Context::create();
        auto args = ftk::convert(argc, argv);
        auto app = tl::tests::App::create(context, args);
        if (app->hasCmdLineHelp())
            return 0;
        return app->run();
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

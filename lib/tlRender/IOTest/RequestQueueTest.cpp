// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IOTest/RequestQueueTest.h>

#include <tlRender/IO/RequestQueuePrivate.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Context.h>
#include <ftk/Core/Format.h>

#include <sstream>
#include <thread>

namespace tl
{
    namespace io_tests
    {
        namespace
        {
            struct IntRequest
            {
                int in = 0;
                std::promise<int> promise;
            };

            struct StringRequest
            {
                std::promise<std::string> promise;
            };
        }

        RequestQueueTest::RequestQueueTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "io_tests::RequestQueueTest")
        {}

        std::shared_ptr<RequestQueueTest> RequestQueueTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<RequestQueueTest>(new RequestQueueTest(context));
        }

        void RequestQueueTest::run()
        {
            _roundTrip();
            _shutdown();
            _stopQueues();
            _stopQueue();
            _cancel();
            _promiseGuard();
        }

        void RequestQueueTest::_roundTrip()
        {
            _print("Round trip");
            // Two queues sharing one condition, serviced by one worker,
            // mirroring the video thread's info and video queues.
            RequestCondition condition;
            RequestQueue<IntRequest, int> ints(condition);
            RequestQueue<StringRequest, std::string> strings(condition);
            std::thread worker(
                [&]
                {
                    while (condition.wait())
                    {
                        for (const auto& request : strings.popAll())
                        {
                            request->promise.set_value("info");
                        }
                        if (auto request = ints.pop())
                        {
                            PromiseGuard<int> guard(request->promise);
                            guard.setValue(request->in * 2);
                        }
                    }
                    condition.stopQueues();
                });
            auto intRequest = std::make_shared<IntRequest>();
            intRequest->in = 21;
            auto stringRequest = std::make_shared<StringRequest>();
            auto intFuture = ints.push(intRequest);
            auto stringFuture = strings.push(stringRequest);
            FTK_ASSERT(42 == intFuture.get());
            FTK_ASSERT("info" == stringFuture.get());
            condition.stop();
            worker.join();
        }

        void RequestQueueTest::_shutdown()
        {
            _print("Shutdown");
            // stop() must wake a waiting worker. There is no timeout to
            // fall back on: a worker waits until it is given something to
            // do or told to stop, so a missed notify hangs here rather
            // than being papered over by the next poll.
            RequestCondition condition;
            RequestQueue<IntRequest, int> queue(condition);
            std::thread worker(
                [&]
                {
                    while (condition.wait())
                        ;
                    condition.stopQueues();
                });
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            const auto t0 = std::chrono::steady_clock::now();
            condition.stop();
            worker.join();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count();
            std::stringstream ss;
            ss << "Shutdown wake: " << elapsed << "ms";
            _print(ss.str());
            if (elapsed >= 10000)
            {
                _error("Shutdown had to wait for the timeout");
                FTK_ASSERT(false);
            }
        }

        void RequestQueueTest::_stopQueues()
        {
            _print("Stop queues");
            RequestCondition condition;
            RequestQueue<IntRequest, int> queue(condition);
            // Pending requests are completed with default values.
            auto pending = std::make_shared<IntRequest>();
            pending->in = 5;
            auto pendingFuture = queue.push(pending);
            condition.stopQueues();
            FTK_ASSERT(0 == pendingFuture.get());
            // Requests pushed after the queues are stopped complete
            // immediately.
            auto late = std::make_shared<IntRequest>();
            late->in = 7;
            auto lateFuture = queue.push(late);
            FTK_ASSERT(std::future_status::ready ==
                lateFuture.wait_for(std::chrono::seconds(0)));
            FTK_ASSERT(0 == lateFuture.get());
        }

        void RequestQueueTest::_stopQueue()
        {
            _print("Stop queue");
            // One queue can be stopped on its own -- an audio queue for a
            // file with no audio -- while the worker keeps serving the
            // information queue on the same condition.
            RequestCondition condition;
            RequestQueue<IntRequest, int> ints(condition);
            RequestQueue<StringRequest, std::string> strings(condition);
            std::thread worker(
                [&]
                {
                    while (condition.wait())
                    {
                        for (const auto& request : strings.popAll())
                        {
                            request->promise.set_value("info");
                        }
                    }
                    condition.stopQueues();
                });
            auto pending = std::make_shared<IntRequest>();
            pending->in = 5;
            auto pendingFuture = ints.push(pending);
            ints.stop();
            FTK_ASSERT(0 == pendingFuture.get());
            auto late = std::make_shared<IntRequest>();
            late->in = 7;
            auto lateFuture = ints.push(late);
            FTK_ASSERT(std::future_status::ready ==
                lateFuture.wait_for(std::chrono::seconds(0)));
            FTK_ASSERT(0 == lateFuture.get());
            auto stringRequest = std::make_shared<StringRequest>();
            auto stringFuture = strings.push(stringRequest);
            FTK_ASSERT("info" == stringFuture.get());
            condition.stop();
            worker.join();
        }

        void RequestQueueTest::_cancel()
        {
            _print("Cancel");
            RequestCondition condition;
            RequestQueue<IntRequest, int> queue(condition);
            auto a = std::make_shared<IntRequest>();
            auto aFuture = queue.push(a);
            FTK_ASSERT(1 == queue.size());
            queue.cancel();
            FTK_ASSERT(0 == aFuture.get());
            FTK_ASSERT(0 == queue.size());
            // The queue keeps working after a cancel.
            auto b = std::make_shared<IntRequest>();
            b->in = 3;
            auto bFuture = queue.push(b);
            auto popped = queue.pop();
            FTK_ASSERT(popped == b);
            popped->promise.set_value(popped->in);
            FTK_ASSERT(3 == bFuture.get());
        }

        void RequestQueueTest::_promiseGuard()
        {
            _print("Promise guard");
            // The guard completes the promise with a default value if it
            // is destroyed by an exception.
            std::promise<int> promise;
            auto future = promise.get_future();
            try
            {
                PromiseGuard<int> guard(promise);
                throw std::runtime_error("boom");
            }
            catch (const std::exception&)
            {}
            FTK_ASSERT(0 == future.get());
            // Setting a value dismisses the guard.
            std::promise<int> promise2;
            auto future2 = promise2.get_future();
            {
                PromiseGuard<int> guard(promise2);
                guard.setValue(9);
            }
            FTK_ASSERT(9 == future2.get());
        }
    }
}

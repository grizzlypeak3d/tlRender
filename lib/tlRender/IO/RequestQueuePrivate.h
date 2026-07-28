// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <vector>

namespace tl
{
    class RequestCondition;

    //! Base class for request queues. See RequestQueue.
    class IRequestQueue
    {
    protected:
        virtual ~IRequestQueue() = 0;

        //! Called with the condition lock held.
        virtual bool _isEmpty() const = 0;

        //! Mark the queue as stopped and stage the pending requests for
        //! cancellation. Called with the condition lock held.
        virtual void _stop() = 0;

        //! Complete the staged requests with default values. Called with
        //! the condition lock released, since completing a promise can
        //! run caller code.
        virtual void _cancelStaged() = 0;

        friend class RequestCondition;
    };

    inline IRequestQueue::~IRequestQueue()
    {}

    //! The shared state for the request queues serviced by a single
    //! worker thread. Multiple queues can share one condition so that
    //! the worker can wait for a request on any of them.
    //!
    //! The worker waits rather than polls: a queue and its condition
    //! share one mutex, so a request cannot be pushed between the wait
    //! predicate being tested and the wait beginning, and every path that
    //! makes the predicate true notifies. An idle worker therefore costs
    //! nothing, which matters because each cached reader holds one.
    //!
    //! The lifecycle invariants that this class centralizes:
    //! * stop() wakes the worker immediately; shutdown never waits.
    //! * stopQueues() is the worker's epilogue: every registered queue
    //!   is marked stopped, so new requests complete immediately with
    //!   default values, and the pending ones are cancelled. Queues
    //!   register themselves, so an epilogue cannot forget one.
    class RequestCondition
    {
    public:
        RequestCondition() = default;
        RequestCondition(const RequestCondition&) = delete;
        RequestCondition& operator=(const RequestCondition&) = delete;

        //! Wait until a request is pending on any registered queue or the
        //! condition is stopped. Returns whether the condition is still
        //! running; the worker thread loops while this is true.
        bool wait()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cv.wait(
                lock,
                [this]
                {
                    bool out = !_running;
                    for (auto i = _queues.begin();
                        !out && i != _queues.end();
                        ++i)
                    {
                        out = !(*i)->_isEmpty();
                    }
                    return out;
                });
            return _running;
        }

        //! Stop the condition and wake the worker thread. Called by the
        //! owner, typically from a destructor before joining the thread.
        void stop()
        {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _running = false;
            }
            _cv.notify_one();
        }

        //! Whether the condition is running.
        bool isRunning() const
        {
            return _running;
        }

        //! Mark every registered queue as stopped and cancel the pending
        //! requests. Called by the worker thread when it exits, whether
        //! normally or from an exception; afterwards new requests
        //! complete immediately with default values.
        void stopQueues()
        {
            {
                std::unique_lock<std::mutex> lock(_mutex);
                for (const auto& queue : _queues)
                {
                    queue->_stop();
                }
            }
            for (const auto& queue : _queues)
            {
                queue->_cancelStaged();
            }
        }

    private:
        template<typename Request, typename Result>
        friend class RequestQueue;

        std::mutex _mutex;
        std::condition_variable _cv;
        std::atomic<bool> _running{ true };
        std::vector<IRequestQueue*> _queues;
    };

    //! A queue of requests serviced by a worker thread. The Request type
    //! provides a "promise" member of type std::promise<Result>.
    //!
    //! Requests pushed after the queue is stopped are completed
    //! immediately with a default constructed result, matching the
    //! behavior of cancellation, so callers always receive a valid
    //! future that will complete.
    template<typename Request, typename Result>
    class RequestQueue : public IRequestQueue
    {
    public:
        explicit RequestQueue(RequestCondition& condition) :
            _condition(condition)
        {
            std::unique_lock<std::mutex> lock(condition._mutex);
            condition._queues.push_back(this);
        }

        ~RequestQueue() override
        {
            std::unique_lock<std::mutex> lock(_condition._mutex);
            const auto i = std::find(
                _condition._queues.begin(),
                _condition._queues.end(),
                this);
            if (i != _condition._queues.end())
            {
                _condition._queues.erase(i);
            }
        }

        //! Push a request and get the future for its result. If the
        //! queue has been stopped the request is completed immediately
        //! with a default constructed result.
        std::future<Result> push(const std::shared_ptr<Request>& request)
        {
            auto future = request->promise.get_future();
            bool stopped = false;
            {
                std::unique_lock<std::mutex> lock(_condition._mutex);
                stopped = _stopped;
                if (!stopped)
                {
                    _requests.push_back(request);
                }
            }
            if (stopped)
            {
                request->promise.set_value(Result());
            }
            else
            {
                _condition._cv.notify_one();
            }
            return future;
        }

        //! Pop one pending request, or return nullptr. Called by the
        //! worker thread.
        std::shared_ptr<Request> pop()
        {
            std::unique_lock<std::mutex> lock(_condition._mutex);
            std::shared_ptr<Request> out;
            if (!_requests.empty())
            {
                out = _requests.front();
                _requests.pop_front();
            }
            return out;
        }

        //! Pop all of the pending requests. Called by the worker thread.
        std::list<std::shared_ptr<Request> > popAll()
        {
            std::unique_lock<std::mutex> lock(_condition._mutex);
            return std::move(_requests);
        }

        //! Get the number of pending requests.
        size_t size() const
        {
            std::unique_lock<std::mutex> lock(_condition._mutex);
            return _requests.size();
        }

        //! Mark this queue as stopped and cancel its pending requests,
        //! leaving the other queues on the condition running. Used when
        //! the worker will never serve this queue -- a file with no audio,
        //! say -- while it still serves information requests.
        void stop()
        {
            {
                std::unique_lock<std::mutex> lock(_condition._mutex);
                _stop();
            }
            _cancelStaged();
        }

        //! Cancel the pending requests, completing them with default
        //! constructed results.
        void cancel()
        {
            std::list<std::shared_ptr<Request> > requests;
            {
                std::unique_lock<std::mutex> lock(_condition._mutex);
                requests = std::move(_requests);
            }
            for (const auto& request : requests)
            {
                request->promise.set_value(Result());
            }
        }

    protected:
        bool _isEmpty() const override
        {
            return _requests.empty();
        }

        void _stop() override
        {
            _stopped = true;
            _staged.splice(_staged.end(), _requests);
        }

        void _cancelStaged() override
        {
            std::list<std::shared_ptr<Request> > requests;
            {
                std::unique_lock<std::mutex> lock(_condition._mutex);
                requests = std::move(_staged);
            }
            for (const auto& request : requests)
            {
                request->promise.set_value(Result());
            }
        }

    private:
        RequestCondition& _condition;
        std::list<std::shared_ptr<Request> > _requests;
        std::list<std::shared_ptr<Request> > _staged;
        bool _stopped = false;
    };

    //! Completes a promise with a default constructed value when
    //! destroyed, unless a value was set through the guard first. Used
    //! by worker threads to guarantee that an in-flight request is
    //! never dropped with a broken promise, even if an exception is
    //! thrown while servicing it.
    template<typename T>
    class PromiseGuard
    {
    public:
        explicit PromiseGuard(std::promise<T>& promise) :
            _promise(&promise)
        {}

        PromiseGuard(const PromiseGuard&) = delete;
        PromiseGuard& operator=(const PromiseGuard&) = delete;

        ~PromiseGuard()
        {
            if (_promise)
            {
                try
                {
                    _promise->set_value(T());
                }
                catch (const std::future_error&)
                {}
            }
        }

        //! Set the promise value and dismiss the guard.
        void setValue(T&& value)
        {
            _promise->set_value(std::move(value));
            _promise = nullptr;
        }

    private:
        std::promise<T>* _promise = nullptr;
    };
}

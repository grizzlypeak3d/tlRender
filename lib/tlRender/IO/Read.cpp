// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/Read.h>

#include <ftk/Core/LogSystem.h>

#include <mutex>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tl
{
    struct IRead::CancellationState
    {
        std::shared_ptr<std::atomic_bool> cancelled =
            std::make_shared<std::atomic_bool>(false);
#if defined(_WIN32)
        struct ThreadHandle
        {
            DWORD id = 0;
            HANDLE handle = nullptr;
        };
        std::mutex mutex;
        std::vector<ThreadHandle> threads;
#endif
    };

    void IRead::_init(
        const ftk::Path& path,
        const std::vector<ftk::MemFile>& mem,
        const IOOptions& options,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        IIO::_init(path, options, logSystem);
        _mem = mem;
    }

    IRead::IRead() :
        _cancellation(std::make_shared<CancellationState>())
    {}

    IRead::~IRead()
    {
        cancelIO();
#if defined(_WIN32)
        std::lock_guard<std::mutex> lock(_cancellation->mutex);
        for (const auto& i : _cancellation->threads)
        {
            if (i.handle)
            {
                CloseHandle(i.handle);
            }
        }
        _cancellation->threads.clear();
#endif
    }

    void IRead::cancelIO()
    {
        _cancellation->cancelled->store(true, std::memory_order_release);
#if defined(_WIN32)
        std::lock_guard<std::mutex> lock(_cancellation->mutex);
        for (const auto& i : _cancellation->threads)
        {
            if (i.handle)
            {
                CancelSynchronousIo(i.handle);
            }
        }
#endif
    }

    bool IRead::isIOCancellationRequested() const
    {
        return _cancellation->cancelled->load(std::memory_order_acquire);
    }

    void IRead::_bindIOCancellation()
    {
#if defined(_WIN32)
        HANDLE duplicate = nullptr;
        if (!DuplicateHandle(
            GetCurrentProcess(),
            GetCurrentThread(),
            GetCurrentProcess(),
            &duplicate,
            0,
            FALSE,
            DUPLICATE_SAME_ACCESS))
        {
            return;
        }
        const DWORD id = GetCurrentThreadId();
        std::lock_guard<std::mutex> lock(_cancellation->mutex);
        _cancellation->threads.push_back({ id, duplicate });
        if (_cancellation->cancelled->load(std::memory_order_acquire))
        {
            CancelSynchronousIo(duplicate);
        }
#endif
    }

    void IRead::_unbindIOCancellation()
    {
#if defined(_WIN32)
        const DWORD id = GetCurrentThreadId();
        std::lock_guard<std::mutex> lock(_cancellation->mutex);
        for (auto i = _cancellation->threads.begin();
            i != _cancellation->threads.end(); ++i)
        {
            if (i->id == id)
            {
                if (i->handle)
                {
                    CloseHandle(i->handle);
                }
                _cancellation->threads.erase(i);
                break;
            }
        }
#endif
    }

    const std::shared_ptr<std::atomic_bool>& IRead::_getIOCancellationFlag() const
    {
        return _cancellation->cancelled;
    }

    std::string IRead::getError() const
    {
        return std::string();
    }

    size_t IRead::getErrorCount() const
    {
        return 0;
    }

    IVideoRead::~IVideoRead()
    {}

    IAudioRead::~IAudioRead()
    {}

    struct IReadPlugin::Private
    {
    };

    void IReadPlugin::_init(
        const std::string& name,
        const std::map<std::string, FileType>& extensions,
        const std::shared_ptr<ftk::LogSystem>& logSystem)
    {
        IIOPlugin::_init(name, extensions, logSystem);
    }

    IReadPlugin::IReadPlugin() :
        _p(new Private)
    {}

    IReadPlugin::~IReadPlugin()
    {}

    std::shared_ptr<IVideoRead> IReadPlugin::videoRead(
        const ftk::Path& path,
        const IOOptions& options)
    {
        return videoRead(path, {}, options);
    }

    std::shared_ptr<IVideoRead> IReadPlugin::videoRead(
        const ftk::Path&,
        const std::vector<ftk::MemFile>&,
        const IOOptions&)
    {
        return nullptr;
    }

    std::shared_ptr<IAudioRead> IReadPlugin::audioRead(
        const ftk::Path& path,
        const IOOptions& options)
    {
        return audioRead(path, {}, options);
    }

    std::shared_ptr<IAudioRead> IReadPlugin::audioRead(
        const ftk::Path&,
        const std::vector<ftk::MemFile>&,
        const IOOptions&)
    {
        return nullptr;
    }

    std::shared_ptr<IDecode> IReadPlugin::decode(const IOOptions&)
    {
        return nullptr;
    }
}
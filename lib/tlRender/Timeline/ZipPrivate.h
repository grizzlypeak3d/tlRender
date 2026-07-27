// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <ftk/Core/LogSystem.h>

#include <map>
#include <optional>

#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

namespace tl
{
    class ZipReader
    {
        FTK_NON_COPYABLE(ZipReader);

        struct MZReaderDeleter
        {
            void operator()(void*) const;
        };
        using MZReaderPtr = std::unique_ptr<void, MZReaderDeleter>;

        struct MZEntryScope
        {
            MZEntryScope(void*);
            ~MZEntryScope();
            void* p = nullptr;
        };

    public:
        ZipReader(const std::shared_ptr<ftk::LogSystem>&);

        void open(
            const std::string& fileName,
            size_t fileSize);

        struct Entry { int64_t offset; int64_t size; };

        std::optional<Entry> find(const std::string& name) const;

        std::string readText(const std::string& name);

    private:
        std::shared_ptr<ftk::LogSystem> _logSystem;
        std::string _fileName;
        size_t _fileSize = 0;
        MZReaderPtr _reader;
        std::map<std::string, Entry> _entries;
    };
}

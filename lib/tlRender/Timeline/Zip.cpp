// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/ZipPrivate.h>

#include <ftk/Core/FileIO.h>
#include <ftk/Core/Format.h>

namespace tl
{
    namespace
    {
        uint16_t readLE16(const uint8_t* p)
        {
            return
                static_cast<uint16_t>(p[0]) |
                (static_cast<uint16_t>(p[1]) << 8);
        }

        uint32_t readLE32(const uint8_t* p)
        {
            return
                static_cast<uint32_t>(p[0]) |
                (static_cast<uint32_t>(p[1]) << 8) |
                (static_cast<uint32_t>(p[2]) << 16) |
                (static_cast<uint32_t>(p[3]) << 24);
        }

        constexpr uint32_t zipHeaderMagic = 0x04034b50u;
        constexpr size_t zipHeaderNameOffset = 26;
        constexpr size_t zipHeaderExtraLenOffset = 28;
        constexpr size_t zipHeaderSize = 30;
    }

    void ZipReader::MZReaderDeleter::operator()(void* p) const
    {
        if (p) mz_zip_reader_delete(&p);
    }

    ZipReader::MZEntryScope::MZEntryScope(void* p) :
        p(p)
    {}

    ZipReader::MZEntryScope::~MZEntryScope()
    { 
        if (p) mz_zip_reader_entry_close(p);
    }

    ZipReader::ZipReader(const std::shared_ptr<ftk::LogSystem>& logSystem) :
        _logSystem(logSystem)
    {}

    void ZipReader::open(
        const std::string& fileName,
        const uint8_t* fileMMap,
        size_t fileSize)
    {
        if (_reader)
        {
            _reader.reset();
            _entries.clear();
        }

        _fileName = fileName;
        _fileMMap = fileMMap;
        _fileSize = fileSize;

        _reader.reset(mz_zip_reader_create());
        if (!_reader.get())
        {
            throw std::runtime_error(ftk::Format(
                "Cannot create zip reader: \"{0}\"").arg(fileName));
        }
        int32_t err = mz_zip_reader_open_file(_reader.get(), fileName.c_str());
        if (err != MZ_OK)
        {
            throw std::runtime_error(ftk::Format(
                "Cannot open zip reader: \"{0}\"").arg(fileName));
        }

        err = mz_zip_reader_goto_first_entry(_reader.get());
        if (err != MZ_OK)
        {
            throw std::runtime_error(ftk::Format(
                "Cannot goto first zip entry: \"{0}\"").arg(fileName));
        }

        auto headerIO = ftk::FileIO::create(
            fileName,
            ftk::FileMode::Read,
            ftk::FileRead::Normal);
        while (MZ_OK == err)
        {
            mz_zip_file* fileInfo = nullptr;
            err = mz_zip_reader_entry_get_info(_reader.get(), &fileInfo);
            if (err != MZ_OK || !fileInfo)
            {
                throw std::runtime_error(ftk::Format(
                    "Cannot get zip entry information: \"{0}\"").arg(fileName));
            }
            if (mz_zip_reader_entry_is_dir(_reader.get()) != MZ_OK &&
                0 == fileInfo->compression_method)
            {
                if (fileInfo->disk_offset < 0 ||
                    static_cast<size_t>(fileInfo->disk_offset) + zipHeaderSize > _fileSize)
                {
                    throw std::runtime_error(ftk::Format(
                        "Local zip header entry out of bounds: \"{0}\"").arg(fileName));
                }
                // Read the local header rather than reaching into the memory
                // map. There is one of these before every file's data, so they
                // are spread across the whole bundle, and faulting each one in
                // costs a page cluster: measured on Windows, 25,000 headers of
                // thirty bytes each pulled tens of gigabytes from disk and took
                // minutes. Reading them costs thirty bytes each.
                uint8_t hdr[zipHeaderSize];
                headerIO->seek(fileInfo->disk_offset, ftk::SeekMode::Set);
                headerIO->read(hdr, zipHeaderSize);
                if (readLE32(hdr) != zipHeaderMagic)
                {
                    throw std::runtime_error(ftk::Format(
                        "Bad local zip header: \"{0}\"").arg(fileName));
                }
                const uint16_t nameLen = readLE16(hdr + zipHeaderNameOffset);
                const uint16_t extraLen = readLE16(hdr + zipHeaderExtraLenOffset);
                const int64_t dataOffset =
                    fileInfo->disk_offset + zipHeaderSize + nameLen + extraLen;

                if (dataOffset < 0 ||
                    static_cast<size_t>(dataOffset) > _fileSize ||
                    fileInfo->uncompressed_size < 0 ||
                    static_cast<size_t>(fileInfo->uncompressed_size) > _fileSize - dataOffset)
                {
                    throw std::runtime_error(ftk::Format(
                        "Local zip entry out of bounds: \"{0}\"").arg(fileName));
                }
                Entry entry{ dataOffset, fileInfo->uncompressed_size };
                if (!_entries.emplace(fileInfo->filename, entry).second)
                {
                    _logSystem->print("tl::ZipReader", ftk::Format(
                        "Duplicate zip entry, ignoring subsequent: \"{0}\"").arg(fileInfo->filename),
                        ftk::LogType::Warning);
                }
            }
            err = mz_zip_reader_goto_next_entry(_reader.get());
            if (err != MZ_OK && err != MZ_END_OF_LIST)
            {
                throw std::runtime_error(ftk::Format(
                    "Cannot goto next zip entry: \"{0}\"").arg(fileName));
            }
        }
    }

    std::optional<ZipReader::Entry> ZipReader::find(const std::string& name) const
    {
        const auto i = _entries.find(name);
        return i != _entries.end() ? std::optional<Entry>(i->second) : std::nullopt;
    }

    std::string ZipReader::readText(const std::string& name)
    {
        int32_t err = mz_zip_reader_locate_entry(
            _reader.get(),
            name.c_str(),
            0);
        if (err != MZ_OK)
        {
            throw std::runtime_error(ftk::Format(
                "Cannot find zip entry: \"{0}\"").arg(name));
        }
        err = mz_zip_reader_entry_open(_reader.get());
        if (err != MZ_OK)
        {
            throw std::runtime_error(ftk::Format(
                "Cannot open zip entry: \"{0}\"").arg(name));
        }
        MZEntryScope entry(_reader.get());
        mz_zip_file* fileInfo = nullptr;
        err = mz_zip_reader_entry_get_info(_reader.get(), &fileInfo);
        if (err != MZ_OK || !fileInfo)
        {
            throw std::runtime_error(ftk::Format(
                "Cannot get zip entry information: \"{0}\"").arg(name));
        }
        if (fileInfo->uncompressed_size > INT32_MAX)
        {
            throw std::runtime_error(ftk::Format(
                "Text zip entry exceeds max size: \"{0}\"").arg(name));
        }
        std::string out(fileInfo->uncompressed_size, 0);
        err = mz_zip_reader_entry_read(
            _reader.get(),
            out.data(),
            fileInfo->uncompressed_size);
        if (err != fileInfo->uncompressed_size)
        {
            throw std::runtime_error(ftk::Format(
                "Cannot read zip entry: \"{0}\"").arg(name));
        }
        return out;
    }

}

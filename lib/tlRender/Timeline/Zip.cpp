// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/ZipPrivate.h>

#include <ftk/Core/FileIO.h>
#include <ftk/Core/Format.h>

#include <algorithm>
#include <vector>

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

        //! The general purpose flag saying a data descriptor follows the data.
        constexpr uint16_t zipFlagDataDescriptor = 0x8;

        //! The extra field is the only unknown between a local header and its
        //! data, and it is length prefixed with sixteen bits.
        constexpr int64_t zipMaxExtraSize = 65535;

        //! How many derived offsets to check against the file.
        //!
        //! Checking every one would be the scattered read per entry that
        //! deriving them exists to avoid. A bundle is written by one writer in
        //! one pass, so entries are laid out the same way throughout, and a
        //! sample spread across the file sees a writer that does otherwise.
        constexpr size_t zipVerifySamples = 64;

        //! Read a local header to get where an entry's data actually starts.
        int64_t readDataOffset(
            const std::shared_ptr<ftk::FileIO>& io,
            const std::string& fileName,
            int64_t headerOffset)
        {
            uint8_t hdr[zipHeaderSize];
            io->readAt(hdr, headerOffset, zipHeaderSize);
            if (readLE32(hdr) != zipHeaderMagic)
            {
                throw std::runtime_error(ftk::Format(
                    "Bad local zip header: \"{0}\"").arg(fileName));
            }
            return
                headerOffset +
                zipHeaderSize +
                readLE16(hdr + zipHeaderNameOffset) +
                readLE16(hdr + zipHeaderExtraLenOffset);
        }
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
        size_t fileSize)
    {
        if (_reader)
        {
            _reader.reset();
            _entries.clear();
        }

        _fileName = fileName;
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

        // Collect the central directory, which minizip has already read as one
        // contiguous region at the end of the file. Nothing here touches the
        // rest of the bundle.
        struct Record
        {
            std::string name;
            int64_t     headerOffset = 0;
            int64_t     size = 0;
            int64_t     minDataOffset = 0;
            bool        trailer = false;
            int64_t     dataOffset = -1;
        };
        std::vector<Record> records;
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
                Record record;
                record.name          = fileInfo->filename;
                record.headerOffset  = fileInfo->disk_offset;
                record.size          = fileInfo->uncompressed_size;
                record.minDataOffset =
                    fileInfo->disk_offset + zipHeaderSize + fileInfo->filename_size;
                record.trailer       = (fileInfo->flag & zipFlagDataDescriptor) != 0;
                records.push_back(record);
            }
            err = mz_zip_reader_goto_next_entry(_reader.get());
            if (err != MZ_OK && err != MZ_END_OF_LIST)
            {
                throw std::runtime_error(ftk::Format(
                    "Cannot goto next zip entry: \"{0}\"").arg(fileName));
            }
        }

        std::sort(
            records.begin(),
            records.end(),
            [](const Record& a, const Record& b)
            {
                return a.headerOffset < b.headerOffset;
            });

        // Where an entry's data starts is only recorded in its local header,
        // and those sit one before each file's data, spread across the whole
        // bundle. Reading them all is what made opening a large bundle take
        // minutes: measured on Windows, 25,000 headers of thirty bytes each
        // pulled tens of gigabytes from disk at 0% CPU.
        //
        // They do not have to be read. An entry's data ends where the next
        // entry's local header begins, so the offset follows from the size in
        // the central directory. That leaves the last entry, and any entry
        // whose data is followed by a descriptor, to be read.
        auto io = ftk::FileIO::create(
            fileName,
            ftk::FileMode::Read,
            ftk::FileRead::Normal,
            ftk::FileAccess::Random);
        size_t readCount = 0;
        for (size_t i = 0; i < records.size(); ++i)
        {
            Record& record = records[i];
            if (!record.trailer && i + 1 < records.size())
            {
                const int64_t derived = records[i + 1].headerOffset - record.size;
                if (derived >= record.minDataOffset &&
                    derived - record.minDataOffset <= zipMaxExtraSize)
                {
                    record.dataOffset = derived;
                }
            }
            if (-1 == record.dataOffset)
            {
                record.dataOffset = readDataOffset(io, fileName, record.headerOffset);
                ++readCount;
            }
        }

        // Check a sample of the derived offsets against the file itself.
        const size_t sampleCount = std::min(zipVerifySamples, records.size());
        for (size_t i = 0; i < sampleCount; ++i)
        {
            const Record& sample = records[i * records.size() / sampleCount];
            if (sample.dataOffset ==
                readDataOffset(io, fileName, sample.headerOffset))
            {
                continue;
            }
            // The writer lays entries out in a way the derivation does not
            // describe, so every offset has to come from its own header.
            _logSystem->print("tl::ZipReader", ftk::Format(
                "Zip entry offsets cannot be derived, reading {0} local "
                "headers: \"{1}\"").arg(records.size()).arg(fileName),
                ftk::LogType::Warning);
            for (auto& record : records)
            {
                record.dataOffset =
                    readDataOffset(io, fileName, record.headerOffset);
            }
            readCount = records.size();
            break;
        }

        for (const auto& record : records)
        {
            if (record.dataOffset < 0 ||
                static_cast<size_t>(record.dataOffset) > _fileSize ||
                record.size < 0 ||
                static_cast<size_t>(record.size) > _fileSize - record.dataOffset)
            {
                throw std::runtime_error(ftk::Format(
                    "Local zip entry out of bounds: \"{0}\"").arg(fileName));
            }
            Entry entry{ record.dataOffset, record.size };
            if (!_entries.emplace(record.name, entry).second)
            {
                _logSystem->print("tl::ZipReader", ftk::Format(
                    "Duplicate zip entry, ignoring subsequent: \"{0}\"").arg(record.name),
                    ftk::LogType::Warning);
            }
        }

        _logSystem->print("tl::ZipReader", ftk::Format(
            "Opened \"{0}\": {1} entries, {2} local headers read").
            arg(fileName).arg(records.size()).arg(readCount),
            ftk::LogType::Message);
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

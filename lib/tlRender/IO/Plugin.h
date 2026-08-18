// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/IO.h>

#include <ftk/Core/FileIO.h>
#include <ftk/Core/Path.h>

#include <future>
#include <set>

namespace ftk
{
    class LogSystem;
}

namespace tl
{
    //! Base class for readers and writers.
    class TL_API_TYPE IIO : public std::enable_shared_from_this<IIO>
    {
        FTK_NON_COPYABLE(IIO);

    protected:
        void _init(
            const ftk::Path&,
            const IOOptions&,
            const std::shared_ptr<ftk::LogSystem>&);

        IIO();

    public:
        TL_API virtual ~IIO() = 0;

        //! Get the path.
        TL_API const ftk::Path& getPath() const;

        //! Get the number of objects currenty instantiated.
        TL_API static size_t getObjectCount();

    protected:
        ftk::Path _path;
        IOOptions _options;
        std::weak_ptr<ftk::LogSystem> _logSystem;
    };

    //! Base class for I/O plugins.
    class TL_API_TYPE IIOPlugin : public std::enable_shared_from_this<IIOPlugin>
    {
        FTK_NON_COPYABLE(IIOPlugin);

    protected:
        void _init(
            const std::string& name,
            const std::map<std::string, FileType>& exts,
            const std::shared_ptr<ftk::LogSystem>&);

        IIOPlugin();

    public:
        TL_API virtual ~IIOPlugin() = 0;

        //! Get the plugin name.
        TL_API const std::string& getPluginName() const;

        //! Get the plugin information.
        TL_API virtual std::string getPluginInfo(
            const IOOptions& = IOOptions()) const;

        //! Get the supported file extensions.
        TL_API std::set<std::string> getExts(int types =
            static_cast<int>(FileType::Media) |
            static_cast<int>(FileType::Seq) |
            static_cast<int>(FileType::Audio)) const;

    protected:
        std::weak_ptr<ftk::LogSystem> _logSystem;

    private:
        FTK_PRIVATE();
    };
}

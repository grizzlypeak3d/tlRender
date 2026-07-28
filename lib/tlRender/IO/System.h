// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Read.h>
#include <tlRender/IO/Write.h>

#include <ftk/Core/ISystem.h>

namespace tl
{
    //! Read system.
    class TL_API_TYPE ReadSystem : public ftk::ISystem
    {
        FTK_NON_COPYABLE(ReadSystem);

    protected:
        ReadSystem(const std::shared_ptr<ftk::Context>&);

    public:
        TL_API virtual ~ReadSystem();

        //! Create a new system.
        TL_API static std::shared_ptr<ReadSystem> create(const std::shared_ptr<ftk::Context>&);

        //! Get the list of plugins.
        const std::vector<std::shared_ptr<IReadPlugin> >& getPlugins() const;

        //! Add a plugin.
        TL_API void addPlugin(const std::shared_ptr<IReadPlugin>&);

        //! Remove a plugin.
        TL_API void removePlugin(const std::shared_ptr<IReadPlugin>&);

        //! Get a plugin.
        template<typename T>
        std::shared_ptr<T> getPlugin() const;

        //! Get a plugin for the given path.
        TL_API std::shared_ptr<IReadPlugin> getPlugin(const ftk::Path&) const;

        //! Get the plugin names.
        TL_API const std::vector<std::string>& getNames() const;

        //! Get the supported file extensions.
        TL_API std::set<std::string> getExts(int types =
            static_cast<int>(FileType::Media) |
            static_cast<int>(FileType::Seq)) const;

        //! Get the file type for the given extension.
        TL_API FileType getFileType(const std::string&) const;

        //! Create a video reader for the given path, or null when the format
        //! has no video or is decoded rather than read.
        TL_API std::shared_ptr<IVideoRead> videoRead(
            const ftk::Path&,
            const IOOptions& = IOOptions());

        //! Create a video reader for the given path and memory locations.
        TL_API std::shared_ptr<IVideoRead> videoRead(
            const ftk::Path&,
            const std::vector<ftk::MemFile>&,
            const IOOptions& = IOOptions());

        //! Create an audio reader for the given path, or null when the
        //! format has no audio.
        TL_API std::shared_ptr<IAudioRead> audioRead(
            const ftk::Path&,
            const IOOptions& = IOOptions());

        //! Create an audio reader for the given path and memory locations.
        TL_API std::shared_ptr<IAudioRead> audioRead(
            const ftk::Path&,
            const std::vector<ftk::MemFile>&,
            const IOOptions& = IOOptions());

    private:
        std::vector<std::shared_ptr<IReadPlugin> > _plugins;

        FTK_PRIVATE();
    };

    //! Write system.
    class TL_API_TYPE WriteSystem : public ftk::ISystem
    {
        FTK_NON_COPYABLE(WriteSystem);

    protected:
        WriteSystem(const std::shared_ptr<ftk::Context>&);

    public:
        TL_API virtual ~WriteSystem();

        //! Create a new system.
        TL_API static std::shared_ptr<WriteSystem> create(const std::shared_ptr<ftk::Context>&);

        //! Get the list of plugins.
        const std::vector<std::shared_ptr<IWritePlugin> >& getPlugins() const;

        //! Add a plugin.
        TL_API void addPlugin(const std::shared_ptr<IWritePlugin>&);

        //! Remove a plugin.
        TL_API void removePlugin(const std::shared_ptr<IWritePlugin>&);

        //! Get a plugin.
        template<typename T>
        std::shared_ptr<T> getPlugin() const;

        //! Get a plugin for the given path.
        TL_API std::shared_ptr<IWritePlugin> getPlugin(const ftk::Path&) const;

        //! Get the plugin names.
        TL_API const std::vector<std::string>& getNames() const;

        //! Get the supported file extensions.
        TL_API std::set<std::string> getExts(int types =
            static_cast<int>(FileType::Media) |
            static_cast<int>(FileType::Seq)) const;

        //! Get the file type for the given extension.
        TL_API FileType getFileType(const std::string&) const;

        //! Create a writer for the given path.
        TL_API std::shared_ptr<IWrite> write(
            const ftk::Path&,
            const IOInfo&,
            const IOOptions& = IOOptions());

    private:
        std::vector<std::shared_ptr<IWritePlugin> > _plugins;

        FTK_PRIVATE();
    };
}

#include <tlRender/IO/SystemInline.h>

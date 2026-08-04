// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/FileBrowser.h>
#include <ftk/UI/Settings.h>

namespace tl
{
    namespace play
    {
        //! This model provides settings that are saved and restored.
        //!
        //! The settings themselves belong to ftk::App, which reads and writes
        //! the file; this reads what it needs out of them when it is made and
        //! puts it back when it goes away.
        class SettingsModel : public std::enable_shared_from_this<SettingsModel>
        {
            FTK_NON_COPYABLE(SettingsModel);

        protected:
            SettingsModel(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ftk::Settings>&);

        public:
            ~SettingsModel();

            //! Create a new model.
            static std::shared_ptr<SettingsModel> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<ftk::Settings>&);

            //! Get the cache settings.
            const PlayerCacheOptions& getCache() const;

            //! Observe the cache settings.
            std::shared_ptr<ftk::IObservable<PlayerCacheOptions> > observeCache() const;

            //! Set the cache settings.
            void setCache(const PlayerCacheOptions&);

        private:
            std::shared_ptr<ftk::Settings> _settings;
            std::weak_ptr<ftk::FileBrowserSystem> _fileBrowserSystem;
            std::shared_ptr<ftk::Observable<PlayerCacheOptions> > _cache;
        };
    }
}

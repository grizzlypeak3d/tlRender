// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Player.h>

#include <ftk/UI/Label.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/Core/Timer.h>

namespace tl
{
    namespace play
    {
        class App;

        //! This widget displays errors and other information.
        class StatusBar : public ftk::IWidget
        {
            FTK_NON_COPYABLE(StatusBar);

        protected:
            void _init(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<IWidget>& parent);

            StatusBar() = default;

        public:
            ~StatusBar();

            static std::shared_ptr<StatusBar> create(
                const std::shared_ptr<ftk::Context>&,
                const std::shared_ptr<App>&,
                const std::shared_ptr<IWidget>& parent = nullptr);

            ftk::Size2I getSizeHint() const override;
            void setGeometry(const ftk::Box2I&) override;

        private:
            void _infoUpdate(const std::shared_ptr<Player>&);

            std::shared_ptr<ftk::HorizontalLayout> _layout;
            std::map<std::string, std::shared_ptr<ftk::Label> > _labels;
            std::shared_ptr<ftk::Timer> _messagesTimer;
            std::shared_ptr<ftk::ListObserver<ftk::LogItem> > _messagesObserver;
            std::shared_ptr<ftk::Observer<std::shared_ptr<Player> > > _playerObserver;
        };
    }
}

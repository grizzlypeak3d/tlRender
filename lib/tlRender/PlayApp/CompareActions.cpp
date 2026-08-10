// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include "CompareActions.h"

#include "App.h"
#include "FilesModel.h"

namespace tl
{
    namespace play
    {
        void CompareActions::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app)
        {
            auto appWeak = std::weak_ptr<App>(app);
            auto labels = getCompareLabels();
            const std::array<std::string, static_cast<size_t>(Compare::Count)> icons =
            {
                "CompareA",
                "CompareB",
                "CompareWipe",
                "CompareOverlay",
                "CompareDifference",
                "CompareHorizontal",
                "CompareVertical",
                "CompareTile",
            };
            const std::array<ftk::Key, static_cast<size_t>(Compare::Count)> shortcuts =
            {
                ftk::Key::A,
                ftk::Key::B,
                ftk::Key::W,
                ftk::Key::Unknown,
                ftk::Key::Unknown,
                ftk::Key::Unknown,
                ftk::Key::Unknown
            };
            const std::array<std::string, static_cast<size_t>(Compare::Count)> tooltips =
            {
                "Show the A file.",
                "Show the B file.",
                "Wipe between the A and B file.",
                "Overlay the A and B file.",
                "Show the difference between the A and B file.",
                "Show the A and B file side by side.",
                "Show the A and B file over and under.",
            };
            for (size_t i = 0; i < labels.size(); ++i)
            {
                _actions[labels[i]] = ftk::Action::create(
                    labels[i],
                    icons[i],
                    ftk::KeyShortcut(shortcuts[i], static_cast<int>(ftk::commandKeyModifier)),
                    [appWeak, i]
                    {
                        if (auto app = appWeak.lock())
                        {
                            app->getFilesModel()->setCompare(static_cast<Compare>(i));
                        }
                    });
                _actions[labels[i]]->setTooltip(tooltips[i]);
            }

            _compareGroup = ftk::ActionGroup::create(ftk::ActionGroupType::Radio);
            for (size_t i = 0; i < labels.size(); ++i)
            {
                _compareGroup->addAction(_actions[labels[i]]);
            }

            _playersObserver = ftk::ListObserver<std::shared_ptr<Player> >::create(
                app->getFilesModel()->observePlayers(),
                [this](const std::vector<std::shared_ptr<Player> >& value)
                {
                    for (const auto& label : getCompareLabels())
                    {
                        _actions[label]->setEnabled(value.size() > 0);
                    }
                });

            _compareObserver = ftk::Observer<Compare>::create(
                app->getFilesModel()->observeCompare(),
                [this](Compare value)
                {
                    _compareGroup->setChecked(static_cast<int>(value));
                });
        }

        CompareActions::~CompareActions()
        {
        }

        std::shared_ptr<CompareActions> CompareActions::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app)
        {
            auto out = std::shared_ptr<CompareActions>(new CompareActions);
            out->_init(context, app);
            return out;
        }

        const std::map<std::string, std::shared_ptr<ftk::Action> >& CompareActions::getActions() const
        {
            return _actions;
        }
    }
}
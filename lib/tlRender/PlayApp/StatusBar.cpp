// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include "StatusBar.h"

#include "App.h"
#include "FilesModel.h"

#include <ftk/UI/Divider.h>
#include <ftk/Core/Format.h>

namespace tl
{
    namespace play
    {
        void StatusBar::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init(context, "StatusBar", parent);

            _layout = ftk::HorizontalLayout::create(context, shared_from_this());
            _layout->setSpacingRole(ftk::SizeRole::SpacingSmall);

            _labels["Log"] = ftk::Label::create(context, _layout);
            _labels["Log"]->setHStretch(ftk::Stretch::Expanding);
            _labels["Log"]->setMarginRole(ftk::SizeRole::MarginInside);
            _labels["Log"]->setClipText(true);

            ftk::Divider::create(context, ftk::Orientation::Horizontal, _layout);
            _labels["Info"] = ftk::Label::create(context, _layout);
            _labels["Info"]->setMarginRole(ftk::SizeRole::MarginInside);
            _labels["Info"]->setClipText(true);

            _messagesTimer = ftk::Timer::create(context);

            _messagesObserver = ftk::ListObserver<ftk::LogItem>::create(
                app->getSysLogModel()->observeMessages(),
                [this](const std::vector<ftk::LogItem>& value)
                {
                    std::string text;
                    if (!value.empty())
                    {
                        text = ftk::getLabel(value.back(), ftk::LogLabel::Message);
                    }
                    _labels["Log"]->setText(text);
                    _labels["Log"]->setTooltip(text);
                    if (!value.empty())
                    {
                        _messagesTimer->start(
                            std::chrono::seconds(5),
                            [this]
                            {
                                _labels["Log"]->setText(std::string());
                                _labels["Log"]->setTooltip(std::string());
                            });
                    }
                });

            _playerObserver = ftk::Observer<std::shared_ptr<Player> >::create(
                app->getFilesModel()->observePlayer(),
                [this](const std::shared_ptr<Player>& value)
                {
                    _infoUpdate(value);
                });
        }

        StatusBar::~StatusBar()
        {}

        std::shared_ptr<StatusBar> StatusBar::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<StatusBar>(new StatusBar);
            out->_init(context, app, parent);
            return out;
        }

        ftk::Size2I StatusBar::getSizeHint() const
        {
            return _layout->getSizeHint();
        }

        void StatusBar::setGeometry(const ftk::Box2I& value)
        {
            IWidget::setGeometry(value);
            _layout->setGeometry(value);
        }

        void StatusBar::_infoUpdate(const std::shared_ptr<Player>& player)
        {
            std::vector<std::string> text;
            std::vector<std::string> tooltip;
            if (player)
            {
                const auto& path = player->getPath();
                text.push_back(path.getFileName());
                tooltip.push_back(path.get());

                const auto& ioInfo = player->getIOInfo();
                if (!ioInfo.video.empty())
                {
                    const auto& videoInfo = ioInfo.video.front();
                    const std::string s = ftk::Format("video: {0}").
                        arg(ftk::getLabel(videoInfo));
                    text.push_back(s);
                    tooltip.push_back(s);
                }

                if (ioInfo.audio.isValid())
                {
                    const std::string s = ftk::Format("audio: {0}").
                        arg(getLabel(ioInfo.audio, true));
                    text.push_back(s);
                    tooltip.push_back(s);
                }
            }
            _labels["Info"]->setText(ftk::join(text, ", "));
            _labels["Info"]->setTooltip(ftk::join(tooltip, "\n"));
        }
    }
}

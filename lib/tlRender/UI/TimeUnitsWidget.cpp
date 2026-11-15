// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/TimeUnitsWidget.h>

#include <ftk/UI/Action.h>
#include <ftk/UI/Menu.h>
#include <ftk/UI/ToolButton.h>

namespace tl
{
    namespace ui
    {
        struct TimeUnitsWidget::Private
        {
            std::shared_ptr<TimeUnitsModel> model;

            std::shared_ptr<ftk::ToolButton> button;
            std::shared_ptr<ftk::Menu> menu;
        };

        void TimeUnitsWidget::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<TimeUnitsModel>& model,
            const std::shared_ptr<IWidget>& parent)
        {
            IContainer::_init(context, "tl::ui::TimeUnitsWidget", parent);
            FTK_P();

            p.model = model;

            p.button = ftk::ToolButton::create(context);
            _setWidget(p.button);
            p.button->setIcon("Time");
            p.button->setPopupIcon(true);

            p.button->setClickedCallback(
                [this]
                {
                    FTK_P();
                    if (auto context = getContext())
                    {
                        if (!p.menu)
                        {
                            // The units, the one in use checked, and below
                            // them whether they count from the start. The
                            // actions do not close the menu: it closes itself
                            // after one, and closing it from the action freed
                            // it (the close callback lets go of it) while it
                            // was still handling the click.
                            p.menu = ftk::Menu::create(context);
                            auto weak = std::weak_ptr<TimeUnitsWidget>(
                                std::dynamic_pointer_cast<TimeUnitsWidget>(shared_from_this()));
                            const auto labels = getTimeUnitsLabels();
                            for (size_t i = 0; i < labels.size(); ++i)
                            {
                                const auto units = static_cast<TimeUnits>(i);
                                auto action = ftk::Action::create(
                                    labels[i],
                                    [weak, units](bool)
                                    {
                                        if (auto widget = weak.lock())
                                        {
                                            widget->_p->model->setTimeUnits(units);
                                        }
                                    });
                                p.menu->addAction(action);
                                p.menu->setChecked(action, units == p.model->getTimeUnits());
                            }
                            p.menu->addDivider();
                            auto startAtZero = ftk::Action::create(
                                "Start at Zero",
                                [weak](bool value)
                                {
                                    if (auto widget = weak.lock())
                                    {
                                        widget->_p->model->setStartAtZero(value);
                                    }
                                });
                            startAtZero->setTooltip(
                                "Count from the start: the first frame 0, or\n"
                                "00:00:00:00, rather than the frame or timecode\n"
                                "the media gives it.");
                            p.menu->addAction(startAtZero);
                            p.menu->setChecked(startAtZero, p.model->isStartAtZero());
                            p.menu->open(getWindow(), p.button->getGeometry());
                            p.menu->setCloseCallback(
                                [weak]
                                {
                                    if (auto widget = weak.lock())
                                    {
                                        widget->_p->menu.reset();
                                    }
                                });
                        }
                        else
                        {
                            p.menu->close();
                            p.menu.reset();
                        }
                    }
                });
        }

        TimeUnitsWidget::TimeUnitsWidget() :
            _p(new Private)
        {}

        TimeUnitsWidget::~TimeUnitsWidget()
        {}

        std::shared_ptr<TimeUnitsWidget> TimeUnitsWidget::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<TimeUnitsModel>& model,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<TimeUnitsWidget>(new TimeUnitsWidget);
            out->_init(context, model, parent);
            return out;
        }
    }
}

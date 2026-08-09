// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/TimeUnitsWidget.h>

#include <ftk/UI/ButtonGroup.h>
#include <ftk/UI/ComboBoxPrivate.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/UI/ToolButton.h>

namespace tl
{
    namespace ui
    {
        struct TimeUnitsWidget::Private
        {
            std::shared_ptr<TimeUnitsModel> model;

            std::shared_ptr<ftk::ToolButton> button;
            std::shared_ptr<ftk::ComboBoxMenu> menu;
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
                            std::vector<ftk::ComboBoxItem> items;
                            for (const auto& label : getTimeUnitsLabels())
                            {
                                items.push_back(ftk::ComboBoxItem(label));
                            }
                            p.menu = ftk::ComboBoxMenu::create(
                                context,
                                items,
                                static_cast<int>(p.model->getTimeUnits()));
                            p.menu->open(getWindow(), p.button->getGeometry());
                            auto weak = std::weak_ptr<TimeUnitsWidget>(
                                std::dynamic_pointer_cast<TimeUnitsWidget>(shared_from_this()));
                            p.menu->setCallback(
                                [weak](int index)
                                {
                                    if (auto widget = weak.lock())
                                    {
                                        widget->_p->menu->close();
                                        if (index != -1)
                                        {
                                            widget->_p->model->setTimeUnits(static_cast<TimeUnits>(index));
                                        }
                                    }
                                });
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

// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/TimeEdit.h>

#include <tlRender/Timeline/TimeUnits.h>

#include <ftk/UI/LineEdit.h>
#include <ftk/UI/LineEditModel.h>
#include <ftk/UI/IncButtons.h>
#include <ftk/UI/RowLayout.h>

namespace tl
{
    namespace ui
    {
        struct TimeEdit::Private
        {
            std::shared_ptr<TimeUnitsModel> timeUnitsModel;
            OTIO_NS::RationalTime value = invalidTime;
            std::function<void(const OTIO_NS::RationalTime&)> callback;
            std::shared_ptr<ftk::LineEdit> lineEdit;
            std::shared_ptr<ftk::IncButtons> incButtons;
            std::shared_ptr<ftk::HorizontalLayout> layout;

            std::shared_ptr<ftk::Observer<TimeUnits> > timeUnitsObserver;
        };

        void TimeEdit::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<TimeUnitsModel>& timeUnitsModel,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init(context, "tl::ui::TimeEdit", parent);
            FTK_P();

            p.timeUnitsModel = timeUnitsModel;
            if (!p.timeUnitsModel)
            {
                p.timeUnitsModel = TimeUnitsModel::create(context);
            }

            p.lineEdit = ftk::LineEdit::create(context, shared_from_this());
            p.lineEdit->getModel()->setRegex("[0-9\\-\\.\\,\\:]+");
            p.lineEdit->setFont(ftk::FontType::Mono);
            p.lineEdit->setHStretch(ftk::Stretch::Expanding);

            p.incButtons = ftk::IncButtons::create(context);

            p.layout = ftk::HorizontalLayout::create(context, shared_from_this());
            p.layout->setSpacingRole(ftk::SizeRole::SpacingTool);
            p.lineEdit->setParent(p.layout);
            p.incButtons->setParent(p.layout);

            _textUpdate();

            p.lineEdit->setCallback(
                [this](const std::string& value)
                {
                    _commitValue(value);
                });
            p.lineEdit->setFocusCallback(
                [this](bool value)
                {
                    if (!value)
                    {
                        _textUpdate();
                    }
                });

            p.incButtons->setIncCallback(
                [this]
                {
                    _commitValue(_p->value + OTIO_NS::RationalTime(1.0, _p->value.rate()));
                });
            p.incButtons->setDecCallback(
                [this]
                {
                    _commitValue(_p->value + OTIO_NS::RationalTime(-1.0, _p->value.rate()));
                });

            p.timeUnitsObserver = ftk::Observer<TimeUnits>::create(
                p.timeUnitsModel->observeTimeUnits(),
                [this](TimeUnits)
                {
                    _textUpdate();
                });
        }

        TimeEdit::TimeEdit() :
            _p(new Private)
        {}

        TimeEdit::~TimeEdit()
        {}

        std::shared_ptr<TimeEdit> TimeEdit::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<TimeUnitsModel>& timeUnitsModel,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<TimeEdit>(new TimeEdit);
            out->_init(context, timeUnitsModel, parent);
            return out;
        }

        const std::shared_ptr<TimeUnitsModel>& TimeEdit::getTimeUnitsModel() const
        {
            return _p->timeUnitsModel;
        }

        const OTIO_NS::RationalTime& TimeEdit::getValue() const
        {
            return _p->value;
        }

        void TimeEdit::setValue(const OTIO_NS::RationalTime& value)
        {
            FTK_P();
            if (value.strictly_equal(p.value))
                return;
            p.value = value;
            _textUpdate();
        }

        void TimeEdit::setCallback(const std::function<void(const OTIO_NS::RationalTime&)>& value)
        {
            _p->callback = value;
        }

        void TimeEdit::selectAll()
        {
            _p->lineEdit->selectAll();
        }

        void TimeEdit::setFont(ftk::FontType value)
        {
            _p->lineEdit->setFont(value);
        }
        
        ftk::Size2I TimeEdit::getSizeHint() const
        {
            return _p->layout->getSizeHint();
        }

        void TimeEdit::setGeometry(const ftk::Box2I& value)
        {
            IWidget::setGeometry(value);
            _p->layout->setGeometry(value);
        }

        void TimeEdit::takeKeyFocus()
        {
            _p->lineEdit->takeKeyFocus();
        }

        void TimeEdit::keyPressEvent(ftk::KeyEvent& event)
        {
            FTK_P();
            if (isEnabled() && 0 == event.modifiers)
            {
                switch (event.key)
                {
                case ftk::Key::Up:
                    event.accept = true;
                    _commitValue(
                        p.value +
                        OTIO_NS::RationalTime(1.0, p.value.rate()));
                    break;
                case ftk::Key::Down:
                    event.accept = true;
                    _commitValue(
                        p.value -
                        OTIO_NS::RationalTime(1.0, p.value.rate()));
                    break;
                case ftk::Key::PageUp:
                    event.accept = true;
                    _commitValue(
                        p.value +
                        OTIO_NS::RationalTime(p.value.rate(), p.value.rate()));
                    break;
                case ftk::Key::PageDown:
                    event.accept = true;
                    _commitValue(
                        p.value -
                        OTIO_NS::RationalTime(p.value.rate(), p.value.rate()));
                    break;
                default: break;
                }
            }
        }

        void TimeEdit::keyReleaseEvent(ftk::KeyEvent& event)
        {
            event.accept = true;
        }

        void TimeEdit::_commitValue(const std::string& value)
        {
            FTK_P();
            OTIO_NS::RationalTime tmp = invalidTime;
            opentime::ErrorStatus errorStatus;
            if (p.timeUnitsModel)
            {
                const TimeUnits timeUnits = p.timeUnitsModel->getTimeUnits();
                tmp = textToTime(
                    value,
                    p.value.rate(),
                    timeUnits,
                    &errorStatus);
            }
            const bool valid =
                isValid(tmp) &&
                !opentime::is_error(errorStatus);
            if (valid)
            {
                p.value = tmp;
            }
            _textUpdate();
            if (valid && p.callback)
            {
                p.callback(_p->value);
            }
        }

        void TimeEdit::_commitValue(const OTIO_NS::RationalTime& value)
        {
            FTK_P();
            p.value = value;
            _textUpdate();
            if (p.callback)
            {
                p.callback(p.value);
            }
        }

        void TimeEdit::_textUpdate()
        {
            FTK_P();
            std::string text;
            std::string format;
            if (p.timeUnitsModel)
            {
                const TimeUnits timeUnits = p.timeUnitsModel->getTimeUnits();
                text = timeToText(p.value, timeUnits);
                format = formatString(timeUnits);
            }
            p.lineEdit->setText(text);
            p.lineEdit->setFormat(format);
        }
    }
}

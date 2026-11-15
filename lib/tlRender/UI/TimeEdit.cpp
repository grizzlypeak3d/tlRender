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
            std::optional<OTIO_NS::RationalTime> value;
            std::optional<OTIO_NS::TimeRange> range;
            TimeMap timeMap;
            std::function<void(const OTIO_NS::RationalTime&)> callback;
            std::function<void(bool)> focusCallback;
            std::shared_ptr<ftk::LineEdit> lineEdit;
            std::shared_ptr<ftk::IncButtons> incButtons;
            std::shared_ptr<ftk::HorizontalLayout> layout;

            std::shared_ptr<ftk::Observer<bool> > timeUnitsObserver;
        };

        void TimeEdit::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<TimeUnitsModel>& timeUnitsModel,
            const std::shared_ptr<IWidget>& parent)
        {
            IContainer::_init(context, "tl::ui::TimeEdit", parent);
            FTK_P();

            p.timeUnitsModel = timeUnitsModel;
            if (!p.timeUnitsModel)
            {
                p.timeUnitsModel = TimeUnitsModel::create(context);
            }

            p.lineEdit = ftk::LineEdit::create(context, shared_from_this());
            p.lineEdit->setTextInputType(ftk::TextInputType::Number);
            p.lineEdit->setSelectAllOnFocus(true);
            p.lineEdit->getModel()->setRegex("[0-9\\-\\.\\,\\:]+");
            p.lineEdit->setFont(ftk::FontType::Mono);
            p.lineEdit->setHStretch(ftk::Stretch::Expanding);

            p.incButtons = ftk::IncButtons::create(context);

            p.layout = ftk::HorizontalLayout::create(context);
            _setWidget(p.layout);
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
                    if (_p->focusCallback)
                    {
                        _p->focusCallback(value);
                    }
                });

            // Stepping keeps the focus in the edit, as the keys do, rather
            // than the press sending it away; taken without selecting the
            // text, which the step rewrites.
            p.incButtons->setPressedCallback(
                [this]
                {
                    FTK_P();
                    if (!p.lineEdit->hasKeyFocus() && p.lineEdit->acceptsKeyFocus())
                    {
                        p.lineEdit->setSelectAllOnFocus(false);
                        p.lineEdit->takeKeyFocus();
                        p.lineEdit->setSelectAllOnFocus(true);
                    }
                });

            // Nothing to step from when the edit has no value.
            p.incButtons->setIncCallback(
                [this]
                {
                    if (_p->value.has_value())
                    {
                        _commitValue(*_p->value +
                            OTIO_NS::RationalTime(1.0, _p->value->rate()));
                    }
                });
            p.incButtons->setDecCallback(
                [this]
                {
                    if (_p->value.has_value())
                    {
                        _commitValue(*_p->value +
                            OTIO_NS::RationalTime(-1.0, _p->value->rate()));
                    }
                });

            // The units or whether timecode is drop frame.
            p.timeUnitsObserver = ftk::Observer<bool>::create(
                p.timeUnitsModel->observeTimeUnitsChanged(),
                [this](bool)
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

        const std::optional<OTIO_NS::RationalTime>& TimeEdit::getValue() const
        {
            return _p->value;
        }

        void TimeEdit::setValue(const std::optional<OTIO_NS::RationalTime>& value)
        {
            FTK_P();
            if (compareExact(value, p.value))
                return;
            p.value = value;
            _textUpdate();
        }

        void TimeEdit::setRange(const std::optional<OTIO_NS::TimeRange>& value)
        {
            FTK_P();
            if (value.has_value() == p.range.has_value() &&
                (!value.has_value() ||
                 (value->start_time().strictly_equal(p.range->start_time()) &&
                  value->duration().strictly_equal(p.range->duration()))))
                return;
            p.range = value;
            _textUpdate();
        }

        void TimeEdit::setTimeMap(const TimeMap& value)
        {
            FTK_P();
            p.timeMap = value;
            _textUpdate();
        }

        void TimeEdit::setCallback(const std::function<void(const OTIO_NS::RationalTime&)>& value)
        {
            _p->callback = value;
        }

        void TimeEdit::setFocusCallback(const std::function<void(bool)>& value)
        {
            _p->focusCallback = value;
        }

        void TimeEdit::setSelectAllOnFocus(bool value)
        {
            _p->lineEdit->setSelectAllOnFocus(value);
        }

        void TimeEdit::selectAll()
        {
            _p->lineEdit->selectAll();
        }

        void TimeEdit::setFont(ftk::FontType value)
        {
            _p->lineEdit->setFont(value);
        }

        void TimeEdit::setIncButtonsVisible(bool value)
        {
            _p->incButtons->setVisible(value);
        }

        void TimeEdit::setBorderRole(ftk::ColorRole value)
        {
            _p->lineEdit->setBorderRole(value);
        }

        void TimeEdit::setWellRole(ftk::ColorRole value)
        {
            _p->lineEdit->setWellRole(value);
        }
        
        

        

        void TimeEdit::takeKeyFocus()
        {
            _p->lineEdit->takeKeyFocus();
        }

        void TimeEdit::keyPressEvent(ftk::KeyEvent& event)
        {
            FTK_P();
            if (isEnabled() && 0 == event.modifiers && p.value.has_value())
            {
                switch (event.key)
                {
                case ftk::Key::Up:
                    event.accept = true;
                    _commitValue(
                        *p.value +
                        OTIO_NS::RationalTime(1.0, p.value->rate()));
                    break;
                case ftk::Key::Down:
                    event.accept = true;
                    _commitValue(
                        *p.value -
                        OTIO_NS::RationalTime(1.0, p.value->rate()));
                    break;
                case ftk::Key::PageUp:
                    event.accept = true;
                    _commitValue(
                        *p.value +
                        OTIO_NS::RationalTime(p.value->rate(), p.value->rate()));
                    break;
                case ftk::Key::PageDown:
                    event.accept = true;
                    _commitValue(
                        *p.value -
                        OTIO_NS::RationalTime(p.value->rate(), p.value->rate()));
                    break;
                default: break;
                }
            }
        }

        void TimeEdit::keyReleaseEvent(ftk::KeyEvent& event)
        {
            event.accept = true;
        }

        std::optional<OTIO_NS::RationalTime> TimeEdit::_mediaValue() const
        {
            return _toShown(_p->value);
        }

        std::optional<OTIO_NS::RationalTime> TimeEdit::_toShown(
            const std::optional<OTIO_NS::RationalTime>& value) const
        {
            FTK_P();
            // Counted from the start, the media's own numbering is what is
            // being set aside.
            if (p.timeUnitsModel && p.timeUnitsModel->isStartAtZero() && value.has_value())
            {
                return p.timeUnitsModel->toDisplay(*value);
            }
            return (p.timeMap.toMedia && value.has_value()) ?
                p.timeMap.toMedia(*value) :
                value;
        }

        void TimeEdit::_commitValue(const std::string& value)
        {
            FTK_P();
            std::optional<OTIO_NS::RationalTime> tmp;
            opentime::ErrorStatus errorStatus;
            const std::optional<OTIO_NS::RationalTime> mediaValue = _mediaValue();
            // Without a value there is no rate to read the text at, so there
            // is nothing to parse it into.
            if (p.timeUnitsModel && mediaValue.has_value())
            {
                const TimeUnits timeUnits = p.timeUnitsModel->getTimeUnits();
                // What was typed is in the media's time, so it is read at the
                // media's rate and taken back to the player's time.
                tmp = textToTime(
                    value,
                    mediaValue->rate(),
                    timeUnits,
                    &errorStatus);
                if (tmp.has_value() && p.timeUnitsModel->isStartAtZero())
                {
                    tmp = p.timeUnitsModel->fromDisplay(*tmp);
                }
                else if (tmp.has_value() && p.timeMap.fromMedia)
                {
                    tmp = p.timeMap.fromMedia(*tmp);
                }
            }
            const bool valid =
                tmp.has_value() &&
                !opentime::is_error(errorStatus);
            if (valid)
            {
                p.value = tmp;
            }
            _textUpdate();
            if (valid && p.callback)
            {
                p.callback(*_p->value);
            }
        }

        void TimeEdit::_commitValue(const OTIO_NS::RationalTime& value)
        {
            FTK_P();
            p.value = value;
            _textUpdate();
            if (p.callback)
            {
                p.callback(value);
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
                const bool dropFrame = p.timeUnitsModel->isDropFrame();
                text = timeToText(_mediaValue(), timeUnits, dropFrame);
                // The template for the units, widened to the longest of the
                // value and the ends of the range as they are shown: a long
                // clip, or timecode an hour or more in, has more frame
                // digits than the template (#959).
                format = formatString(timeUnits);
                std::vector<std::string> texts = { text };
                if (p.range.has_value())
                {
                    for (const auto& time : {
                        p.range->start_time(),
                        p.range->end_time_inclusive() })
                    {
                        texts.push_back(timeToText(_toShown(time), timeUnits, dropFrame));
                    }
                }
                for (const auto& i : texts)
                {
                    if (i.size() > format.size())
                    {
                        format = i;
                        for (auto& c : format)
                        {
                            if (c >= '1' && c <= '9')
                            {
                                c = '0';
                            }
                        }
                    }
                }
            }
            p.lineEdit->setText(text);
            p.lineEdit->setFormat(format);
        }
    }
}

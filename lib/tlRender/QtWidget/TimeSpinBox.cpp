// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/QtWidget/TimeSpinBox.h>

#include <tlRender/QtWidget/Util.h>

#include <QApplication>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QStyleOptionSpinBox>

namespace tl
{
    namespace qtwidget
    {
        struct TimeSpinBox::Private
        {
            std::optional<OTIO_NS::RationalTime> value;
            TimeUnits timeUnits = TimeUnits::Timecode;
            QRegularExpressionValidator* validator = nullptr;
            qt::TimeObject* timeObject = nullptr;
        };

        TimeSpinBox::TimeSpinBox(QWidget* parent) :
            QAbstractSpinBox(parent),
            _p(new Private)
        {
            const QFont fixedFont("Noto Mono");
            setFont(fixedFont);

            _vaidatorUpdate();
            _textUpdate();

            connect(
                lineEdit(),
                SIGNAL(returnPressed()),
                SLOT(_lineEditCallback()));
            connect(
                lineEdit(),
                SIGNAL(editingFinished()),
                SLOT(_lineEditCallback()));
        }

        TimeSpinBox::~TimeSpinBox()
        {}

        void TimeSpinBox::setTimeObject(qt::TimeObject* timeObject)
        {
            FTK_P();
            if (timeObject == p.timeObject)
                return;
            if (p.timeObject)
            {
                disconnect(
                    p.timeObject,
                    SIGNAL(timeUnitsChanged(tl::TimeUnits)),
                    this,
                    SLOT(setTimeUnits(tl::TimeUnits)));
            }
            p.timeObject = timeObject;
            if (p.timeObject)
            {
                p.timeUnits = p.timeObject->timeUnits();
                connect(
                    p.timeObject,
                    SIGNAL(timeUnitsChanged(tl::TimeUnits)),
                    SLOT(setTimeUnits(tl::TimeUnits)));
            }
            _vaidatorUpdate();
            _textUpdate();
            updateGeometry();
        }

        const std::optional<OTIO_NS::RationalTime>& TimeSpinBox::value() const
        {
            return _p->value;
        }

        TimeUnits TimeSpinBox::timeUnits() const
        {
            return _p->timeUnits;
        }

        void TimeSpinBox::stepBy(int steps)
        {
            FTK_P();
            // Nothing to step from when the box has no value.
            if (!p.value.has_value())
                return;
            p.value = *p.value + OTIO_NS::RationalTime(steps, p.value->rate());
            Q_EMIT valueChanged(*p.value);
            _textUpdate();
        }

        QValidator::State TimeSpinBox::validate(QString&, int& pos) const
        {
            return QValidator::Acceptable;
        }

        void TimeSpinBox::setValue(const std::optional<OTIO_NS::RationalTime>& value)
        {
            FTK_P();
            if (compareExact(value, p.value))
                return;
            p.value = value;
            if (p.value.has_value())
            {
                Q_EMIT valueChanged(*p.value);
            }
            _textUpdate();
        }

        void TimeSpinBox::setTimeUnits(TimeUnits value)
        {
            FTK_P();
            if (value == p.timeUnits)
                return;
            p.timeUnits = value;
            Q_EMIT timeUnitsChanged(p.timeUnits);
            _vaidatorUpdate();
            _textUpdate();
            updateGeometry();
        }

        QAbstractSpinBox::StepEnabled TimeSpinBox::stepEnabled() const
        {
            return QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
        }

        QSize TimeSpinBox::minimumSizeHint() const
        {
            FTK_P();
            ensurePolished();
            int h = lineEdit()->minimumSizeHint().height();
            const QFontMetrics fm(fontMetrics());
            const std::string s = " " + formatString(p.timeUnits);
            int w = fm.horizontalAdvance(QString::fromUtf8(s.c_str()));
            w += 2; // cursor blinking space
            QStyleOptionSpinBox opt;
            initStyleOption(&opt);
            QSize hint(w, h);
            return style()->sizeFromContents(QStyle::CT_SpinBox, &opt, hint, this);
        }

        void TimeSpinBox::_lineEditCallback()
        {
            FTK_P();
            // Without a value there is no rate to read the text at, so
            // there is nothing to parse it into.
            if (!p.value.has_value())
                return;
            opentime::ErrorStatus errorStatus;
            const std::optional<OTIO_NS::RationalTime> time = textToTime(
                lineEdit()->text().toUtf8().data(),
                p.value->rate(),
                p.timeUnits,
                &errorStatus);
            if (!opentime::is_error(errorStatus) &&
                time.has_value() &&
                *time != *p.value)
            {
                p.value = time;
                Q_EMIT valueChanged(*p.value);
            }
            _textUpdate();
        }

        void TimeSpinBox::_vaidatorUpdate()
        {
            FTK_P();
            if (p.validator)
            {
                p.validator->setParent(nullptr);
            }
            const std::string s = validator(p.timeUnits);
            p.validator = new QRegularExpressionValidator(
                QRegularExpression(QString::fromUtf8(s.c_str())),
                this);
            lineEdit()->setValidator(p.validator);
        }

        void TimeSpinBox::_textUpdate()
        {
            FTK_P();
            const std::string s = timeToText(p.value, p.timeUnits);
            lineEdit()->setText(QString::fromUtf8(s.c_str()));
        }
    }
}

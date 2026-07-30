// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/QtWidget/TimeLabel.h>

#include <tlRender/QtWidget/Util.h>

#include <QHBoxLayout>
#include <QLabel>

namespace tl
{
    namespace qtwidget
    {
        struct TimeLabel::Private
        {
            std::optional<OTIO_NS::RationalTime> value;
            TimeUnits timeUnits = TimeUnits::Timecode;
            QLabel* label = nullptr;
            qt::TimeObject* timeObject = nullptr;
        };

        TimeLabel::TimeLabel(QWidget* parent) :
            QWidget(parent),
            _p(new Private)
        {
            FTK_P();

            const QFont fixedFont("Noto Mono");
            setFont(fixedFont);

            p.label = new QLabel;

            auto layout = new QHBoxLayout;
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            layout->addWidget(p.label);
            setLayout(layout);

            _textUpdate();
        }

        TimeLabel::~TimeLabel()
        {}

        void TimeLabel::setTimeObject(qt::TimeObject* timeObject)
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
            _textUpdate();
            updateGeometry();
        }

        const std::optional<OTIO_NS::RationalTime>& TimeLabel::value() const
        {
            return _p->value;
        }

        TimeUnits TimeLabel::timeUnits() const
        {
            return _p->timeUnits;
        }

        void TimeLabel::setValue(const std::optional<OTIO_NS::RationalTime>& value)
        {
            FTK_P();
            if (compareExact(value, p.value))
                return;
            p.value = value;
            _textUpdate();
        }

        void TimeLabel::setTimeUnits(TimeUnits value)
        {
            FTK_P();
            if (value == p.timeUnits)
                return;
            p.timeUnits = value;
            _textUpdate();
            updateGeometry();
        }

        void TimeLabel::_textUpdate()
        {
            FTK_P();
            const std::string label = timeToText(p.value, p.timeUnits);
            p.label->setText(QString::fromUtf8(label.c_str()));
        }
    }
}

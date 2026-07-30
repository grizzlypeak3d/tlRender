// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/QtWidget/SpeedLabel.h>

#include <tlRender/QtWidget/Util.h>

#include <QHBoxLayout>
#include <QLabel>

namespace tl
{
    namespace qtwidget
    {
        struct SpeedLabel::Private
        {
            std::optional<OTIO_NS::RationalTime> value;
            QLabel* label = nullptr;
        };

        SpeedLabel::SpeedLabel(QWidget* parent) :
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

        SpeedLabel::~SpeedLabel()
        {}

        const std::optional<OTIO_NS::RationalTime>& SpeedLabel::value() const
        {
            return _p->value;
        }

        void SpeedLabel::setValue(const std::optional<OTIO_NS::RationalTime>& value)
        {
            FTK_P();
            if (compareExact(value, p.value))
                return;
            p.value = value;
            _textUpdate();
        }

        void SpeedLabel::_textUpdate()
        {
            FTK_P();
            p.label->setText(QString("%1").
                arg(p.value.has_value() ? p.value->rate() : 0.0, 0, 'f', 2));
        }
    }
}

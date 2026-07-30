// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Qt/TimeObject.h>

#include <QWidget>

#include <memory>

namespace tl
{
    namespace qtwidget
    {
        //! Speed label.
        class SpeedLabel : public QWidget
        {
            Q_OBJECT
            Q_PROPERTY(
                std::optional<OTIO_NS::RationalTime> value
                READ value
                WRITE setValue)

        public:
            SpeedLabel(QWidget* parent = nullptr);

            virtual ~SpeedLabel();

            //! Get the speed value, unset when the label has none.
            const std::optional<OTIO_NS::RationalTime>& value() const;

        public Q_SLOTS:
            //! Set the speed value.
            void setValue(const std::optional<OTIO_NS::RationalTime>&);

        private:
            void _textUpdate();

            FTK_PRIVATE();
        };
    }
}

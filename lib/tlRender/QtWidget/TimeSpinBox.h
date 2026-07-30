// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Qt/TimeObject.h>

#include <QAbstractSpinBox>

#include <memory>

namespace tl
{
    namespace qtwidget
    {
        //! Time spin box.
        class TimeSpinBox : public QAbstractSpinBox
        {
            Q_OBJECT
            Q_PROPERTY(
                std::optional<OTIO_NS::RationalTime> value
                READ value
                WRITE setValue
                NOTIFY valueChanged)
            Q_PROPERTY(
                tl::TimeUnits timeUnits
                READ timeUnits
                WRITE setTimeUnits
                NOTIFY timeUnitsChanged)

        public:
            TimeSpinBox(QWidget* parent = nullptr);

            virtual ~TimeSpinBox();

            //! Set the time object.
            void setTimeObject(qt::TimeObject*);

            //! Get the time value, unset when the box has none.
            const std::optional<OTIO_NS::RationalTime>& value() const;

            //! Get the time units.
            TimeUnits timeUnits() const;

            void stepBy(int steps) override;
            QValidator::State validate(QString&, int& pos) const override;
            QSize minimumSizeHint() const override;

        public Q_SLOTS:
            //! Set the time value.
            void setValue(const std::optional<OTIO_NS::RationalTime>&);

            //! Set the time units.
            void setTimeUnits(tl::TimeUnits);

        Q_SIGNALS:
            //! This signal is emitted when the time is changed.
            void valueChanged(const OTIO_NS::RationalTime&);

            //! This signal is emitted when the time units are changed.
            void timeUnitsChanged(tl::TimeUnits);

        protected:
            QAbstractSpinBox::StepEnabled stepEnabled() const override;

        private Q_SLOTS:
            void _lineEditCallback();

        private:
            void _vaidatorUpdate();
            void _textUpdate();

            FTK_PRIVATE();
        };
    }
}

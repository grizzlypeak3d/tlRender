// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include "SettingsWidget.h"

#include "App.h"
#include "SettingsModel.h"

#include <ftk/UI/FormLayout.h>
#include <ftk/UI/GroupBox.h>

namespace tl
{
    namespace play
    {
        void CacheSettingsWidget::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init(context, "CacheSettingsWidget", parent);

            _videoEdit = ftk::DoubleEdit::create(context);
            _videoEdit->setRange(0.0, 128.0);
            _videoEdit->setStep(1.0);
            _videoEdit->setLargeStep(10.0);

            _audioEdit = ftk::DoubleEdit::create(context);
            _audioEdit->setRange(0.0, 128.0);
            _audioEdit->setStep(1.0);
            _audioEdit->setLargeStep(10.0);

            _readBehindEdit = ftk::DoubleEdit::create(context);
            _readBehindEdit->setRange(0.0, 2.0);

            _layout = ftk::FormLayout::create(context, shared_from_this());
            _layout->setSpacingRole(ftk::SizeRole::SpacingSmall);
            _layout->addRow("Video cache (GB):", _videoEdit);
            _layout->addRow("Audio cache (GB):", _audioEdit);
            _layout->addRow("Read behind (seconds):", _readBehindEdit);

            std::weak_ptr<App> appWeak(app);
            _videoEdit->setCallback(
                [appWeak](double value)
                {
                    if (auto app = appWeak.lock())
                    {
                        auto cache = app->getSettingsModel()->getCache();
                        cache.videoGB = value;
                        app->getSettingsModel()->setCache(cache);
                    }
                });

            _audioEdit->setCallback(
                [appWeak](double value)
                {
                    if (auto app = appWeak.lock())
                    {
                        auto cache = app->getSettingsModel()->getCache();
                        cache.audioGB = value;
                        app->getSettingsModel()->setCache(cache);
                    }
                });

            _readBehindEdit->setCallback(
                [appWeak](double value)
                {
                    if (auto app = appWeak.lock())
                    {
                        auto cache = app->getSettingsModel()->getCache();
                        cache.readBehind = value;
                        app->getSettingsModel()->setCache(cache);
                    }
                });

            _cacheObserver = ftk::Observer<PlayerCacheOptions>::create(
                app->getSettingsModel()->observeCache(),
                [this](const PlayerCacheOptions& value)
                {
                    _videoEdit->setValue(value.videoGB);
                    _audioEdit->setValue(value.audioGB);
                    _readBehindEdit->setValue(value.readBehind);
                });
        }

        CacheSettingsWidget::~CacheSettingsWidget()
        {}

        std::shared_ptr<CacheSettingsWidget> CacheSettingsWidget::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<CacheSettingsWidget>(new CacheSettingsWidget);
            out->_init(context, app, parent);
            return out;
        }

        ftk::Size2I CacheSettingsWidget::getSizeHint() const
        {
            return _layout->getSizeHint();
        }

        void CacheSettingsWidget::setGeometry(const ftk::Box2I& value)
        {
            IWidget::setGeometry(value);
            _layout->setGeometry(value);
        }

        void FileBrowserSettingsWidget::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init(context, "FileBrowserSettingsWidget", parent);

            _nativeCheckBox = ftk::CheckBox::create(context);
            auto fileBrowserSystem = context->getSystem<ftk::FileBrowserSystem>();
            _nativeCheckBox->setChecked(fileBrowserSystem->isNativeFileDialog());

            _layout = ftk::FormLayout::create(context, shared_from_this());
            _layout->setSpacingRole(ftk::SizeRole::SpacingSmall);
            _layout->addRow("Native file browser:", _nativeCheckBox);

            _nativeCheckBox->setCheckedCallback(
                [this](bool value)
                {
                    auto fileBrowserSystem = getContext()->getSystem<ftk::FileBrowserSystem>();
                    fileBrowserSystem->setNativeFileDialog(value);
                });
        }

        FileBrowserSettingsWidget::~FileBrowserSettingsWidget()
        {}

        std::shared_ptr<FileBrowserSettingsWidget> FileBrowserSettingsWidget::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<FileBrowserSettingsWidget>(new FileBrowserSettingsWidget);
            out->_init(context, app, parent);
            return out;
        }

        ftk::Size2I FileBrowserSettingsWidget::getSizeHint() const
        {
            return _layout->getSizeHint();
        }

        void FileBrowserSettingsWidget::setGeometry(const ftk::Box2I& value)
        {
            IWidget::setGeometry(value);
            _layout->setGeometry(value);
        }

        void SettingsWidget::_init(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            IWidget::_init(context, "SettingsWidget", parent);
            _layout = ftk::VerticalLayout::create(context, shared_from_this());
            _layout->setMarginRole(ftk::SizeRole::MarginSmall);
            _layout->setSpacingRole(ftk::SizeRole::SpacingSmall);
            auto groupBox = ftk::GroupBox::create(context, "Cache", _layout);
            CacheSettingsWidget::create(context, app, groupBox);
            groupBox = ftk::GroupBox::create(context, "File Browser", _layout);
            FileBrowserSettingsWidget::create(context, app, groupBox);

            // The sections' labels share one width.
            ftk::setFormGroup(_layout, ftk::FormGroup::create());
        }

        SettingsWidget::~SettingsWidget()
        {}

        std::shared_ptr<SettingsWidget> SettingsWidget::create(
            const std::shared_ptr<ftk::Context>& context,
            const std::shared_ptr<App>& app,
            const std::shared_ptr<IWidget>& parent)
        {
            auto out = std::shared_ptr<SettingsWidget>(new SettingsWidget);
            out->_init(context, app, parent);
            return out;
        }

        ftk::Size2I SettingsWidget::getSizeHint() const
        {
            return _layout->getSizeHint();
        }

        void SettingsWidget::setGeometry(const ftk::Box2I& value)
        {
            IWidget::setGeometry(value);
            _layout->setGeometry(value);
        }
    }
}

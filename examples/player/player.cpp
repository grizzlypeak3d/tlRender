// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

// A minimal player: a viewport, a timeline, and transport controls.
// On the desktop it plays whatever the build's plugins read; on the
// web it plays movies through the WebCodecs plugin, and the shell
// forwards the page's query string as the command line, so
// "?url=clip.mp4" chooses the file.

#include <tlRender/UI/FrameToolBar.h>
#include <tlRender/UI/Init.h>
#include <tlRender/UI/PlaybackLoopWidget.h>
#include <tlRender/UI/PlaybackToolBar.h>
#include <tlRender/UI/TimeEdit.h>
#include <tlRender/UI/TimeLabel.h>
#include <tlRender/UI/TimeUnitsWidget.h>
#include <tlRender/UI/TimelineWidget.h>
#include <tlRender/UI/Viewport.h>

#include <tlRender/Timeline/Player.h>
#include <tlRender/Timeline/TimeUnits.h>

#include <tlRender/Core/Audio.h>

#include <ftk/UI/App.h>
#include <ftk/UI/Divider.h>
#include <ftk/UI/IWidgetPopup.h>
#include <ftk/UI/IntEditSlider.h>
#include <ftk/UI/ComboBoxPrivate.h>
#include <ftk/UI/Label.h>
#include <ftk/UI/Tooltip.h>
#include <ftk/UI/OverlayLayout.h>
#include <ftk/UI/RowLayout.h>
#include <ftk/UI/Spacer.h>
#include <ftk/UI/ToolButton.h>
#include <ftk/UI/Window.h>

#include <ftk/Core/CmdLine.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>
#include <ftk/Core/Timer.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/heap.h>
#include <malloc.h>
#endif

#include <atomic>
#include <cmath>
#include <iostream>
#include <thread>

using namespace ftk;

namespace
{
    //! The volume slider and the mute, in a popup under the volume
    //! button.
    class AudioPopup : public IWidgetPopup
    {
    protected:
        AudioPopup() = default;

    public:
        virtual ~AudioPopup() {}

        static std::shared_ptr<AudioPopup> create(
            const std::shared_ptr<Context>& context,
            const std::shared_ptr<tl::Player>& player,
            const std::shared_ptr<IWidget>& parent = nullptr)
        {
            auto out = std::shared_ptr<AudioPopup>(new AudioPopup);
            out->_init(context, "AudioPopup", parent);

            out->_volumeSlider = IntEditSlider::create(context);
            out->_volumeSlider->setRange(0, 100);
            out->_volumeSlider->setStep(1);
            out->_volumeSlider->setLargeStep(10);
            out->_volumeSlider->setTooltip("Audio volume");

            out->_muteButton = ToolButton::create(context);
            out->_muteButton->setIcon("Mute");
            out->_muteButton->setCheckable(true);
            out->_muteButton->setTooltip("Mute the audio");

            auto layout = HorizontalLayout::create(context);
            layout->setMarginRole(SizeRole::MarginSmall);
            layout->setSpacingRole(SizeRole::SpacingSmall);
            out->_volumeSlider->setParent(layout);
            out->_muteButton->setParent(layout);
            out->setWidget(layout);

            out->_volumeSlider->setCallback(
                [player](int value)
                {
                    player->setVolume(value / 100.F);
                });
            out->_muteButton->setCheckedCallback(
                [player](bool value)
                {
                    player->setMute(value);
                });

            auto* p = out.get();
            out->_volumeObserver = ftk::Observer<float>::create(
                player->observeVolume(),
                [p](float value)
                {
                    p->_volumeSlider->setValue(std::roundf(value * 100.F));
                });
            out->_muteObserver = ftk::Observer<bool>::create(
                player->observeMute(),
                [p](bool value)
                {
                    p->_muteButton->setChecked(value);
                });

            return out;
        }

    private:
        std::shared_ptr<IntEditSlider> _volumeSlider;
        std::shared_ptr<ToolButton> _muteButton;
        std::shared_ptr<ftk::Observer<float> > _volumeObserver;
        std::shared_ptr<ftk::Observer<bool> > _muteObserver;
    };

    //! Opening blocks on the readers' information, and on the web the
    //! WebCodecs reader cannot answer until the main thread's event
    //! loop runs -- so the open happens on a thread and the main
    //! thread polls for the result.
    struct Open
    {
        std::atomic<bool> done{ false };
        std::string error;
        std::shared_ptr<tl::Timeline> timeline;
        std::shared_ptr<tl::Player> player;
        std::shared_ptr<ftk::Observer<OTIO_NS::RationalTime> > currentTimeObserver;
        std::shared_ptr<ftk::Observer<float> > displayScaleObserver;
        std::shared_ptr<ftk::Observer<tl::Loop> > loopObserver;
        std::shared_ptr<ftk::ComboBoxMenu> scaleMenu;
        std::shared_ptr<ftk::Observer<float> > volumeObserver;
        std::shared_ptr<ftk::Observer<bool> > muteObserver;
        std::shared_ptr<AudioPopup> audioPopup;
        std::shared_ptr<ftk::Timer> scrubTimer;
        std::thread thread;
    };

    bool hasPopup(const std::shared_ptr<ftk::IWindow>& window)
    {
        bool out = false;
        for (const auto& child : window->getChildren())
        {
            if (std::dynamic_pointer_cast<ftk::IPopup>(child) &&
                !std::dynamic_pointer_cast<ftk::Tooltip>(child))
            {
                out = true;
                break;
            }
        }
        return out;
    }
}

int main(int argc, char** argv)
{
    try
    {
        auto context = Context::create();
        tl::ui::init(context);
        auto urlOption = CmdLineOption<std::string>::create(
            { "-url" },
            "The file to play.",
            "Player");
        auto autoscrubOption = CmdLineFlag::create(
            { "-autoscrub" },
            "Continuously seek to random times, for profiling.",
            "Player");
        auto playOption = CmdLineFlag::create(
            { "-play" },
            "Start playback once the file is open.",
            "Player");
        auto heapLogOption = CmdLineFlag::create(
            { "-heapLog" },
            "Log the heap size every ten seconds, for leak hunting.",
            "Player");
        auto noThumbnailsOption = CmdLineFlag::create(
            { "-noThumbnails" },
            "Disable the timeline thumbnails.",
            "Player");
        auto noWaveformsOption = CmdLineFlag::create(
            { "-noWaveforms" },
            "Disable the timeline waveforms.",
            "Player");
        auto overlayOption = CmdLineFlag::create(
            { "-overlay" },
            "Overlay the playback controls on the viewport (the "
            "phone layout).",
            "Player");
        auto app = App::create(
            context,
            argc,
            argv,
            "player",
            "Example player.",
            {},
            { urlOption, autoscrubOption, playOption, heapLogOption,
                noThumbnailsOption, noWaveformsOption, overlayOption });
        if (app->hasCmdLineHelp())
            return 0;

        bool mobile = false;
#if defined(__EMSCRIPTEN__)
        // A touch screen without a fine pointer is the phone shape.
        // The display scale is left to the device pixel ratio it
        // derives from; overriding it here made the phone SMALLER
        // (the ratio was already two).
        mobile = EM_ASM_INT({
            return (navigator.maxTouchPoints || 0) > 1 ? 1 : 0;
        });
#endif
        const bool overlayUI = overlayOption->found() || mobile;

        auto window = Window::create(context, app, "player");
        if (mobile)
        {
            // Touch has no hover: a tooltip would appear under the
            // resting cursor and never leave.
            window->setTooltipsEnabled(false);
        }
        auto layout = VerticalLayout::create(context, window);
        layout->setSpacingRole(SizeRole::None);
        auto overlayLayout = OverlayLayout::create(context, layout);
        overlayLayout->setVStretch(Stretch::Expanding);
        auto viewport = tl::ui::Viewport::create(context, overlayLayout);
        // The overlay gives the full box; the spacers and the cross
        // axis alignment are what center the label in it.
        auto loadingLayout = HorizontalLayout::create(context, overlayLayout);
        auto spacer = Spacer::create(
            context, Orientation::Horizontal, loadingLayout);
        spacer->setHStretch(Stretch::Expanding);
        auto loadingLabel = Label::create(context, "Loading", loadingLayout);
        loadingLabel->setVAlign(VAlign::Center);
        spacer = Spacer::create(
            context, Orientation::Horizontal, loadingLayout);
        spacer->setHStretch(Stretch::Expanding);
        // On a small screen the viewport keeps the whole window and
        // the controls ride the overlay, anchored to the bottom by
        // the expanding spacer, over a translucent scrim; on the
        // desktop they stack under the viewport as usual.
        std::shared_ptr<IWidget> controlsParent = layout;
        std::shared_ptr<VerticalLayout> controlsLayout;
        std::shared_ptr<HorizontalLayout> topLayout;
        std::shared_ptr<tl::ui::PlaybackLoopWidget> loopWidget;
        if (overlayUI)
        {
            auto anchorLayout = VerticalLayout::create(
                context, overlayLayout);
            anchorLayout->setSpacingRole(SizeRole::None);
            // The settings ride their own bar at the top, so the
            // transport row fits a phone at a large display scale.
            topLayout = HorizontalLayout::create(
                context, anchorLayout);
            topLayout->setBackgroundRole(ColorRole::Overlay);
            topLayout->setMarginRole(SizeRole::MarginInside);
            topLayout->setSpacingRole(SizeRole::SpacingSmall);
            auto topSpacer = Spacer::create(
                context, Orientation::Horizontal, topLayout);
            topSpacer->setHStretch(Stretch::Expanding);
            loopWidget = tl::ui::PlaybackLoopWidget::create(
                context, topLayout);
            auto vSpacer = Spacer::create(
                context, Orientation::Vertical, anchorLayout);
            vSpacer->setVStretch(Stretch::Expanding);
            controlsLayout = VerticalLayout::create(
                context, anchorLayout);
            controlsLayout->setSpacingRole(SizeRole::None);
            controlsLayout->setBackgroundRole(ColorRole::Overlay);
            controlsParent = controlsLayout;
        }
        auto timeUnitsModel = tl::TimeUnitsModel::create(context);
        auto timelineWidget = tl::ui::TimelineWidget::create(
            context, timeUnitsModel, controlsParent);
        if (noThumbnailsOption->found() || noWaveformsOption->found() ||
            overlayUI)
        {
            auto displayOptions = timelineWidget->getDisplayOptions();
            displayOptions.thumbnails = !noThumbnailsOption->found();
            displayOptions.waveforms = !noWaveformsOption->found();
            if (overlayUI)
            {
                // The scrim is the backing; a solid timeline would
                // defeat it.
                displayOptions.background = ColorRole::None;
            }
            timelineWidget->setDisplayOptions(displayOptions);
        }
        if (!overlayUI)
        {
            Divider::create(context, Orientation::Vertical, layout);
        }
        auto bottomLayout = HorizontalLayout::create(
            context, controlsParent);
        bottomLayout->setMarginRole(SizeRole::MarginInside);
        bottomLayout->setSpacingRole(SizeRole::SpacingSmall);
        auto transportLayout = HorizontalLayout::create(
            context, bottomLayout);
        transportLayout->setSpacingRole(SizeRole::SpacingTool);
        auto playbackToolBar = tl::ui::PlaybackToolBar::create(
            context, transportLayout);
        if (overlayUI)
        {
            playbackToolBar->setLoopVisible(false);
        }
        auto frameToolBar = tl::ui::FrameToolBar::create(
            context, transportLayout);
        auto timeEdit = tl::ui::TimeEdit::create(
            context, timeUnitsModel, bottomLayout);
        if (overlayUI)
        {
            timeEdit->setWellRole(ColorRole::None);
            timeEdit->setBorderRole(ColorRole::Text);
            // Too small for touch, and the frame controls are just
            // to the left.
            timeEdit->setIncButtonsVisible(false);
        }
        auto durationLabel = tl::ui::TimeLabel::create(
            context, timeUnitsModel, bottomLayout);
        auto settingsParent = overlayUI ?
            std::static_pointer_cast<IWidget>(topLayout) :
            std::static_pointer_cast<IWidget>(bottomLayout);
        auto timeUnitsWidget = tl::ui::TimeUnitsWidget::create(
            context, timeUnitsModel, settingsParent);
        if (!overlayUI)
        {
            spacer = Spacer::create(
                context, Orientation::Horizontal, bottomLayout);
            spacer->setHStretch(Stretch::Expanding);
        }
        auto volumeLabel = Label::create(context, settingsParent);
        volumeLabel->setFont(FontType::Mono);
        volumeLabel->setTooltip("Audio volume.");
        auto volumeButton = ToolButton::create(context, settingsParent);
        volumeButton->setIcon("Volume");
        volumeButton->setPopupIcon(true);
        volumeButton->setTooltip("Audio controls.");
        // Styled like the time units widget rather than a combo box:
        // a flat button and a popup menu, nothing solid behind it.
        const std::vector<float> displayScales =
            { 1.F, 2.F, 3.F, 4.F, 5.F, 6.F };
        const std::vector<std::string> displayScaleLabels =
            { "1x", "2x", "3x", "4x", "5x", "6x" };
        auto scaleIndex = std::make_shared<int>(0);
        auto scaleButton = ToolButton::create(context, settingsParent);
        scaleButton->setPopupIcon(true);
        scaleButton->setTooltip("Display scale.");
        // The status bar costs a row a phone does not have; in the
        // overlay layout it exists but never joins a window.
        std::shared_ptr<Label> statusLabel;
        if (!overlayUI)
        {
            Divider::create(context, Orientation::Vertical, layout);
            statusLabel = Label::create(context, layout);
        }
        else
        {
            statusLabel = Label::create(context);
        }
        statusLabel->setMarginRole(SizeRole::MarginInside);
        statusLabel->setHAlign(HAlign::Right);

        const std::string url =
            urlOption->found() ? urlOption->getValue() : "test.mp4";
        auto open = std::make_shared<Open>();
        open->thread = std::thread(
            [open, context, url]
            {
                try
                {
                    Path path(url);
                    const auto seq = findSeq(path);
                    if (!seq.empty())
                    {
                        path.setSeq(seq);
                    }
                    open->timeline = tl::Timeline::create(context, path);
                }
                catch (const std::exception& e)
                {
                    open->error = e.what();
                    std::cout << "ERROR: " << e.what() << std::endl;
                }
                open->done = true;
            });

#if defined(__EMSCRIPTEN__)
        auto heapTimer = Timer::create(context);
        if (heapLogOption->found())
        {
            heapTimer->setRepeating(true);
            heapTimer->start(
                std::chrono::seconds(10),
                [context, heapTimer]
                {
                    // The heap never shrinks, so the trend over a long
                    // run separates a leak from a bounded footprint.
                    // The beacon reaches the serving host's log, for
                    // reading a run without the browser console.
                    const struct mallinfo mi = mallinfo();
                    const std::string text = Format(
                        "heap: {0}MB, used: {1}MB").
                        arg(emscripten_get_heap_size() / 1048576).
                        arg(static_cast<size_t>(mi.uordblks) / 1048576);
                    context->getLogSystem()->print("player", text);
                    EM_ASM({
                        fetch('tlog?m=' + encodeURIComponent(
                            UTF8ToString($0)));
                    }, text.c_str());
                });
        }
#endif

        auto timer = Timer::create(context);
        timer->setRepeating(true);
        auto ticks = std::make_shared<int>(0);
        timer->start(
            std::chrono::milliseconds(100),
            [open, context, viewport, timelineWidget, playbackToolBar,
                frameToolBar, timeEdit, durationLabel, loadingLabel,
                volumeLabel, volumeButton, statusLabel, ticks, timer,
                autoscrubOption, playOption, mobile, loopWidget]
            {
                if (!open->done)
                {
                    // A remote open can take a while; the dots are the
                    // sign of life.
                    ++(*ticks);
                    loadingLabel->setText(
                        "Loading" + std::string(*ticks / 5 % 4, '.'));
                }
                if (open->done && !open->player)
                {
                    open->thread.join();
                    timer->stop();
                    if (open->timeline)
                    {
                        loadingLabel->hide();
                        tl::PlayerOptions playerOptions;
#if defined(__EMSCRIPTEN__)
                        // The desktop cache default would fill the
                        // heap, and the browser kills a page over its
                        // memory budget -- a phone's budget is a
                        // fraction of a desktop's.
                        // Small everywhere: the heap never shrinks,
                        // so scrubbing ratchets the footprint toward
                        // the limit -- a shallow cache lowers the
                        // ceiling it ratchets to. The depth lives in
                        // the worker's compressed block cache, which
                        // re-decodes instead of replaying.
                        playerOptions.cache.videoGB = mobile ? .1F : .25F;
                        playerOptions.cache.audioGB = mobile ? .05F : .1F;
#endif
                        open->player = tl::Player::create(
                            context, open->timeline, playerOptions);
                        viewport->setPlayer(open->player);
                        timelineWidget->setPlayer(open->player);
                        playbackToolBar->setPlayer(open->player);
                        frameToolBar->setPlayer(open->player);
                        timeEdit->setCallback(
                            [open](const OTIO_NS::RationalTime& value)
                            {
                                if (open->player)
                                {
                                    open->player->seek(value);
                                }
                            });
                        open->currentTimeObserver =
                            ftk::Observer<OTIO_NS::RationalTime>::create(
                                open->player->observeCurrentTime(),
                                [timeEdit](const OTIO_NS::RationalTime& value)
                                {
                                    timeEdit->setValue(value);
                                });
                        durationLabel->setValue(
                            open->player->getTimeRange().duration());
                        open->volumeObserver = ftk::Observer<float>::create(
                            open->player->observeVolume(),
                            [volumeLabel](float value)
                            {
                                volumeLabel->setText(Format("{0}%").arg(
                                    static_cast<int>(value * 100.F), 3));
                            });
                        open->muteObserver = ftk::Observer<bool>::create(
                            open->player->observeMute(),
                            [volumeButton](bool value)
                            {
                                volumeButton->setIcon(
                                    value ? "Mute" : "Volume");
                            });
                        std::vector<std::string> text;
                        text.push_back(
                            open->player->getPath().getFileName());
                        const auto& ioInfo = open->player->getIOInfo();
                        if (!ioInfo.video.empty())
                        {
                            text.push_back("video: " +
                                getLabel(ioInfo.video.front()));
                        }
                        if (ioInfo.audio.isValid())
                        {
                            text.push_back("audio: " +
                                tl::getLabel(ioInfo.audio, true));
                        }
                        statusLabel->setText(join(text, ", "));
                        if (loopWidget)
                        {
                            loopWidget->setCallback(
                                [open](tl::Loop value)
                                {
                                    if (open->player)
                                    {
                                        open->player->setLoop(value);
                                    }
                                });
                            open->loopObserver =
                                ftk::Observer<tl::Loop>::create(
                                    open->player->observeLoop(),
                                    [loopWidget](tl::Loop value)
                                    {
                                        loopWidget->setLoop(value);
                                    });
                        }
                        if (playOption->found())
                        {
                            open->player->setPlayback(
                                tl::Playback::Forward);
                        }
                        // Profiling harness: seek to a new time three
                        // times a second.
                        if (autoscrubOption->found())
                        {
                            open->scrubTimer = Timer::create(context);
                            open->scrubTimer->setRepeating(true);
                            auto n = std::make_shared<int>(0);
                            open->scrubTimer->start(
                                std::chrono::milliseconds(300),
                                [open, n]
                                {
                                    // The golden ratio spreads the
                                    // seeks over the timeline so each
                                    // one is a cold jump.
                                    const auto& range =
                                        open->player->getTimeRange();
                                    const double t = std::fmod(
                                        ++(*n) * .618, 1.0);
                                    const auto dur = range.duration();
                                    open->player->seek(
                                        range.start_time() +
                                        OTIO_NS::RationalTime(
                                            std::round(dur.value() * t),
                                            dur.rate()));
                                });
                        }
                    }
                    else
                    {
                        loadingLabel->setText(
                            !open->error.empty() ?
                            open->error :
                            "Cannot open");
                    }
                }
            });

        // The button follows the scale wherever it is set from -- the
        // device pixel ratio lands after startup on the web -- and
        // the nearest entry keeps the round trip stable.
        open->displayScaleObserver = Observer<float>::create(
            app->observeDisplayScale(),
            [scaleButton, scaleIndex, displayScales, displayScaleLabels]
                (float value)
            {
                int nearest = 0;
                for (size_t i = 1; i < displayScales.size(); ++i)
                {
                    if (std::abs(displayScales[i] - value) <
                        std::abs(displayScales[nearest] - value))
                    {
                        nearest = i;
                    }
                }
                *scaleIndex = nearest;
                scaleButton->setText(displayScaleLabels[nearest]);
            });
        scaleButton->setClickedCallback(
            [context, window, app, open, scaleButton, displayScales,
                displayScaleLabels, scaleIndex]
            {
                if (!open->scaleMenu)
                {
                    std::vector<ftk::ComboBoxItem> items;
                    for (const auto& label : displayScaleLabels)
                    {
                        items.push_back(ftk::ComboBoxItem(label));
                    }
                    open->scaleMenu = ftk::ComboBoxMenu::create(
                        context, items, *scaleIndex);
                    open->scaleMenu->open(
                        window, scaleButton->getGeometry());
                    open->scaleMenu->setCallback(
                        [app, open, displayScales](int index)
                        {
                            // The scale is set before the close: the
                            // close callback releases the menu, and
                            // this lambda lives in it.
                            if (index >= 0 &&
                                index < static_cast<int>(
                                    displayScales.size()))
                            {
                                app->setDisplayScale(
                                    displayScales[index]);
                            }
                            open->scaleMenu->close();
                        });
                    open->scaleMenu->setCloseCallback(
                        [open]
                        {
                            open->scaleMenu.reset();
                        });
                }
            });

        // The overlay controls get out of the way during playback:
        // pointer or touch activity shows them and resets the clock,
        // three quiet seconds hide them. A tap lands as motion first,
        // so it registers as activity.
        auto uiTimer = Timer::create(context);
        if (overlayUI)
        {
            uiTimer->setRepeating(true);
            auto lastPos = std::make_shared<V2I>();
            auto quiet = std::make_shared<int>(0);
            uiTimer->start(
                std::chrono::milliseconds(200),
                [window, controlsLayout, topLayout, open, lastPos, quiet,
                    uiTimer, mobile]
                {
                    const V2I pos = window->getCursorPos();
                    if (pos != *lastPos)
                    {
                        *lastPos = pos;
                        *quiet = 0;
                        controlsLayout->show();
                        topLayout->show();
                        if (!mobile)
                        {
                            window->setTooltipsEnabled(true);
                        }
                    }
                    else if (hasPopup(window))
                    {
                        // The user is in a menu or popup, so the
                        // clock holds and nothing hides out from
                        // under it. Tooltips do not count: on touch
                        // one would sit under the resting cursor
                        // forever.
                        *quiet = 0;
                    }
                    else if (open->player &&
                        tl::Playback::Stop !=
                            open->player->observePlayback()->get())
                    {
                        ++(*quiet);
                        if (*quiet >= 15)
                        {
                            controlsLayout->hide();
                            topLayout->hide();
                            // A tooltip must not outlive the widget
                            // it describes; disabling also closes an
                            // open one.
                            window->setTooltipsEnabled(false);
                        }
                    }
                });
        }

        volumeButton->setClickedCallback(
            [open, context, window, volumeButton]
            {
                if (open->player && !open->audioPopup)
                {
                    open->audioPopup = AudioPopup::create(
                        context, open->player);
                    open->audioPopup->open(
                        window, volumeButton->getGeometry());
                    open->audioPopup->setCloseCallback(
                        [open]
                        {
                            open->audioPopup.reset();
                        });
                }
            });

        app->run();
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

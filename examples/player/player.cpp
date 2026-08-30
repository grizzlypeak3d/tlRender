// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

// A minimal player: a viewport, a timeline, and transport controls.
// On the desktop it plays whatever the build's plugins read; on the
// web it plays movies through the WebCodecs plugin, and the shell
// forwards the page's query string as the command line, so
// "?url=clip.mp4" chooses the file.

#include <tlRender/UI/FrameToolBar.h>
#include <tlRender/UI/Init.h>
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
#include <ftk/UI/Label.h>
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
        std::shared_ptr<ftk::Observer<float> > volumeObserver;
        std::shared_ptr<ftk::Observer<bool> > muteObserver;
        std::shared_ptr<AudioPopup> audioPopup;
        std::shared_ptr<ftk::Timer> scrubTimer;
        std::thread thread;
    };
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
        auto app = App::create(
            context,
            argc,
            argv,
            "player",
            "Example player.",
            {},
            { urlOption, autoscrubOption });
        if (app->hasCmdLineHelp())
            return 0;

        auto window = Window::create(context, app, "player");
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
        auto timeUnitsModel = tl::TimeUnitsModel::create(context);
        auto timelineWidget = tl::ui::TimelineWidget::create(
            context, timeUnitsModel, layout);
        Divider::create(context, Orientation::Vertical, layout);
        auto bottomLayout = HorizontalLayout::create(context, layout);
        bottomLayout->setMarginRole(SizeRole::MarginInside);
        bottomLayout->setSpacingRole(SizeRole::SpacingSmall);
        auto transportLayout = HorizontalLayout::create(
            context, bottomLayout);
        transportLayout->setSpacingRole(SizeRole::SpacingTool);
        auto playbackToolBar = tl::ui::PlaybackToolBar::create(
            context, transportLayout);
        auto frameToolBar = tl::ui::FrameToolBar::create(
            context, transportLayout);
        auto timeEdit = tl::ui::TimeEdit::create(
            context, timeUnitsModel, bottomLayout);
        auto durationLabel = tl::ui::TimeLabel::create(
            context, timeUnitsModel, bottomLayout);
        auto timeUnitsWidget = tl::ui::TimeUnitsWidget::create(
            context, timeUnitsModel, bottomLayout);
        spacer = Spacer::create(
            context, Orientation::Horizontal, bottomLayout);
        spacer->setHStretch(Stretch::Expanding);
        auto volumeLabel = Label::create(context, bottomLayout);
        volumeLabel->setFont(FontType::Mono);
        volumeLabel->setTooltip("Audio volume.");
        auto volumeButton = ToolButton::create(context, bottomLayout);
        volumeButton->setIcon("Volume");
        volumeButton->setPopupIcon(true);
        volumeButton->setTooltip("Audio controls.");
        Divider::create(context, Orientation::Vertical, layout);
        auto statusLabel = Label::create(context, layout);
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

        auto timer = Timer::create(context);
        timer->setRepeating(true);
        auto ticks = std::make_shared<int>(0);
        timer->start(
            std::chrono::milliseconds(100),
            [open, context, viewport, timelineWidget, playbackToolBar,
                frameToolBar, timeEdit, durationLabel, loadingLabel,
                volumeLabel, volumeButton, statusLabel, ticks, timer,
                autoscrubOption]
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
                        const bool mobile = EM_ASM_INT({
                            return (navigator.maxTouchPoints || 0) > 1 ?
                                1 : 0;
                        });
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

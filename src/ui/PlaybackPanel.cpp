#include "PlaybackPanel.h"

#include "Icons.h"
#include "Palette.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>

namespace vdsa {

namespace {

QToolButton* makeButton(icons::Glyph glyph, const QString& tip, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setIcon(icons::make(glyph, palette::textPrimary));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tip);
    button->setAccessibleName(tip);
    button->setFixedSize(36, 32);
    button->setFocusPolicy(Qt::TabFocus);
    return button;
}

} // namespace

PlaybackPanel::PlaybackPanel(QWidget* parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("PlaybackBar"));

    previous_ = makeButton(icons::Glyph::Previous, tr("Previous step (Ctrl+Left)"), this);
    playPause_ = makeButton(icons::Glyph::Play, tr("Play / pause (Ctrl+Space)"), this);
    playPause_->setObjectName(QStringLiteral("PlayButton"));
    playPause_->setFixedSize(42, 34);
    next_ = makeButton(icons::Glyph::Next, tr("Next step (Ctrl+Right)"), this);
    restart_ = makeButton(icons::Glyph::Restart, tr("Restart playback from step 1"), this);

    auto* speedLabel = new QLabel(tr("SPEED"), this);
    speedLabel->setProperty("role", "section");
    speed_ = new QComboBox(this);
    speed_->setAccessibleName(tr("Playback speed"));
    for (double s : {0.5, 1.0, 2.0, 4.0})
        speed_->addItem(QStringLiteral("%1×").arg(s), s);
    speed_->setCurrentIndex(1);

    timeline_ = new QSlider(Qt::Horizontal, this);
    timeline_->setAccessibleName(tr("Timeline"));
    timeline_->setRange(0, 0);
    timeline_->setPageStep(1);

    stepLabel_ = new QLabel(this);
    stepLabel_->setObjectName(QStringLiteral("StepLabel"));
    stepLabel_->setFont(palette::monoFont(9.5));
    stepLabel_->setMinimumWidth(130);
    stepLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 10, 24, 10);
    layout->setSpacing(6);
    layout->addWidget(previous_);
    layout->addWidget(playPause_);
    layout->addWidget(next_);
    layout->addSpacing(4);
    layout->addWidget(restart_);
    layout->addSpacing(18);
    layout->addWidget(speedLabel);
    layout->addWidget(speed_);
    layout->addSpacing(18);
    layout->addWidget(timeline_, 1);
    layout->addSpacing(12);
    layout->addWidget(stepLabel_);

    connect(previous_, &QToolButton::clicked, this, &PlaybackPanel::previousRequested);
    connect(playPause_, &QToolButton::clicked, this, &PlaybackPanel::playPauseRequested);
    connect(next_, &QToolButton::clicked, this, &PlaybackPanel::nextRequested);
    connect(restart_, &QToolButton::clicked, this, &PlaybackPanel::restartRequested);
    connect(timeline_, &QSlider::valueChanged, this, &PlaybackPanel::seekRequested);
    connect(speed_, &QComboBox::currentIndexChanged, this,
            [this](int index) { emit speedRequested(speed_->itemData(index).toDouble()); });

    setState(0, 0, false);
}

void PlaybackPanel::setState(int step, int count, bool playing)
{
    const bool has = count > 0;
    previous_->setEnabled(has && step > 0);
    next_->setEnabled(has && step < count - 1);
    playPause_->setEnabled(count > 1);
    restart_->setEnabled(count > 1);
    timeline_->setEnabled(count > 1);

    if (playing != playing_ || !playPause_->property("init").isValid()) {
        playing_ = playing;
        playPause_->setProperty("init", true);
        playPause_->setIcon(icons::make(playing ? icons::Glyph::Pause : icons::Glyph::Play, palette::background));
    }
    {
        const QSignalBlocker block(timeline_); // programmatic moves must not emit seeks
        timeline_->setRange(0, std::max(0, count - 1));
        timeline_->setValue(step);
    }
    stepLabel_->setText(has ? tr("STEP %1 / %2").arg(step + 1, 2, 10, QLatin1Char('0')).arg(count, 2, 10, QLatin1Char('0'))
                            : tr("NO OPERATION YET"));
}

void PlaybackPanel::setSpeed(double speed)
{
    const int index = speed_->findData(speed);
    if (index >= 0) {
        const QSignalBlocker block(speed_);
        speed_->setCurrentIndex(index);
    }
}

} // namespace vdsa

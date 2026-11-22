// mini_player.cpp
//
// TruthGram detachable FFT mini media player.
//
// TruthGram - privacy-first, zero-telemetry Telegram Desktop client
// Copyright (C) 2026 TruthGram Contributors
#include "truthgram/ios_ui/mini_player.h"
#include "truthgram/theme/truthgram_theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include <QtMath>

#include <cmath>
#include <cstring>

namespace tg::truthgram::ios_ui {

namespace {

constexpr int kFftSize = 512;          // power of 2 for kiss_fft
constexpr int kRingCap = kFftSize * 4; // ~85 ms of mono at 48 kHz

} // namespace

struct MiniPlayer::Impl {
    QHBoxLayout* row{nullptr};
    QPushButton* prev{nullptr};
    QPushButton* play{nullptr};
    QPushButton* next{nullptr};
    QSlider* speedSlider{nullptr};
    QLabel*  speedLabel{nullptr};
    QPushButton* pitchLock{nullptr};

    // Lock-free ring buffer of mono samples for the FFT.
    std::atomic<int> writePos{0};
    std::vector<float> ring{kRingCap, 0.0f};

#if defined(TRUTHGRAM_HAVE_KISSFFT) && TRUTHGRAM_HAVE_KISSFFT
    // kiss_fftr_cfg is the real-input FFT plan; fftOut holds the kFftSize/2+1
    // complex bins it produces. We use kiss_fftr (not kiss_fft) because the
    // input is real float samples, so passing a float* is type-correct.
    kiss_fftr_cfg fftCfg{nullptr};
    std::vector<kiss_fft_cpx> fftOut;
#endif
    std::vector<float> magnitudes;

    QTimer* repaintTimer{nullptr};
    std::atomic<float> speed{1.0f};
    std::atomic<bool> pitchLock{false};
    std::atomic<bool> detached{false};
};

MiniPlayer::MiniPlayer(QWidget* parent)
    : QWidget(parent)
    , _i(std::make_unique<Impl>()) {
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMinimumHeight(56);
    setMaximumHeight(56);

#if defined(TRUTHGRAM_HAVE_KISSFFT) && TRUTHGRAM_HAVE_KISSFFT
    _i->fftCfg = kiss_fftr_alloc(kFftSize, 0, nullptr, nullptr);
    _i->fftOut.assign(kFftSize / 2 + 1, {0, 0});
#endif
    _i->magnitudes.assign(kFftSize / 2 + 1, 0.0f);

    _i->row = new QHBoxLayout(this);
    _i->row->setContentsMargins(12, 6, 12, 6);
    _i->row->setSpacing(8);

    _i->prev = new QPushButton(QStringLiteral("\u23EE"), this);
    _i->play = new QPushButton(QStringLiteral("\u23F8"), this);
    _i->next = new QPushButton(QStringLiteral("\u23ED"), this);
    _i->speedSlider = new QSlider(Qt::Horizontal, this);
    _i->speedSlider->setMinimum(50);
    _i->speedSlider->setMaximum(250);
    _i->speedSlider->setValue(100);
    _i->speedLabel = new QLabel(QStringLiteral("1.00x"), this);
    _i->pitchLock = new QPushButton(QStringLiteral("Pitch Lock"), this);
    _i->pitchLock->setCheckable(true);

    for (auto* b : {_i->prev, _i->play, _i->next, _i->pitchLock})
        b->setFixedSize(32, 32);
    _i->speedSlider->setFixedWidth(120);

    _i->row->addWidget(_i->prev);
    _i->row->addWidget(_i->play);
    _i->row->addWidget(_i->next);
    _i->row->addStretch(1);
    _i->row->addWidget(_i->speedSlider);
    _i->row->addWidget(_i->speedLabel);
    _i->row->addWidget(_i->pitchLock);

    const QColor accent = theme::theme().accent();
    const QColor bg = theme::theme().windowBg();
    const QString pill = QStringLiteral(
        "QPushButton{background:%1;color:%2;border:none;border-radius:14px;}"
        "QPushButton:hover{background:%3;}"
        "QSlider::groove:horizontal{height:3px;background:%1;border-radius:1px;}"
        "QSlider::handle:horizontal{background:%2;width:10px;margin:-4px 0;border-radius:5px;}"
        "QLabel{color:%2;}")
        .arg(bg.name(), accent.name(), theme::theme().accentDim().name());
    setStyleSheet(pill);

    connect(_i->speedSlider, &QSlider::valueChanged, this, [this](int v) {
        const float s = v / 100.0f;
        setSpeed(s);
        _i->speedLabel->setText(QStringLiteral("%1x").arg(s, 0, 'f', 2));
    });
    connect(_i->pitchLock, &QPushButton::toggled, this, [this](bool on) {
        setPitchLock(on);
    });
    connect(_i->next, &QPushButton::clicked, this, &MiniPlayer::nextRequested);
    connect(_i->prev, &QPushButton::clicked, this, &MiniPlayer::prevRequested);

    _i->repaintTimer = new QTimer(this);
    _i->repaintTimer->setInterval(33);   // ~30 fps for the waveform
    connect(_i->repaintTimer, &QTimer::timeout, this,
        QOverload<>::of(&MiniPlayer::update));
    _i->repaintTimer->start();
}

MiniPlayer::~MiniPlayer() {
#if defined(TRUTHGRAM_HAVE_KISSFFT) && TRUTHGRAM_HAVE_KISSFFT
    if (_i->fftCfg) kiss_fftr_free(_i->fftCfg);
#endif
}

void MiniPlayer::pushSamples(const float* mono, std::size_t n) {
    if (!mono || n == 0) return;
    for (std::size_t i = 0; i < n; ++i) {
        const int idx = _i->writePos.fetch_add(1, std::memory_order_relaxed) % kRingCap;
        _i->ring[std::size_t(idx)] = mono[i];
    }
}

void MiniPlayer::setSpeed(float ratio) {
    _i->speed.store(std::clamp(ratio, 0.5f, 2.5f), std::memory_order_release);
    Q_EMIT speedChanged(_i->speed.load());
}

void MiniPlayer::setPitchLock(bool on) {
    _i->pitchLock.store(on, std::memory_order_release);
    Q_EMIT pitchLockChanged(on);
}

float MiniPlayer::speed() const { return _i->speed.load(std::memory_order_acquire); }
bool MiniPlayer::pitchLock() const { return _i->pitchLock.load(std::memory_order_acquire); }

void MiniPlayer::detach() {
    if (_i->detached.exchange(true)) return;
    setParent(nullptr);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    show();
}

void MiniPlayer::reattach() {
    if (!_i->detached.exchange(false)) return;
    // Re-parent handled by the caller.
}

void MiniPlayer::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
}

void MiniPlayer::paintEvent(QPaintEvent*) {
#if defined(TRUTHGRAM_HAVE_KISSFFT) && TRUTHGRAM_HAVE_KISSFFT
    // Run a real FFT (kiss_fftr) over the most recent kFftSize samples.
    // kiss_fftr takes a const kiss_fft_scalar* (= float*) input, which matches
    // our real mono samples - using kiss_fft here would require a complex
    // input buffer and a (float* -> kiss_fft_cpx*) cast that does not compile.
    std::vector<float> in(kFftSize, 0.0f);
    const int start = _i->writePos.load(std::memory_order_relaxed) - kFftSize;
    for (int i = 0; i < kFftSize; ++i) {
        const int idx = ((start + i) % kRingCap + kRingCap) % kRingCap;
        in[i] = _i->ring[std::size_t(idx)];
        // Hann window.
        const float w = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * i / (kFftSize - 1)));
        in[i] *= w;
    }
    kiss_fftr(_i->fftCfg, in.data(), _i->fftOut.data());

    // Magnitude + smoothing.
    for (std::size_t i = 0; i < _i->magnitudes.size(); ++i) {
        const float re = _i->fftOut[i].r;
        const float im = _i->fftOut[i].i;
        const float mag = std::sqrt(re * re + im * im) / float(kFftSize);
        // Per-frame exponential smoothing so the bars don't flicker.
        _i->magnitudes[i] = 0.7f * _i->magnitudes[i] + 0.3f * mag;
    }
#else
    // Kiss FFT not available: magnitudes stay at zero (initialized in the
    // constructor). The waveform bars below render flat; the player controls
    // (prev / play / next / speed slider / pitch lock) are unaffected.
#endif

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& extras = theme::theme().extras();
    const QRectF body = rect().adjusted(2, 2, -2, -2);
    QPainterPath pill;
    pill.addRoundedRect(body, 18, 18);
    p.fillPath(pill, theme::theme().windowBg());
    QPen pen(theme::theme().accent());
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.drawPath(pill);

    // Draw FFT bars across the middle of the pill. Low bins -> Lime,
    // high bins -> Ice; we interpolate the hue across the spectrum.
    const QColor lime = extras.miniPlayerFftLow;
    const QColor ice  = extras.miniPlayerFftHigh;
    const int barAreaX = 110;
    const int barAreaW = width() - barAreaX - 180;
    const int bars = 48;
    const float barW = float(barAreaW) / bars;
    const int midY = height() / 2;
    for (int i = 0; i < bars; ++i) {
        const std::size_t bin = std::size_t(float(i) / bars * (_i->magnitudes.size() - 4));
        const float m = std::clamp(_i->magnitudes[bin] * 24.0f, 0.0f, 1.0f);
        const int h = int(m * (height() / 2 - 8));
        const QRectF r(barAreaX + i * barW + 1, midY - h, barW - 2, h * 2);
        // Interpolate lime -> ice across the spectrum.
        const float t = float(i) / bars;
        QColor c;
        c.setRedF  (lime.redF()   * (1 - t) + ice.redF()   * t);
        c.setGreenF(lime.greenF() * (1 - t) + ice.greenF() * t);
        c.setBlueF (lime.blueF()  * (1 - t) + ice.blueF()  * t);
        p.fillRect(r, c);
    }
}

void MiniPlayer::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && _i->detached.load()) {
        // Allow dragging the detached pill.
        _dragOffset = e->globalPos() - frameGeometry().topLeft();
        e->accept();
    } else {
        QWidget::mousePressEvent(e);
    }
}
void MiniPlayer::mouseMoveEvent(QMouseEvent* e) {
    if (_i->detached.load() && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPos() - _dragOffset);
        e->accept();
    } else {
        QWidget::mouseMoveEvent(e);
    }
}

} // namespace tg::truthgram::ios_ui

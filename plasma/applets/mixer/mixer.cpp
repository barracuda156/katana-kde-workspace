/*  This file is part of the KDE project
    Copyright (C) 2023 Ivailo Monev <xakepa10@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "mixer.h"

#include <QTimer>
#include <QApplication>
#include <QGridLayout>
#include <QGraphicsGridLayout>
#include <Plasma/TabBar>
#include <Plasma/Frame>
#include <Plasma/Label>
#include <Plasma/Slider>
#include <Plasma/IconWidget>
#include <Plasma/Theme>
#include <Plasma/SignalPlotter>
#include <Plasma/ToolTipManager>
#include <KIconLoader>
#include <KIcon>
#include <KDebug>

#include <alsa/asoundlib.h>

static const QSizeF s_minimumsize = QSizeF(290, 140);
static const QSizeF s_minimumslidersize = QSizeF(10, 70);
static const QSizeF s_minimumvisualizersize = QSizeF(120, 70);
static const int s_svgiconsize = 256;
static const QString s_defaultpopupicon = QString::fromLatin1("audio-card");
static const int s_defaultsoundcard = -1;
static const int s_alsapollinterval = 250;
// for butter-smooth visualization the poll interval is very frequent
static const int s_alsavisualizerinterval = 50;
// deciding factor for the visualization samples frequency
static const int s_alsapcmbuffersize = 256;
static const bool s_showvisualizer = true;
static const uint s_visualizerscale = 2;
static const bool s_visualizericon = false;

static QList<snd_mixer_selem_channel_id_t> kALSAChannelTypes(snd_mixer_elem_t *alsaelement, const bool capture)
{
    QList<snd_mixer_selem_channel_id_t> result;
    static const snd_mixer_selem_channel_id_t alsachanneltypes[] = {
        SND_MIXER_SCHN_FRONT_LEFT,
        SND_MIXER_SCHN_FRONT_RIGHT,
        SND_MIXER_SCHN_REAR_LEFT,
        SND_MIXER_SCHN_REAR_RIGHT,
        SND_MIXER_SCHN_FRONT_CENTER,
        SND_MIXER_SCHN_WOOFER,
        SND_MIXER_SCHN_SIDE_LEFT,
        SND_MIXER_SCHN_SIDE_RIGHT,
        SND_MIXER_SCHN_REAR_CENTER,
        SND_MIXER_SCHN_UNKNOWN
    };
    int counter = 0;
    while (alsachanneltypes[counter] != SND_MIXER_SCHN_UNKNOWN) {
        int alsaresult = 0;
        if (!capture) {
            alsaresult = snd_mixer_selem_has_playback_channel(alsaelement, alsachanneltypes[counter]);
        } else {
            alsaresult = snd_mixer_selem_has_capture_channel(alsaelement, alsachanneltypes[counter]);
        }
        if (alsaresult != 0) {
            result.append(alsachanneltypes[counter]);
        }
        counter++;
    }
    return result;
}

static bool kGetChannelVolumes(snd_mixer_elem_t *alsaelement, snd_mixer_selem_channel_id_t alsaelementchannel, const bool alsahascapture,
                               long *alsavolumemin, long *alsavolumemax, long *alsavolume)
{
    int alsaresult = 0;
    if (alsahascapture) {
        alsaresult = snd_mixer_selem_get_capture_volume_range(alsaelement, alsavolumemin, alsavolumemax);
        if (alsaresult != 0) {
            kWarning() << "Could not get capture channel volume range" << snd_strerror(alsaresult);
            return false;
        }
        alsaresult = snd_mixer_selem_get_capture_volume(alsaelement, alsaelementchannel, alsavolume);
        if (alsaresult != 0) {
            kWarning() << "Could not get capture channel volume" << snd_strerror(alsaresult);
            return false;
        }
        return true;
    }
    alsaresult = snd_mixer_selem_get_playback_volume_range(alsaelement, alsavolumemin, alsavolumemax);
    if (alsaresult != 0) {
        kWarning() << "Could not get playback channel volume range" << snd_strerror(alsaresult);
        return false;
    }
    alsaresult = snd_mixer_selem_get_playback_volume(alsaelement, alsaelementchannel, alsavolume);
    if (alsaresult != 0) {
        kWarning() << "Could not get playback channel volume" << snd_strerror(alsaresult);
        return false;
    }
    return true;
}

static bool kIsMasterElement(const QString &alsaelementname)
{
    return alsaelementname.contains(QLatin1String("master"), Qt::CaseInsensitive);
}

// for reference:
// alsa-lib/src/control/hcontrol.c
static QString kIconForElement(const QString &alsaelementname)
{
    if (kIsMasterElement(alsaelementname)) {
        return QString::fromLatin1("mixer-master");
    }
    if (alsaelementname.contains(QLatin1String("capture"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-capture");
    }
    if (alsaelementname.contains(QLatin1String("pcm"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-pcm");
    }
    if (alsaelementname.contains(QLatin1String("video"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-video");
    }
    if (alsaelementname.contains(QLatin1String("pc speaker"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-pc-speaker");
    }
    if (alsaelementname.contains(QLatin1String("mic"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-microphone");
    }
    if (alsaelementname.contains(QLatin1String("cd"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-cd");
    }
    if (alsaelementname.contains(QLatin1String("front"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-front");
    }
    if (alsaelementname.contains(QLatin1String("center"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-surround-center");
    }
    if (alsaelementname.contains(QLatin1String("surround"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-surround");
    }
    if (alsaelementname.contains(QLatin1String("lfe"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-lfe");
    }
    if (alsaelementname.contains(QLatin1String("headphone"), Qt::CaseInsensitive)) {
        return QString::fromLatin1("mixer-headset");
    }
    return QString::fromLatin1("mixer-line");
}

static int kFixedVolume(const long alsavolume, const long alsavolumemax)
{
    const qreal valuefactor = (qreal(alsavolumemax) / 100);
    return qRound(qreal(alsavolume) / valuefactor);
}

static int kVolumeStep()
{
    return qMax(QApplication::wheelScrollLines(), 1);
}

static QIcon kMixerIcon(QObject *parent, const int value)
{
    QIcon result;
    Plasma::Svg plasmasvg(parent);
    plasmasvg.setImagePath("icons/audio");
    plasmasvg.setContainsMultipleImages(true);
    if (plasmasvg.isValid()) {
        QPixmap iconpixmap(s_svgiconsize, s_svgiconsize);
        iconpixmap.fill(Qt::transparent);
        QPainter iconpainter(&iconpixmap);
        if (value >= 75) {
            plasmasvg.paint(&iconpainter, iconpixmap.rect(), "audio-volume-high");
        } else if (value >= 50) {
            plasmasvg.paint(&iconpainter, iconpixmap.rect(), "audio-volume-medium");
        } else if (value >= 25) {
            plasmasvg.paint(&iconpainter, iconpixmap.rect(), "audio-volume-low");
        } else {
            plasmasvg.paint(&iconpainter, iconpixmap.rect(), "audio-volume-muted");
        }
        result = QIcon(iconpixmap);
    } else {
        result = KIcon(s_defaultpopupicon);
    }
    return result;
}

static QColor kDefaultVisualizerColor()
{
    return Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
}

static QGraphicsWidget* kMakeSpacer(QGraphicsWidget *parent)
{
    QGraphicsWidget* result = new QGraphicsWidget(parent);
    result->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    result->setMinimumSize(0, 0);
    result->setFlag(QGraphicsItem::ItemHasNoContents);
    return result;
}

int k_alsa_element_callback(snd_mixer_elem_t *alsaelement, unsigned int alsamask);

class MixerSlider : public Plasma::Slider
{
    Q_OBJECT
public:
    explicit MixerSlider(const uint alsaelementindex,
                         const snd_mixer_selem_channel_id_t alsaelementchannel,
                         const QString &alsaelementname,
                         QGraphicsWidget *parent);


    void setVolume(const long alsavolumemin, const long alsavolumemax, const long alsavolume);
    long alsaVolume() const;

    long alsavolumemin;
    long alsavolumemax;
    long alsavolume;
    uint alsaelementindex;
    snd_mixer_selem_channel_id_t alsaelementchannel;
    QString alsaelementname;
};

MixerSlider::MixerSlider(const uint _alsaelementindex,
                         const snd_mixer_selem_channel_id_t _alsaelementchannel,
                         const QString &_alsaelementname,
                         QGraphicsWidget *parent)
    : Plasma::Slider(parent),
    alsavolumemin(0),
    alsavolumemax(0),
    alsavolume(0),
    alsaelementindex(_alsaelementindex),
    alsaelementchannel(_alsaelementchannel),
    alsaelementname(_alsaelementname)
{
    setOrientation(Qt::Vertical);
    setRange(0, 100);
    setValue(0);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    setMinimumSize(s_minimumslidersize);
    setToolTip(QString::fromLocal8Bit(snd_mixer_selem_channel_name(alsaelementchannel)));
}

void MixerSlider::setVolume(const long _alsavolumemin, const long _alsavolumemax, const long _alsavolume)
{
    alsavolumemin = _alsavolumemin;
    alsavolumemax = _alsavolumemax;
    alsavolume = _alsavolume;
    setValue(kFixedVolume(alsavolume, alsavolumemax));
}

long MixerSlider::alsaVolume() const
{
    const qreal valuefactor = (qreal(alsavolumemax) / 100);
    return (valuefactor * value());
}


class MixerPlotter : public Plasma::SignalPlotter
{
    Q_OBJECT
public:
    explicit MixerPlotter(const bool icon, QGraphicsWidget *parent);

private Q_SLOTS:
    void slotIconChanged();

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) final;

private:
    bool m_icon;
    KIcon m_plainicon;
    KIcon m_uncertainicon;
    KIcon m_sadicon;
    KIcon m_smileicon;
};

MixerPlotter::MixerPlotter(const bool icon, QGraphicsWidget *parent)
    : Plasma::SignalPlotter(parent),
    m_icon(icon)
{
    slotIconChanged();
    connect(
        KGlobalSettings::self(), SIGNAL(iconChanged(int)),
        this, SLOT(slotIconChanged())
    );
}

void MixerPlotter::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Plasma::SignalPlotter::paint(painter, option, widget);
    if (!m_icon) {
        return;
    }
    const QSizeF plottersize = size();
    const QSizeF quarterplottersize = (plottersize / 4);
    const int iconsize = qMin(quarterplottersize.width(), quarterplottersize.height());
    if (iconsize <= 16) {
        return;
    }
    const double lastplotvalue = lastValue(0);
    const QPointF pixmappoint = QPointF((plottersize.width() / 2) - (iconsize / 2), 10);
    if (lastplotvalue <= 0.05 && lastplotvalue >= -0.05) {
        painter->drawPixmap(pixmappoint, m_plainicon.pixmap(iconsize, iconsize));
    } else if (lastplotvalue >= -0.30 && lastplotvalue <= 0.30) {
        painter->drawPixmap(pixmappoint, m_uncertainicon.pixmap(iconsize, iconsize));
    } else if (lastplotvalue > 0.0) {
        painter->drawPixmap(pixmappoint, m_smileicon.pixmap(iconsize, iconsize));
    } else {
        painter->drawPixmap(pixmappoint, m_sadicon.pixmap(iconsize, iconsize));
    }
}

void MixerPlotter::slotIconChanged()
{
    m_plainicon = KIcon("face-plain");
    m_uncertainicon = KIcon("face-uncertain");
    m_sadicon = KIcon("face-sad");
    m_smileicon = KIcon("face-smile");
}


class MixerTabWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    MixerTabWidget(const bool isdefault, const QString &alsamixername, Plasma::TabBar *tabbar);
    ~MixerTabWidget();

    bool setup(const QByteArray &cardname);
    void showVisualizer(const bool show, const uint scale, const QColor &color, const bool icon);
    void pauseVisualizer(const bool pause);
     // can't be const because Plasma::Svg requires non-const parent
    QIcon mainVolumeIcon();
    Plasma::ToolTipContent toolTipContent() const;
    void decreaseVolume();
    void increaseVolume();

    // public for the callback
    QList<MixerSlider*> sliders;

Q_SIGNALS:
    void mainVolumeChanged();

private Q_SLOTS:
    void slotSliderMovedOrChanged(const int value);
    void slotEventsTimeout();
    void slotVisualizerTimeout();

private:
    QGraphicsLinearLayout* m_layout;
    snd_mixer_t* m_alsamixer;
    snd_pcm_t* m_alsapcm;
    QTimer* m_eventstimer;
    QTimer* m_visualizertimer;
    QGraphicsWidget* m_spacer;
    Plasma::Frame* m_plotterframe;
    MixerPlotter* m_signalplotter;
    float m_alsapcmbuffer[s_alsapcmbuffersize];
    bool m_isdefault;
    QString m_alsamixername;
    QString m_mainelement;
    QByteArray m_alsacardname;
};

MixerTabWidget::MixerTabWidget(const bool isdefault, const QString &alsamixername, Plasma::TabBar *tabbar)
    : QGraphicsWidget(tabbar),
    m_layout(nullptr),
    m_alsamixer(nullptr),
    m_alsapcm(nullptr),
    m_eventstimer(nullptr),
    m_visualizertimer(nullptr),
    m_spacer(nullptr),
    m_plotterframe(nullptr),
    m_signalplotter(nullptr),
    m_isdefault(isdefault),
    m_alsamixername(alsamixername)
{
    setMinimumSize(s_minimumsize);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    m_layout = new QGraphicsLinearLayout(Qt::Horizontal, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    m_eventstimer = new QTimer(this);
    m_eventstimer->setInterval(s_alsapollinterval);
    connect(
        m_eventstimer, SIGNAL(timeout()),
        this, SLOT(slotEventsTimeout())
    );

    m_visualizertimer = new QTimer(this);
    m_visualizertimer->setInterval(s_alsavisualizerinterval);
    connect(
        m_visualizertimer, SIGNAL(timeout()),
        this, SLOT(slotVisualizerTimeout())
    );
}

MixerTabWidget::~MixerTabWidget()
{
    m_eventstimer->stop();
    m_visualizertimer->stop();
    if (m_alsapcm) {
        snd_pcm_close(m_alsapcm);
    }
    if (m_alsamixer) {
        snd_mixer_close(m_alsamixer);
    }
}

bool MixerTabWidget::setup(const QByteArray &alsacardname)
{
    Q_ASSERT(m_alsamixer == nullptr);
    m_alsacardname = alsacardname;
    int alsaresult = snd_mixer_open(&m_alsamixer, 0);
    if (alsaresult != 0) {
        kWarning() << "Could not open mixer" << snd_strerror(alsaresult);
        return false;
    }
    alsaresult = snd_mixer_attach(m_alsamixer, alsacardname.constData());
    if (alsaresult != 0) {
        kWarning() << "Could not attach mixer" << snd_strerror(alsaresult);
        snd_mixer_close(m_alsamixer);
        m_alsamixer = nullptr;
        return false;
    }
    alsaresult = snd_mixer_selem_register(m_alsamixer, nullptr, nullptr);
    if (alsaresult != 0) {
        kWarning() << "Could not register mixer" << snd_strerror(alsaresult);
        snd_mixer_close(m_alsamixer);
        m_alsamixer = nullptr;
        return false;
    }
    alsaresult = snd_mixer_load(m_alsamixer);
    if (alsaresult != 0) {
        kWarning() << "Could not load mixer" << snd_strerror(alsaresult);
        snd_mixer_close(m_alsamixer);
        m_alsamixer = nullptr;
        return false;
    }

    const int smalliconsize = KIconLoader::global()->currentSize(KIconLoader::Small);
    const QSizeF smalliconsizef = QSizeF(smalliconsize, smalliconsize);
    bool hasvalidelement = false;
    QStringList alsaelementnames;
    snd_mixer_elem_t *alsaelement = snd_mixer_first_elem(m_alsamixer);
    for (; alsaelement; alsaelement = snd_mixer_elem_next(alsaelement)) {
        if (snd_mixer_elem_empty(alsaelement)) {
            continue;
        }
        const bool alsahasplayback = snd_mixer_selem_has_playback_volume(alsaelement);
        const bool alsahascapture = snd_mixer_selem_has_capture_volume(alsaelement);
        const uint alsaelementindex = snd_mixer_selem_get_index(alsaelement);
        const QString alsaelementname = QString::fromLocal8Bit(snd_mixer_selem_get_name(alsaelement));
        if (!alsahasplayback && !alsahascapture) {
            // no volume to mix
            kDebug() << "Skipping" << alsaelementindex << alsaelementname << "due to lack of volume";
            continue;
        }
        // qDebug() << Q_FUNC_INFO << alsaelementindex << alsaelementname << alsahasplayback << alsahascapture;

        const QList<snd_mixer_selem_channel_id_t> alsaelementchannels = kALSAChannelTypes(alsaelement, alsahascapture);
        if (alsaelementchannels.size() < 1) {
            kWarning() << "Element has no channels" << alsaelementindex << alsaelementname;
            continue;
        }

        hasvalidelement = true;
        Plasma::Frame* frame = new Plasma::Frame(this);
        frame->setFrameShadow(Plasma::Frame::Sunken);
        frame->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        frame->setText(alsaelementname);
        QGraphicsGridLayout* framelayout = new QGraphicsGridLayout(frame);
        int columncounter = 0;
        foreach (const snd_mixer_selem_channel_id_t alsaelementchannel, alsaelementchannels) {
            long alsavolumemin = 0;
            long alsavolumemax = 0;
            long alsavolume = 0;
            const bool gotvolumes = kGetChannelVolumes(
                alsaelement, alsaelementchannel, alsahascapture,
                &alsavolumemin, &alsavolumemax, &alsavolume
            );
            if (!gotvolumes) {
                continue;
            }
            const QString alsaelementchannelname = QString::fromLocal8Bit(snd_mixer_selem_channel_name(alsaelementchannel));
            MixerSlider* slider = new MixerSlider(alsaelementindex, alsaelementchannel, alsaelementname, frame);
            slider->setVolume(alsavolumemin, alsavolumemax, alsavolume);
            connect(
                slider, SIGNAL(sliderMoved(int)),
                this, SLOT(slotSliderMovedOrChanged(int))
            );
            connect(
                slider, SIGNAL(valueChanged(int)),
                this, SLOT(slotSliderMovedOrChanged(int))
            );
            sliders.append(slider);
            framelayout->addItem(slider, 0, columncounter, 1, 1);
            columncounter++;
        }
        Plasma::IconWidget* iconwidget = new Plasma::IconWidget(frame);
        iconwidget->setIcon(kIconForElement(alsaelementname));
        iconwidget->setAcceptHoverEvents(false);
        iconwidget->setMinimumIconSize(smalliconsizef);
        iconwidget->setMaximumIconSize(smalliconsizef);
        framelayout->addItem(iconwidget, 1, 0, 1, columncounter);
        framelayout->setAlignment(iconwidget, Qt::AlignCenter);
        frame->setLayout(framelayout);
        snd_mixer_elem_set_callback(alsaelement, k_alsa_element_callback);
        snd_mixer_elem_set_callback_private(alsaelement, this);

        m_layout->addItem(frame);
        columncounter++;

        if (m_mainelement.isEmpty() && kIsMasterElement(alsaelementname)) {
            m_mainelement = alsaelementname;
        } else {
            alsaelementnames.append(alsaelementname);
        }
    }
    if (m_mainelement.isEmpty() && alsaelementnames.size() > 0) {
        // pick the first as main if there is no master
        m_mainelement = alsaelementnames.first();
    }
    kDebug() << "Main element is" << m_mainelement;
    m_spacer = kMakeSpacer(this);
    m_layout->addItem(m_spacer);

    if (hasvalidelement) {
        m_eventstimer->start();
    }

    adjustSize();

    return hasvalidelement;
}

void MixerTabWidget::showVisualizer(const bool show, const uint scale, const QColor &color, const bool icon)
{
    const bool starttimer = m_visualizertimer->isActive();
    m_visualizertimer->stop();
    if (m_alsapcm) {
        snd_pcm_close(m_alsapcm);
        m_alsapcm = nullptr;
    }
    if (m_spacer) {
        m_layout->removeItem(m_spacer);
        m_spacer->deleteLater();
        m_spacer = nullptr;
    }
    if (m_signalplotter) {
        m_layout->removeItem(m_signalplotter);
        m_signalplotter->deleteLater();
        m_signalplotter = nullptr;
        m_layout->removeItem(m_plotterframe);
        m_plotterframe->deleteLater();
        m_plotterframe = nullptr;
    }

    if (!show) {
        if (starttimer) {
            m_visualizertimer->start();
        }
        m_spacer = kMakeSpacer(this);
        m_layout->addItem(m_spacer);
        return;
    }

    int alsaresult = snd_pcm_open(&m_alsapcm, m_alsacardname.constData(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if (alsaresult != 0) {
        kWarning() << "Could not open PCM" << snd_strerror(alsaresult);
        m_alsapcm = nullptr;
    } else {
        kDebug() << "Opened PCM" << m_alsacardname;
    }
    if (m_alsapcm) {
        alsaresult = snd_spcm_init(
            m_alsapcm,
            44100, // rate
            1, // channels
            SND_PCM_FORMAT_FLOAT,
            SND_PCM_SUBFORMAT_STD,
            SND_SPCM_LATENCY_STANDARD,
            SND_PCM_ACCESS_RW_INTERLEAVED,
            SND_SPCM_XRUN_STOP
        );
        if (alsaresult != 0) {
            kWarning() << "Could not init PCM" << snd_strerror(alsaresult);
            snd_pcm_close(m_alsapcm);
            m_alsapcm = nullptr;
        }
    }
    if (m_alsapcm) {
        alsaresult = snd_pcm_prepare(m_alsapcm);
        if (alsaresult != 0) {
            kWarning() << "Could not prepare PCM" << snd_strerror(alsaresult);
            snd_pcm_close(m_alsapcm);
            m_alsapcm = nullptr;
        }
    }
    if (m_alsapcm) {
        // has to be setup not to block
        alsaresult = snd_pcm_nonblock(m_alsapcm, 1);
        if (alsaresult != 0) {
            kWarning() << "Could not setup PCM for non-blocking" << snd_strerror(alsaresult);
            snd_pcm_close(m_alsapcm);
            m_alsapcm = nullptr;
        }
    }
    if (m_alsapcm) {
        m_plotterframe = new Plasma::Frame(this);
        m_plotterframe->setFrameShadow(Plasma::Frame::Sunken);
        m_plotterframe->setMinimumSize(s_minimumvisualizersize);
        m_plotterframe->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        QGraphicsLinearLayout* plotterframelayout = new QGraphicsLinearLayout(m_plotterframe);
        plotterframelayout->setContentsMargins(0, 0, 0, 0);
        m_plotterframe->setLayout(plotterframelayout);
        m_signalplotter = new MixerPlotter(icon, m_plotterframe);
        m_signalplotter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_signalplotter->setShowLabels(false);
        m_signalplotter->setShowVerticalLines(false);
        m_signalplotter->setShowHorizontalLines(false);
        m_signalplotter->setThinFrame(false);
        m_signalplotter->setHorizontalScale(scale);
        m_signalplotter->setUseAutoRange(false);
        // the documented range for SND_PCM_FORMAT_FLOAT
        m_signalplotter->setVerticalRange(-1.0, 1.0);
        m_signalplotter->addPlot(color);
        plotterframelayout->addItem(m_signalplotter);
        m_layout->addItem(m_plotterframe);

        m_visualizertimer->start();
    } else {
        m_spacer = kMakeSpacer(this);
        m_layout->addItem(m_spacer);
    }
}

void MixerTabWidget::pauseVisualizer(const bool pause)
{
    if (!m_alsapcm) {
        return;
    }
    if (pause) {
        kDebug() << "Pausing visualizer";
        m_visualizertimer->stop();
        const int alsaresult = snd_pcm_drop(m_alsapcm);
        if (alsaresult != 0) {
            kWarning() << "Could not drop PCM" << snd_strerror(alsaresult);
        }
    } else if (m_signalplotter) {
        kDebug() << "Unpausing visualizer";
        int alsaresult = snd_pcm_prepare(m_alsapcm);
        if (alsaresult != 0) {
            kWarning() << "Could not prepare PCM" << snd_strerror(alsaresult);
        }
        alsaresult = snd_pcm_start(m_alsapcm);
        if (alsaresult != 0) {
            kWarning() << "Could not start PCM" << snd_strerror(alsaresult);
        }
        m_visualizertimer->start();
    }
}

QIcon MixerTabWidget::mainVolumeIcon()
{
    if (m_mainelement.isEmpty()) {
        return KIcon(s_defaultpopupicon);
    }
    snd_mixer_elem_t *alsaelement = snd_mixer_first_elem(m_alsamixer);
    for (; alsaelement; alsaelement = snd_mixer_elem_next(alsaelement)) {
        if (snd_mixer_elem_empty(alsaelement)) {
            continue;
        }
        const QString alsaelementname = QString::fromLocal8Bit(snd_mixer_selem_get_name(alsaelement));
        if (alsaelementname != m_mainelement) {
            continue;
        }
        // the icon represents the highest volume from all element channels
        int alsaelementvolume = 0;
        const bool alsahascapture = snd_mixer_selem_has_capture_volume(alsaelement);
        const QList<snd_mixer_selem_channel_id_t> alsaelementchannels = kALSAChannelTypes(alsaelement, alsahascapture);
        foreach (const snd_mixer_selem_channel_id_t alsaelementchannel, alsaelementchannels) {
            long alsavolumemin = 0;
            long alsavolumemax = 0;
            long alsavolume = 0;
            const bool gotvolumes = kGetChannelVolumes(
                alsaelement, alsaelementchannel, alsahascapture,
                &alsavolumemin, &alsavolumemax, &alsavolume
            );
            if (!gotvolumes) {
                return KIcon(s_defaultpopupicon);
            }
            alsaelementvolume = qMax(alsaelementvolume, kFixedVolume(alsavolume, alsavolumemax));
        }
        return kMixerIcon(this, alsaelementvolume);
    }
    return KIcon(s_defaultpopupicon);
}

Plasma::ToolTipContent MixerTabWidget::toolTipContent() const
{
    Plasma::ToolTipContent result;
    const char* const tooltipiconname = (m_isdefault ? "mixer-pcm-default" : "mixer-pcm");
    result.setImage(
        KIconLoader::global()->loadIcon(
            QString::fromLatin1(tooltipiconname),
            KIconLoader::Dialog
        )
    );
    result.setMainText(QString::fromLatin1("<center>%1</center>").arg(m_alsamixername));
    result.setSubText(QString::fromLatin1("<center>%1</center>").arg(m_mainelement));
    return result;
}

void MixerTabWidget::decreaseVolume()
{
    foreach (MixerSlider *slider, sliders) {
        if (slider->alsaelementname == m_mainelement) {
            slider->setValue(qMax(slider->value() - kVolumeStep(), 0));
        }
    }
}
void MixerTabWidget::increaseVolume()
{
    foreach (MixerSlider *slider, sliders) {
        if (slider->alsaelementname == m_mainelement) {
            slider->setValue(qMax(slider->value() + kVolumeStep(), 100));
        }
    }
}

void MixerTabWidget::slotSliderMovedOrChanged(const int value)
{
    Q_ASSERT(m_alsamixer != nullptr);
    MixerSlider* slider = qobject_cast<MixerSlider*>(sender());
    Q_ASSERT(slider != nullptr);
    snd_mixer_elem_t *alsaelement = snd_mixer_first_elem(m_alsamixer);
    for (; alsaelement; alsaelement = snd_mixer_elem_next(alsaelement)) {
        if (snd_mixer_elem_empty(alsaelement)) {
            continue;
        }
        const uint alsaiterelementindex = snd_mixer_selem_get_index(alsaelement);
        const QString alsaiterelementname = QString::fromLocal8Bit(snd_mixer_selem_get_name(alsaelement));
        if (alsaiterelementindex == slider->alsaelementindex && alsaiterelementname == slider->alsaelementname) {
            kDebug() << "Changing" << slider->alsaelementindex << "volume to" << value;
            const bool alsahascapture = snd_mixer_selem_has_capture_volume(alsaelement);
            if (alsahascapture) {
                const int alsaresult = snd_mixer_selem_set_capture_volume(alsaelement, slider->alsaelementchannel, slider->alsaVolume());
                if (alsaresult != 0) {
                    kWarning() << "Could not set capture volume" << snd_strerror(alsaresult);
                    return;
                }
            } else {
                const int alsaresult = snd_mixer_selem_set_playback_volume(alsaelement, slider->alsaelementchannel, slider->alsaVolume());
                if (alsaresult != 0) {
                    kWarning() << "Could not set playback volume" << snd_strerror(alsaresult);
                    return;
                }
            }
            if (alsaiterelementname == m_mainelement) {
                emit mainVolumeChanged();
            }
            return;
        }
    }
    kWarning() << "Could not find the element" << slider->alsaelementindex;
}

void MixerTabWidget::slotEventsTimeout()
{
    if (Q_LIKELY(m_alsamixer)) {
        snd_mixer_handle_events(m_alsamixer);
    }
}

void MixerTabWidget::slotVisualizerTimeout()
{
    if (m_alsapcm) {
        switch (snd_pcm_state(m_alsapcm)) {
            case SND_PCM_STATE_PREPARED:
            case SND_PCM_STATE_RUNNING: {
                while (true) {
                    ::memset(m_alsapcmbuffer, 0, sizeof(m_alsapcmbuffer));
                    const int alsaresult = snd_pcm_readi(m_alsapcm, m_alsapcmbuffer, s_alsapcmbuffersize);
                    if (alsaresult < 1) {
                        if (errno == EAGAIN) {
                            // no data? snd_pcm_recover() does not handle it anyway
                            break;
                        }
                        // NOTE: this happens when the data is drained which is quite often because
                        // the visualization timer is 50ms
                        kWarning() << "Could not read PCM data" << snd_strerror(alsaresult);
                        snd_pcm_recover(m_alsapcm, alsaresult, 1);
                        break;
                    }
                    QList<double> alsapcmsamples;
                    alsapcmsamples.reserve(alsaresult);
                    for (int i = 0; i < alsaresult; i++) {
                        alsapcmsamples.append(double(m_alsapcmbuffer[i]));
                    }
                    // qDebug() << Q_FUNC_INFO << alsasamples;
                    m_signalplotter->addSample(alsapcmsamples);
                }
                break;
            }
            case SND_PCM_STATE_XRUN: {
                kWarning() << "PCM xrun" << m_alsamixername;
                break;
            }
            default: {
                break;
            }
        }
    }
}

// NOTE: this is called only on external events
int k_alsa_element_callback(snd_mixer_elem_t *alsaelement, unsigned int alsamask)
{
    // qDebug() << Q_FUNC_INFO << alsamask;
    if (alsamask & SND_CTL_EVENT_MASK_VALUE) {
        MixerTabWidget* mixertabwidget = static_cast<MixerTabWidget*>(snd_mixer_elem_get_callback_private(alsaelement));
        Q_ASSERT(mixertabwidget != nullptr);
        const uint alsaeventelementindex = snd_mixer_selem_get_index(alsaelement);
        const QString alsaeventelementname = QString::fromLocal8Bit(snd_mixer_selem_get_name(alsaelement));
        foreach (MixerSlider *slider, mixertabwidget->sliders) {
            if (alsaeventelementindex == slider->alsaelementindex && alsaeventelementname == slider->alsaelementname) {
                const bool alsahascapture = snd_mixer_selem_has_capture_volume(alsaelement);
                long alsavolumemin = 0;
                long alsavolumemax = 0;
                long alsavolume = 0;
                const bool gotvolumes = kGetChannelVolumes(
                    alsaelement, slider->alsaelementchannel, alsahascapture,
                    &alsavolumemin, &alsavolumemax, &alsavolume
                );
                if (!gotvolumes) {
                    continue;
                }
                slider->setVolume(alsavolumemin, alsavolumemax, alsavolume);
            }
        }
    }
    return 0;
}


class MixerWidget : public Plasma::TabBar
{
    Q_OBJECT
public:
    MixerWidget(MixerApplet *mixer);

    void showVisualizer(const bool show, const uint scale, const QColor &color, const bool icon);
    void pauseVisualizer(const bool pause);
    void decreaseVolume();
    void increaseVolume();

private Q_SLOTS:
    void slotMainVolumeChanged();
    void slotCurrentChanged(const int index);

private:
    MixerApplet* m_mixer;
    QList<MixerTabWidget*> m_tabwidgets;
};

MixerWidget::MixerWidget(MixerApplet* mixer)
    : Plasma::TabBar(mixer),
    m_mixer(mixer)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setMinimumSize(s_minimumsize);

    QStringList uniquemixers;
    int alsacard = s_defaultsoundcard;
    while (true) {
        int alsaresult = snd_card_next(&alsacard);
        if (alsaresult != 0) {
            kWarning() << "Could not get card" << snd_strerror(alsaresult);
            break;
        }

        const bool isdefault = (alsacard == s_defaultsoundcard);
        const QByteArray alsacardname = (isdefault ? "default" : "hw:" + QByteArray::number(alsacard));
        snd_ctl_t *alsactl = nullptr;
        alsaresult = snd_ctl_open(&alsactl, alsacardname.constData(), SND_CTL_NONBLOCK);
        if (alsaresult != 0) {
            kWarning() << "Could not open card" << snd_strerror(alsaresult);
            break;
        }

        snd_ctl_card_info_t *alsacardinfo = nullptr;
        snd_ctl_card_info_alloca(&alsacardinfo);
        alsaresult = snd_ctl_card_info(alsactl, alsacardinfo);
        if (alsaresult != 0) {
            kWarning() << "Could not open card" << snd_strerror(alsaresult);
            snd_ctl_close(alsactl);
            break;
        }

        const QString alsamixername = QString::fromLocal8Bit(snd_ctl_card_info_get_mixername(alsacardinfo));
        snd_ctl_close(alsactl);
        // default may be duplicate
        if (uniquemixers.contains(alsamixername)) {
            if (isdefault) {
                break;
            }
            alsacard++;
            continue;
        }
        uniquemixers.append(alsamixername);

        MixerTabWidget* mixertabwidget = new MixerTabWidget(isdefault, alsamixername, this);
        if (mixertabwidget->setup(alsacardname)) {
            if (isdefault) {
                // default sound card goes to the front
                insertTab(0, KIcon("mixer-pcm-default"), alsamixername, mixertabwidget);
                m_tabwidgets.prepend(mixertabwidget);
            } else {
                addTab(KIcon("mixer-pcm"), alsamixername, mixertabwidget);
                m_tabwidgets.append(mixertabwidget);
            }
            connect(
                mixertabwidget, SIGNAL(mainVolumeChanged()),
                this, SLOT(slotMainVolumeChanged())
            );
        } else {
            delete mixertabwidget;
        }

        // the loop is until -1 which happens to be the default card
        if (isdefault) {
            break;
        }
        alsacard++;
    }

    setTabBarShown(m_tabwidgets.size() > 1);
    setCurrentIndex(0);
    if (m_tabwidgets.size() > 0) {
        MixerTabWidget* firstmixertabwidget = m_tabwidgets.first();
        m_mixer->setStatus(Plasma::ItemStatus::ActiveStatus);
        m_mixer->setPopupIcon(firstmixertabwidget->mainVolumeIcon());
        Plasma::ToolTipManager::self()->setContent(m_mixer, firstmixertabwidget->toolTipContent());
    } else {
        Plasma::Label* label = new Plasma::Label(this);
        label->setMinimumSize(s_minimumsize);
        label->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
        label->setAlignment(Qt::AlignCenter);
        label->setText(i18n("No sound cards found"));
        addTab(QString::fromLatin1("nocards"), label);
        m_mixer->setStatus(Plasma::ItemStatus::PassiveStatus);
    }
    connect(
        this, SIGNAL(currentChanged(int)),
        this, SLOT(slotCurrentChanged(int))
    );
}

void MixerWidget::showVisualizer(const bool show, const uint scale, const QColor &color, const bool icon)
{
    foreach (MixerTabWidget *mixertabwidget, m_tabwidgets) {
        mixertabwidget->showVisualizer(show, scale, color, icon);
    }
}

void MixerWidget::pauseVisualizer(const bool pause)
{
    foreach (MixerTabWidget *mixertabwidget, m_tabwidgets) {
        mixertabwidget->pauseVisualizer(pause);
    }
}

void MixerWidget::decreaseVolume()
{
    if (m_tabwidgets.size() < 1) {
        return;
    }
    MixerTabWidget* mixertabwidget = m_tabwidgets.at(currentIndex());
    mixertabwidget->decreaseVolume();
}

void MixerWidget::increaseVolume()
{
    if (m_tabwidgets.size() < 1) {
        return;
    }
    MixerTabWidget* mixertabwidget = m_tabwidgets.at(currentIndex());
    mixertabwidget->increaseVolume();
}

void MixerWidget::slotMainVolumeChanged()
{
    MixerTabWidget* mixertabwidget = qobject_cast<MixerTabWidget*>(sender());
    if (m_tabwidgets.indexOf(mixertabwidget) == currentIndex()) {
        m_mixer->setPopupIcon(mixertabwidget->mainVolumeIcon());
    }
}

void MixerWidget::slotCurrentChanged(const int index)
{
    Q_ASSERT(index < m_tabwidgets.size());
    MixerTabWidget* mixertabwidget = m_tabwidgets.at(index);
    Q_ASSERT(mixertabwidget != nullptr);
    m_mixer->setPopupIcon(mixertabwidget->mainVolumeIcon());
    Plasma::ToolTipManager::self()->setContent(m_mixer, mixertabwidget->toolTipContent());
}


MixerApplet::MixerApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_mixerwidget(nullptr),
    m_showvisualizer(s_showvisualizer),
    m_visualizerscale(s_visualizerscale),
    m_visualizericon(s_visualizericon),
    m_visualizerbox(nullptr),
    m_visualizerscalelabel(nullptr),
    m_visualizerscalebox(nullptr),
    m_visualizerbutton(nullptr),
    m_spacer(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_mixer");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setPopupIcon(kMixerIcon(this, 0));
}

MixerApplet::~MixerApplet()
{
    delete m_mixerwidget;
}

void MixerApplet::init()
{
    Plasma::PopupApplet::init();

    m_mixerwidget = new MixerWidget(this);
    slotThemeChanged();
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(slotThemeChanged()));
}

void MixerApplet::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget* widget = new QWidget();
    QGridLayout* widgetlayout = new QGridLayout(widget);
    m_visualizerbox = new QCheckBox(widget);
    m_visualizerbox->setChecked(m_showvisualizer);
    m_visualizerbox->setText(i18n("Show visualizer"));
    widgetlayout->addWidget(m_visualizerbox, 0, 0, 1, 2);

    m_visualizerscalelabel = new QLabel(widget);
    m_visualizerscalelabel->setText(i18n("Smooth-factor"));
    widgetlayout->addWidget(m_visualizerscalelabel, 1, 0, 1, 1, Qt::AlignRight | Qt::AlignVCenter);

    m_visualizerscalebox = new KIntNumInput(widget);
    m_visualizerscalebox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    m_visualizerscalebox->setRange(1, 5);
    m_visualizerscalebox->setValue(m_visualizerscale);
    widgetlayout->addWidget(m_visualizerscalebox, 1, 1, 1, 1);

    const QColor defaultvisualizercolor = kDefaultVisualizerColor();
    QColor visualizercolor = m_visualizercolor;
    if (!visualizercolor.isValid()) {
        visualizercolor = defaultvisualizercolor;
    }
    m_visualizerbutton = new KColorButton(widget);
    m_visualizerbutton->setDefaultColor(defaultvisualizercolor);
    m_visualizerbutton->setColor(visualizercolor);
    widgetlayout->addWidget(m_visualizerbutton, 2, 0, 1, 2);

    m_visualizericonbox = new QCheckBox(widget);
    m_visualizericonbox->setChecked(m_visualizericon);
    m_visualizericonbox->setText(i18n("Show icon"));
    widgetlayout->addWidget(m_visualizericonbox, 3, 0, 1, 2);

    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addItem(m_spacer, 4, 0, 1, 2);

    widget->setLayout(widgetlayout);
    parent->addPage(widget, i18n("Visualizer"), "player-volume");

    slotVisualizerToggled(m_showvisualizer);
    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_visualizerbox, SIGNAL(toggled(bool)), this, SLOT(slotVisualizerToggled(bool)));
    connect(m_visualizerbox, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
    connect(m_visualizerscalebox, SIGNAL(valueChanged(int)), parent, SLOT(settingsModified()));
    connect(m_visualizerbutton, SIGNAL(changed(QColor)), parent, SLOT(settingsModified()));
    connect(m_visualizericonbox, SIGNAL(toggled(bool)), parent, SLOT(settingsModified()));
}

QGraphicsWidget* MixerApplet::graphicsWidget()
{
    return m_mixerwidget;
}

void MixerApplet::popupEvent(bool show)
{
    if (!m_mixerwidget) {
        return;
    }
    m_mixerwidget->pauseVisualizer(!show);
}

void MixerApplet::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    if (!m_mixerwidget) {
        return;
    }
    if (event->delta() < 0) {
        m_mixerwidget->decreaseVolume();
    } else {
        m_mixerwidget->increaseVolume();
    }
}

void MixerApplet::slotVisualizerToggled(bool toggled)
{
    m_visualizerscalebox->setEnabled(toggled);
    m_visualizerbutton->setEnabled(toggled);
    m_visualizericonbox->setEnabled(toggled);
}

void MixerApplet::slotConfigAccepted()
{
    Q_ASSERT(m_mixerwidget != nullptr);
    Q_ASSERT(m_visualizerbox != nullptr);
    Q_ASSERT(m_visualizerscalebox != nullptr);
    Q_ASSERT(m_visualizerbutton != nullptr);
    Q_ASSERT(m_visualizericonbox != nullptr);
    m_showvisualizer = m_visualizerbox->isChecked();
    m_visualizerscale = m_visualizerscalebox->value();
    m_visualizercolor = m_visualizerbutton->color();
    m_visualizericon = m_visualizericonbox->isChecked();
    KConfigGroup configgroup = config();
    configgroup.writeEntry("showVisualizer", m_showvisualizer);
    configgroup.writeEntry("visualizerScale", m_visualizerscale);
    if (m_visualizercolor == kDefaultVisualizerColor()) {
        configgroup.writeEntry("visualizerColor", QColor());
    } else {
        configgroup.writeEntry("visualizerColor", m_visualizercolor);
    }
    configgroup.writeEntry("visualizerIcon", m_visualizericon);
    emit configNeedsSaving();
    m_mixerwidget->showVisualizer(m_showvisualizer, m_visualizerscale, m_visualizercolor, m_visualizericon);
    m_mixerwidget->pauseVisualizer(isIconified() && !isPopupShowing());
}

void MixerApplet::slotThemeChanged()
{
    Q_ASSERT(m_mixerwidget != nullptr);
    KConfigGroup configgroup = config();
    m_showvisualizer = configgroup.readEntry("showVisualizer", s_showvisualizer);
    m_visualizerscale = configgroup.readEntry("visualizerScale", s_visualizerscale);
    m_visualizercolor = configgroup.readEntry("visualizerColor", QColor());
    if (!m_visualizercolor.isValid()) {
        m_visualizercolor = kDefaultVisualizerColor();
    }
    m_visualizericon = configgroup.readEntry("visualizerIcon", s_visualizericon);

    m_mixerwidget->showVisualizer(m_showvisualizer, m_visualizerscale, m_visualizercolor, m_visualizericon);
    m_mixerwidget->pauseVisualizer(isIconified() && !isPopupShowing());
}

K_EXPORT_PLASMA_APPLET(mixer, MixerApplet)

#include "moc_mixer.cpp"
#include "mixer.moc"

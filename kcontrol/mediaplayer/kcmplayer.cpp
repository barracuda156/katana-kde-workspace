/*  This file is part of the KDE libraries
    Copyright (C) 2016 Ivailo Monev <xakepa10@gmail.com>

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

#include <QCoreApplication>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <knuminput.h>
#include <kaboutdata.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>
#include <kdebug.h>
#include <kstandarddirs.h>
#include <kdesktopfile.h>
#include <kconfiggroup.h>
#include <kservice.h>
#include <kicon.h>

#include "kcmplayer.h"

// NOTE: keep in sync with:
// kdelibs/kutils/kmediaplayer/kmediaplayer.cpp
static const QString s_kmediaoutput = QString::fromLatin1("auto");
static const bool s_kmediamute = false;
static const int s_kmediavolume = 90;

class KMediaBox : public QGroupBox
{
    Q_OBJECT
public:
    KMediaBox(QWidget *parent,
              const QString &id, const QString &description,
              const QString &output, bool mute, int volume,
              const QStringList &audiooutputs);

public:
    QString id() const;
    QString output() const;
    bool mute() const;
    int volume() const;

    void setDefault();

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void slotOutput();
    void slotMute();
    void slotVolume();

private:
    void setOutput(const QString &output);

    QString m_id;
    QComboBox* m_outputbox;
    QCheckBox* m_mutebox;
    KIntNumInput* m_volumeinput;
};

KMediaBox::KMediaBox(QWidget *parent,
                     const QString &id, const QString &name,
                     const QString &output, bool mute, int volume,
                     const QStringList &audiooutputs)
    : QGroupBox(parent),
    m_id(id),
    m_outputbox(nullptr),
    m_mutebox(nullptr),
    m_volumeinput(nullptr)
{
    const QString title = QString::fromLatin1("%1 (%2)").arg(name, id);
    setTitle(title);

    QGridLayout* medialayout = new QGridLayout(this);

    QLabel* outputlabel = new QLabel(i18n("Output:"), this);
    medialayout->addWidget(outputlabel, 0, 0);
    m_outputbox = new QComboBox(this);
    m_outputbox->addItems(audiooutputs);
    setOutput(output);
    connect(m_outputbox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotOutput()));
    medialayout->addWidget(m_outputbox, 0, 1);
    
    m_mutebox = new QCheckBox(i18n("Mute"), this);
    m_mutebox->setChecked(mute);
    connect(m_mutebox, SIGNAL(stateChanged(int)), this, SLOT(slotMute()));
    medialayout->addWidget(m_mutebox, 1, 0, 1, 2);

    QLabel* volumelabel = new QLabel(i18n("Volume:"), this);
    medialayout->addWidget(volumelabel, 2, 0);
    m_volumeinput = new KIntNumInput(this);
    m_volumeinput->setRange(0, 100);
    m_volumeinput->setValue(volume);
    m_volumeinput->setSliderEnabled(true);
    m_volumeinput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_volumeinput, SIGNAL(valueChanged(int)), this, SLOT(slotVolume()));
    medialayout->addWidget(m_volumeinput, 2, 1);
}

QString KMediaBox::id() const
{
    return m_id;
}

QString KMediaBox::output() const
{
    return m_outputbox->currentText();
}

bool KMediaBox::mute() const
{
    return m_mutebox->isChecked();
}

int KMediaBox::volume() const
{
    return m_volumeinput->value();
}

void KMediaBox::setDefault()
{
    setOutput(s_kmediaoutput);
    m_mutebox->setChecked(s_kmediamute);
    m_volumeinput->setValue(s_kmediavolume);
}

void KMediaBox::setOutput(const QString &output)
{
    for (int i = 0; i < m_outputbox->count(); i++) {
        if (m_outputbox->itemText(i) == output) {
            m_outputbox->setCurrentIndex(i);
            break;
        }
    }
}

void KMediaBox::slotOutput()
{
    emit changed();
}

void KMediaBox::slotMute()
{
    emit changed();
}

void KMediaBox::slotVolume()
{
    emit changed();
}


K_PLUGIN_FACTORY(PlayerFactory, registerPlugin<KCMPlayer>();)
K_EXPORT_PLUGIN(PlayerFactory("kcmplayer"))

KCMPlayer::KCMPlayer(QWidget *parent, const QVariantList &arguments)
    : KCModule(PlayerFactory::componentData(), parent),
    m_layout(nullptr),
    m_spacer(nullptr)
{
    Q_UNUSED(arguments);

    setButtons(KCModule::Default | KCModule::Apply);
    setQuickHelp(i18n("<h1>Media Player</h1> This module allows you to change KDE media player options."));

    KAboutData *about = new KAboutData(
        I18N_NOOP("kcmplayer"), 0,
        ki18n("KDE Media Player Module"),
        "2.0", KLocalizedString(), KAboutData::License_GPL,
        ki18n("Copyright 2016, Ivailo Monev xakepa10@gmail.com")
    );
    about->addAuthor(ki18n("Ivailo Monev"), KLocalizedString(), "xakepa10@gmail.com");
    setAboutData(about);

    m_layout = new QVBoxLayout(this);
    setLayout(m_layout);
}

KCMPlayer::~KCMPlayer()
{
}

void KCMPlayer::defaults()
{
    foreach (KMediaBox* mediabox, m_mediaboxes) {
        mediabox->setDefault();
    }
    emit changed(true);
}

void KCMPlayer::load()
{
    qDeleteAll(m_mediaboxes);
    m_mediaboxes.clear();
    for (int i = 0; i < m_layout->count(); i++) {
        const QLayoutItem* layoutitem = m_layout->itemAt(i);
        if (layoutitem == m_spacer) {
            delete m_layout->takeAt(i);
            m_spacer = nullptr;
            break;
        }
    }
    Q_ASSERT(m_spacer == nullptr);

    // HACK: if application starts the KCM (like kmediaplayer does) show only the application settings
    const QString appname = QCoreApplication::applicationName();
    const bool issystemsettings = (appname == QLatin1String("systemsettings"));
    bool showall = true;

    KAudioPlayer kaudioplayer(this);
    const QStringList audiooutputs = kaudioplayer.audiooutputs();
    KSettings ksettings("kmediaplayer", KSettings::FullConfig);
    // NOTE: this catches all .desktop files
    const KService::List servicefiles = KService::allServices();
    foreach (const KService::Ptr service, servicefiles) {
        if (!issystemsettings && service->desktopEntryName() == appname) {
            showall = false;
            break;
        }
    }
    foreach (const KService::Ptr service, servicefiles) {
        if (!showall && service->desktopEntryName() != appname) {
            continue;
        }
        const QString medianame = service->name();
        const QStringList mediaids = service->property("X-KDE-MediaPlayer", QVariant::StringList).toStringList();
        foreach (const QString &id, mediaids) {
            const QString output = ksettings.string(id + "/audiooutput", s_kmediaoutput);
            const bool mute = ksettings.boolean(id + "/mute", s_kmediamute);
            const int volume = ksettings.integer(id + "/volume", s_kmediavolume);
            KMediaBox* mediabox = new KMediaBox(
                this,
                id, medianame,
                output, mute, volume,
                audiooutputs
            );
            m_mediaboxes.append(mediabox);
            connect(mediabox, SIGNAL(changed()), this, SLOT(slotMediaChanged()));
            m_layout->addWidget(mediabox);
        }
    }
    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_layout->addSpacerItem(m_spacer);
    emit changed(false);
}

void KCMPlayer::save()
{
    KSettings ksettings("kmediaplayer", KSettings::FullConfig);
    foreach (const KMediaBox* mediabox, m_mediaboxes) {
        const QString id = mediabox->id();
        ksettings.setString(id + "/audiooutput", mediabox->output());
        ksettings.setBoolean(id + "/mute", mediabox->mute());
        ksettings.setInteger(id + "/volume", mediabox->volume());
    }
    emit changed(false);
}

void KCMPlayer::slotMediaChanged()
{
    emit changed(true);
}

#include "moc_kcmplayer.cpp"
#include "kcmplayer.moc"

/*  This file is part of the KDE project
    Copyright (C) 2022 Ivailo Monev <xakepa10@gmail.com>

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

#include "kpasswdstoreconfig.h"

#include <QGroupBox>
#include <QLabel>
#include <QGridLayout>
#include <kpasswdstore.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kpluginfactory.h>
#include <kpluginloader.h>

// NOTE: keep in sync with:
// kdelibs/kutils/kpasswdstore/kded/kpasswdstoreimpl.cpp
// kdelibs/kutils/kpasswdstore/kpasswdstore.cpp
static const QByteArray kpasswdstore_cookie = QByteArray("user");
static const int kpasswdstore_passretries = 3;
static const qint64 kpasswdstore_passtimeout = 2; // minutes

K_PLUGIN_FACTORY(KCMPasswdStoreFactory, registerPlugin<KCMPasswdStore>();)
K_EXPORT_PLUGIN(KCMPasswdStoreFactory("kcmpasswdstoreconfig", "kcm_passwdstoreconfig"))

KCMPasswdStore::KCMPasswdStore(QWidget* parent, const QVariantList& args)
    : KCModule(KCMPasswdStoreFactory::componentData(), parent),
    m_cookiebox(nullptr),
    m_retriesinput(nullptr),
    m_timeoutinput(nullptr),
    m_storesbox(nullptr),
    m_keyedit(nullptr),
    m_passedit(nullptr),
    m_storebutton(nullptr)
{
    Q_UNUSED(args);

    setButtons(KCModule::Default | KCModule::Apply);
    setQuickHelp(i18n("<h1>Password Store</h1> This module allows you to change KDE password store preferences."));

    KAboutData *about = new KAboutData(
        I18N_NOOP("KCMPasswdStore"), 0,
        ki18n("KDE Password Store Module"),
        0, KLocalizedString(), KAboutData::License_GPL,
        ki18n("Copyright 2022, Ivailo Monev xakepa10@gmail.com")
    );
    about->addAuthor(ki18n("Ivailo Monev"), KLocalizedString(), "xakepa10@gmail.com");
    setAboutData(about);

    QVBoxLayout* layout = new QVBoxLayout(this);
    setLayout(layout);

    // General
    QGroupBox* generalgroup = new QGroupBox(this);
    generalgroup->setTitle(i18n("General"));
    QGridLayout* generallayout = new QGridLayout(generalgroup);

    QLabel* cookielabel = new QLabel(i18n("Cookie:"), this);
    cookielabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    generallayout->addWidget(cookielabel, 0, 0);
    m_cookiebox = new QComboBox(this);
    m_cookiebox->addItem(QString::fromLatin1("User"));
    m_cookiebox->addItem(QString::fromLatin1("PID"));
    m_cookiebox->addItem(QString::fromLatin1("Random"));
    m_cookiebox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    m_cookiebox->setToolTip(
        i18nc("@info:tooltip",
            "<b>User:</b>"
            "<p>Opening same password store from the same user will not require authentication again"
            " until the store is closed automatically or the user logs out.</p>"
            "<b>PID:</b>"
            "<p>Opening same password store from the same process will not require authentication again"
            " until the store is closed automatically or the process exits.</p>"
            "<b>Random:</b>\n"
            "<p>Opening password store will always require authentication.</p>"
        )
    );
    connect(
        m_cookiebox, SIGNAL(currentIndexChanged(QString)),
        this, SLOT(slotCookieChanged(QString))
    );
    generallayout->addWidget(m_cookiebox, 0, 1);

    QLabel* retrieslabel = new QLabel(i18n("Retries:"), generalgroup);
    retrieslabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    generallayout->addWidget(retrieslabel, 1, 0);
    m_retriesinput = new KIntNumInput(this);
    m_retriesinput->setRange(1, 255);
    m_retriesinput->setSliderEnabled(true);
    m_retriesinput->setToolTip(i18n("The number of password retries after which stores will not store passwords permanently."));
    connect(
        m_retriesinput, SIGNAL(valueChanged(int)),
        this, SLOT(slotRetriesChanged(int))
    );
    generallayout->addWidget(m_retriesinput, 1, 1);

    QLabel* timeoutlabel = new QLabel(i18n("Timeout:"), generalgroup);
    timeoutlabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    generallayout->addWidget(timeoutlabel, 2, 0);
    m_timeoutinput = new KIntNumInput(generalgroup);
    m_timeoutinput->setRange(1, 255);
    m_timeoutinput->setSliderEnabled(true);
    m_timeoutinput->setSuffix(i18n(" minute(s)"));
    m_timeoutinput->setToolTip(i18n("The timeout in minutes of inactivity after which stores are automatically closed."));
    connect(
        m_timeoutinput, SIGNAL(valueChanged(int)),
        this, SLOT(slotTimeoutChanged(int))
    );
    generallayout->addWidget(m_timeoutinput, 2, 1);

    // Password stores
    QGroupBox* storesgroup = new QGroupBox(this);
    storesgroup->setTitle(i18n("Password stores"));
    QGridLayout* storeslayout = new QGridLayout(storesgroup);

    QLabel* storelabel = new QLabel(i18n("Store:"), storesgroup);
    storelabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    storeslayout->addWidget(storelabel, 0, 0);
    m_storesbox = new QComboBox(this);
    m_storesbox->addItems(KPasswdStore::stores());
    m_storesbox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    storeslayout->addWidget(m_storesbox, 0, 1);

    QLabel* userlabel = new QLabel(i18n("Key:"), storesgroup);
    userlabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    storeslayout->addWidget(userlabel, 1, 0);
    m_keyedit = new KLineEdit(storesgroup);
    m_keyedit->setPlaceholderText(i18n("Enter key..."));
    m_keyedit->setToolTip(
        i18nc("@info:tooltip",
            "<p>The key can be anything - URL to website, URL to encrypted PDF document or just a string.<p>"
            "<p>If the key exists in the password store <b>it will be overwritten</b>.<p>"
        )
    );
    storeslayout->addWidget(m_keyedit, 1, 1);

    QLabel* passlabel = new QLabel(i18n("Password:"), storesgroup);
    passlabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    storeslayout->addWidget(passlabel, 2, 0);
    m_passedit = new KLineEdit(storesgroup);
    m_passedit->setPlaceholderText(i18n("Enter password..."));
    m_passedit->setEchoMode(QLineEdit::Password);
    storeslayout->addWidget(m_passedit, 2, 1);

    m_storebutton = new KPushButton(storesgroup);
    m_storebutton->setIcon(KIcon("dialog-ok-apply"));
    m_storebutton->setText(i18n("Store"));
    m_storebutton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    connect(
        m_storebutton, SIGNAL(pressed()),
        this, SLOT(slotStorePressed())
    );
    storeslayout->addWidget(m_storebutton, 3, 1);

    layout->addWidget(generalgroup);
    layout->addWidget(storesgroup);
    QSpacerItem* spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addItem(spacer);
}

KCMPasswdStore::~KCMPasswdStore()
{
}

void KCMPasswdStore::load()
{
    KConfig kconfig("kpasswdstorerc", KConfig::SimpleConfig);
    KConfigGroup kconfiggroup = kconfig.group("KPasswdStore");
    const QByteArray cookietype = kconfiggroup.readEntry("Cookie", kpasswdstore_cookie).toLower();
    for (int i = 0; i < m_cookiebox->count(); i++) {
        if (m_cookiebox->itemText(i).toLower() == cookietype) {
            m_cookiebox->setCurrentIndex(i);
            break;
        }
    }
    m_retriesinput->setValue(kconfiggroup.readEntry("Retries", kpasswdstore_passretries));
    m_timeoutinput->setValue(kconfiggroup.readEntry("Timeout", kpasswdstore_passtimeout));
    emit changed(false);
}

void KCMPasswdStore::save()
{
    KConfig kconfig("kpasswdstorerc", KConfig::SimpleConfig);
    KConfigGroup kconfiggroup = kconfig.group("KPasswdStore");
    kconfiggroup.writeEntry("Cookie", m_cookiebox->currentText().toLatin1());
    kconfiggroup.writeEntry("Retries", m_retriesinput->value());
    kconfiggroup.writeEntry("Timeout", m_timeoutinput->value());
    emit changed(false);
}

void KCMPasswdStore::defaults()
{
    for (int i = 0; i < m_cookiebox->count(); i++) {
        if (m_cookiebox->itemText(i).toLower() == kpasswdstore_cookie) {
            m_cookiebox->setCurrentIndex(i);
            break;
        }
    }
    m_retriesinput->setValue(kpasswdstore_passretries);
    m_timeoutinput->setValue(kpasswdstore_passtimeout);
    emit changed(true);
}

void KCMPasswdStore::slotCookieChanged(const QString &style)
{
    Q_UNUSED(style);
    emit changed(true);
}

void KCMPasswdStore::slotRetriesChanged(const int retries)
{
    Q_UNUSED(retries);
    emit changed(true);
}

void KCMPasswdStore::slotTimeoutChanged(const int timeout)
{
    Q_UNUSED(timeout);
    emit changed(true);
}

void KCMPasswdStore::slotStorePressed()
{
    const QString key = m_keyedit->text();
    if (key.isEmpty()) {
        KMessageBox::error(this, i18n("No key specified"));
        return;
    }
    KPasswdStore kpasswdstore(this);
    kpasswdstore.setStoreID(m_storesbox->currentText());
    if (!kpasswdstore.openStore(winId())) {
        return;
    }
    const bool storeresult = kpasswdstore.storePasswd(
        KPasswdStore::makeKey(key), m_passedit->text(),
        winId()
    );
    if (!storeresult) {
        KMessageBox::error(this, i18n("Could not store the password"));
        return;
    }
    KMessageBox::information(
        this,
        i18n("The password was successfully stored, restart of applications may be required"),
        QString(),
        QString::fromLatin1("KCMPasswdStore_store")
    );
}

#include "moc_kpasswdstoreconfig.cpp"

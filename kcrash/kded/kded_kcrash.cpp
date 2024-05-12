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

#include "kded_kcrash.h"
#include <kpluginfactory.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <ktoolinvocation.h>
#include <kurl.h>
#include <kcrash.h>
#include <kdebug.h>

#include <QDir>
#include <QFileInfo>

#include <signal.h>

// NOTE: keep in sync with:
// kdelibs/kdeui/util/kcrash.cpp

static const QStringList s_kcrashfilters = QStringList()
    << QString::fromLatin1("*.kcrash");

static QString kSignalDescription(const int signal)
{
    switch (signal) {
        // for reference:
        // https://man7.org/linux/man-pages/man7/signal.7.html
        case SIGSEGV: {
            return i18n("Invalid memory reference");
        }
        case SIGBUS: {
            return i18n("Bus error (bad memory access)");
        }
        case SIGFPE: {
            return i18n("Floating-point exception");
        }
        case SIGILL: {
            return i18n("Illegal Instruction");
        }
        case SIGABRT: {
            return i18n("Abort signal from <i>abort<i>");
        }
    }
    return i18n("Unknown");
}

static QString kFixURL(const QString &url)
{
    KUrl kurl(url);
    const QString kurlprotocol = kurl.protocol();
    if (kurlprotocol.isEmpty() || kurlprotocol == QLatin1String("mailto")) {
        kurl.setScheme("mailto");
    }
    return kurl.url();
}

K_PLUGIN_FACTORY(KCrashModuleFactory, registerPlugin<KCrashModule>();)
K_EXPORT_PLUGIN(KCrashModuleFactory("kcrash"))

KCrashModule::KCrashModule(QObject *parent, const QList<QVariant> &args)
    : KDEDModule(parent),
    m_dirwatch(nullptr)
{
    Q_UNUSED(args);

    m_kcrashpath = KGlobal::dirs()->saveLocation("tmp", "kcrash/");
    m_dirwatch = new KDirWatch(this);
    m_dirwatch->addDir(m_kcrashpath);
    connect(
        m_dirwatch, SIGNAL(dirty(QString)),
        this, SLOT(slotDirty(QString))
    );
}

KCrashModule::~KCrashModule()
{
    delete m_dirwatch;

    kDebug() << "Closing notifications" << m_notifications.size();
    QMutableListIterator<KNotification*> iter(m_notifications);
    while (iter.hasNext()) {
        KNotification* knotification = iter.next();
        disconnect(knotification, 0, this, 0);
        knotification->close();
        iter.remove();
    }
}

void KCrashModule::slotDirty(const QString &path)
{
    Q_UNUSED(path);
    QDir kcrashdir(m_kcrashpath);
    foreach (const QFileInfo &fileinfo, kcrashdir.entryInfoList(s_kcrashfilters, QDir::Files)) {
        const QString kcrashfilepath = fileinfo.absoluteFilePath();
        QFile kcrashfile(kcrashfilepath);
        if (!kcrashfile.open(QFile::ReadOnly)) {
            kWarning() << "Could not open" << kcrashfilepath;
            continue;
        }

        kDebug() << "Reading" << kcrashfilepath;
        QMap<QByteArray,QString> kcrashdata;
        int kcrashsignal = 0;
        QByteArray kcrashbacktrace;
        {
            QDataStream crashstream(&kcrashfile);
            crashstream >> kcrashdata;
            crashstream >> kcrashsignal;
            crashstream >> kcrashbacktrace;
        }
        if (!QFile::remove(kcrashfilepath)) {
            kWarning() << "Could not remove" << kcrashfilepath;
        }

        const KCrash::CrashFlags kcrashflags = static_cast<KCrash::CrashFlags>(
            kcrashdata["flags"].toInt()
        );
        QString kcrashappname = kcrashdata["programname"];
        if (kcrashappname.isEmpty()) {
            kcrashappname = kcrashdata["appname"];
        }
        if (kcrashflags & KCrash::Notify) {
            QString bugreporturl = kcrashdata["bugaddress"];
            // special case for Katana applications
            if (bugreporturl == QLatin1String(KDE_BUG_REPORT_EMAIL)) {
                bugreporturl = QString::fromLatin1(KDE_BUG_REPORT_URL);
            }

            kDebug() << "Sending notification for" << kcrashfilepath;
            // NOTE: when the notification is closed/deleted the actions become non-operational
            KNotification* knotification = new KNotification(this);
            knotification->setEventID("kcrash/Crash");
            knotification->setFlags(KNotification::Persistent);
            knotification->setTitle(i18n("%1 crashed", kcrashappname));
            knotification->setText(
                i18n(
                    "<p><b>Reason:</b><br/>%1</p><p>For details about the crash look into the system log.</p>",
                    kSignalDescription(kcrashsignal)
                )
            );
            knotification->setActions(QStringList() << i18n("Report"));
            knotification->setProperty("_k_url", kFixURL(bugreporturl));
            m_notifications.append(knotification);
            connect(knotification, SIGNAL(closed()), this, SLOT(slotClosed()));
            connect(knotification, SIGNAL(action1Activated()), this, SLOT(slotReport()));
            knotification->send();
        }
        if (kcrashflags & KCrash::Log) {
            // NOTE: this goes to the system log by default which the user may not have access to
            kError() << kcrashappname << "crashed" << kcrashbacktrace;
        }
        if (kcrashflags & KCrash::AutoRestart) {
            const QString kcrashdisplay = kcrashdata["display"];
            const QString kcrashapppath = kcrashdata["apppath"];
            QStringList kcrashargs;
            if (!kcrashdisplay.isEmpty()) {
                kcrashargs.append(QString::fromLatin1("--display"));
                kcrashargs.append(kcrashdisplay);
            }
            kDebug() << "Restarting" << kcrashfilepath << kcrashapppath << kcrashargs;
            KToolInvocation::self()->startProgram(kcrashapppath, kcrashargs);
        }
    }
}

void KCrashModule::slotClosed()
{
    KNotification* knotification = qobject_cast<KNotification*>(sender());
    kDebug() << "Notification closed" << knotification;
    m_notifications.removeAll(knotification);
}

void KCrashModule::slotReport()
{
    KNotification* knotification = qobject_cast<KNotification*>(sender());
    kDebug() << "Notification report action triggered" << knotification;
    const QString kcrashreporturl = knotification->property("_k_url").toString();
    knotification->close();
    if (kcrashreporturl.startsWith(QLatin1String("mailto:"))) {
        KToolInvocation::self()->invokeMailer(kcrashreporturl);
    } else {
        KToolInvocation::self()->invokeBrowser(kcrashreporturl);
    }
}

#include "moc_kded_kcrash.cpp"

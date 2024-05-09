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

#include <KAboutData>
#include <KCmdLineArgs>
#include <KApplication>
#include <KLocale>
#include <KEMailDialog>
#include <KHelpMenu>
#include <KDebug>

static QStringList splitMailArg(const QString &arg)
{
    return arg.split(QLatin1Char(','));
}

class KMailDialog : public KEMailDialog
{
    Q_OBJECT
public:
    KMailDialog(QWidget *parent = nullptr, Qt::WindowFlags flags = 0);
    ~KMailDialog();
};

KMailDialog::KMailDialog(QWidget *parent, Qt::WindowFlags flags)
    : KEMailDialog(parent, flags)
{
    KConfigGroup kconfiggroup(KGlobal::config(), "KMailDialog");
    restoreDialogSize(kconfiggroup);
}

KMailDialog::~KMailDialog()
{
    KConfigGroup kconfiggroup(KGlobal::config(), "KMailDialog");
    saveDialogSize(kconfiggroup);
    KGlobal::config()->sync();
}

int main(int argc, char **argv) {
    KAboutData aboutData(
        "kmail", 0, ki18n("KMail"),
        "1.0.0", ki18n("Simple e-mail sender for KDE."),
        KAboutData::License_GPL_V2,
        ki18n("(c) 2022 Ivailo Monev")
    );

    aboutData.addAuthor(ki18n("Ivailo Monev"), ki18n("Maintainer"), "xakepa10@gmail.com");
    aboutData.setProgramIconName(QLatin1String("internet-mail"));

    KCmdLineArgs::init(argc, argv, &aboutData);
    KCmdLineOptions option;
    option.add("+[url]", ki18n("URL to be opened"));
    KCmdLineArgs::addCmdLineOptions(option);

    KApplication *kapplication = new KApplication();
    kapplication->enableSessionManagement();
    KMailDialog kmaildialog;
    kmaildialog.setButtons(KDialog::Ok | KDialog::Close | KDialog::Help);
    kmaildialog.show();
    KHelpMenu khelpmenu(&kmaildialog, &aboutData, true);
    kmaildialog.setButtonMenu(KDialog::Help, (QMenu*)khelpmenu.menu());

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    for (int pos = 0; pos < args->count(); pos++) {
        const KUrl argurl = args->url(pos);

        if (argurl.hasQueryItem("subject")) {
            kmaildialog.setSubject(argurl.queryItemValue("subject"));
        }

        QStringList mailto;
        if (argurl.hasQueryItem("to")) {
            mailto.append(splitMailArg(argurl.queryItemValue("to")));
        }
        if (argurl.hasQueryItem("cc")) {
            mailto.append(splitMailArg(argurl.queryItemValue("cc")));
        }
        if (mailto.isEmpty()) {
            mailto.append(argurl.path());
        }
        kmaildialog.setTo(mailto);

        if (argurl.hasQueryItem("body")) {
            kmaildialog.setMessage(argurl.queryItemValue("body"));
        }

        if (argurl.hasQueryItem("attach")) {
            kmaildialog.setAttach(splitMailArg(argurl.queryItemValue("attach")));
        }
        break;
    }

    return kapplication->exec();
}

#include "main.moc"

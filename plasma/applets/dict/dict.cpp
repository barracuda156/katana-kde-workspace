/*
    This file is part of the KDE project
    Copyright (C) 2024 Ivailo Monev <xakepa10@gmail.com>

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

#include "dict.h"

#include <QGraphicsGridLayout>
#include <QJsonDocument>
#include <KIcon>
#include <KLineEdit>
#include <KIO/Job>
#include <KIO/StoredTransferJob>
#include <Plasma/IconWidget>
#include <Plasma/LineEdit>
#include <Plasma/TextBrowser>
#include <KDebug>

static const QString s_dictapi = QString::fromLatin1("https://api.dictionaryapi.dev/api/v2/entries/en/");
static const QString s_defaultpopupicon = QString::fromLatin1("accessories-dictionary");
static const QSizeF s_minimumsize = QSizeF(300, 250);

class DictAppletWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    DictAppletWidget(DictApplet* dictapplet);

private Q_SLOTS:
    void slotWordChanged();
    void slotFinished(KJob *kjob);

private:
    void setText(const QString &text, const bool error);

    DictApplet* m_dictapplet;
    QGraphicsGridLayout* m_layout;
    Plasma::IconWidget* m_iconwidget;
    Plasma::LineEdit* m_wordedit;
    Plasma::TextBrowser* m_textbrowser;
    QTextBrowser* m_nativetextbrowser;
    KIO::StoredTransferJob* m_kiojob;
};

DictAppletWidget::DictAppletWidget(DictApplet* dictapplet)
    : QGraphicsWidget(dictapplet),
    m_dictapplet(dictapplet),
    m_layout(nullptr),
    m_iconwidget(nullptr),
    m_wordedit(nullptr),
    m_textbrowser(nullptr),
    m_kiojob(nullptr)
{
    setMinimumSize(s_minimumsize);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_layout = new QGraphicsGridLayout(this);

    m_iconwidget = new Plasma::IconWidget(this);
    m_iconwidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_iconwidget->setIcon(KIcon(s_defaultpopupicon));
    m_iconwidget->setAcceptHoverEvents(false);
    m_iconwidget->setAcceptedMouseButtons(Qt::NoButton);
    m_layout->addItem(m_iconwidget, 0, 0);

    m_wordedit = new Plasma::LineEdit(this);
    m_wordedit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    m_wordedit->setClearButtonShown(true);
    m_wordedit->setClickMessage(i18n("Enter word to define here"));
    setFocusProxy(m_wordedit);
    m_wordedit->setFocus();
    connect(m_wordedit, SIGNAL(returnPressed()), this, SLOT(slotWordChanged()));
    connect(m_wordedit->nativeWidget(), SIGNAL(clearButtonClicked()), this, SLOT(slotWordChanged()));
    m_layout->addItem(m_wordedit, 0, 1);

    m_textbrowser = new Plasma::TextBrowser(this);
    m_textbrowser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_nativetextbrowser = m_textbrowser->nativeWidget();
    m_nativetextbrowser->setReadOnly(true);
    m_layout->addItem(m_textbrowser, 1, 0, 1, 2);
    // big stretch factor to squeeze the icon as much as possible
    m_layout->setColumnStretchFactor(1, 100);

    setLayout(m_layout);
}

void DictAppletWidget::setText(const QString &text, const bool error)
{
    if (m_kiojob) {
        m_kiojob->deleteLater();
        m_kiojob = nullptr;
    }
    m_textbrowser->setText(text);
    QTextCursor textcursor = m_nativetextbrowser->textCursor();
    m_nativetextbrowser->selectAll();
    if (error) {
        m_nativetextbrowser->setAlignment(Qt::AlignCenter);
    } else {
        m_nativetextbrowser->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    }
    textcursor.movePosition(QTextCursor::End);
    m_nativetextbrowser->setTextCursor(textcursor);
    m_wordedit->setEnabled(true);
    m_dictapplet->setBusy(false);
}

void DictAppletWidget::slotWordChanged()
{
    const QString queryword = m_wordedit->text();
    // basic validation
    if (queryword.isEmpty()) {
        setText(QString(), false);
        return;
    // NOTE: API restriction
    } else if (queryword.contains(' ')) {
        setText(i18n("Only words can be queried"), true);
        return;
    }
    
    kDebug() << "starting dict job for" << queryword;
    m_wordedit->setEnabled(false);
    m_dictapplet->setBusy(true);
    const KUrl queryurl = s_dictapi + queryword;
    m_kiojob = KIO::storedGet(queryurl, KIO::HideProgressInfo);
    m_kiojob->setAutoDelete(false);
    connect(m_kiojob, SIGNAL(finished(KJob*)), this, SLOT(slotFinished(KJob*)));
}

void DictAppletWidget::slotFinished(KJob *kjob)
{
    kDebug() << "dict job finished";
    if (kjob->error() == KIO::ERR_SERVICE_NOT_AVAILABLE) {
        // special case, the server returns 404 when no definition is found
        setText(i18n("No definitions found"), true);
        return;
    } else if (kjob->error() != KJob::NoError) {
        setText(kjob->errorString(), true);
        return;
    }

    const QJsonDocument jsondocument = QJsonDocument::fromJson(m_kiojob->data());
    if (jsondocument.isNull()) {
        kWarning() << jsondocument.errorString();
        setText(i18n("Cannot parse JSON"), true);
        return;
    }

    const QVariantList rootlist = jsondocument.toVariant().toList();
    if (rootlist.isEmpty()) {
        kWarning() << "unexpected dict JSON data";
        setText(i18n("Unexpected JSON data"), true);
        return;
    }
    const QVariantList meaningslist = rootlist.first().toMap().value("meanings").toList();
    if (meaningslist.isEmpty()) {
        kWarning() << "unexpected dict meanings data";
        setText(i18n("Unexpected meanings data"), true);
        return;
    }
    // qDebug() << Q_FUNC_INFO << "meanings" << meaningslist;
    const QVariantList definitionslist = meaningslist.first().toMap().value("definitions").toList();
    if (definitionslist.isEmpty()) {
        kWarning() << "unexpected dict definitions data";
        setText(i18n("Unexpected definitions data"), true);
        return;
    }
    // qDebug() << Q_FUNC_INFO << "definitions" << definitionslist;
    const QString definition = definitionslist.first().toMap().value("definition").toString();
    const QString example = definitionslist.first().toMap().value("example").toString();
    QString meaning = "<p>\n<dl><b>Definition:</b> ";
    meaning.append(definition);
    meaning.append("\n</dl>");
    meaning.append("<dl>\n<b>Example:</b> ");
    meaning.append(example);
    meaning.append("\n</dl>\n</p>\n");
    kDebug() << "dict result is" << definition << example;
    setText(meaning, false);
}

DictApplet::DictApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_dictwidget(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_qstardict");
    setPopupIcon(s_defaultpopupicon);
    setAspectRatioMode(Plasma::IgnoreAspectRatio);

    m_dictwidget = new DictAppletWidget(this);
}

void DictApplet::init()
{
}

QGraphicsWidget *DictApplet::graphicsWidget()
{
    return m_dictwidget;
}

#include "dict.moc"
#include "moc_dict.cpp"

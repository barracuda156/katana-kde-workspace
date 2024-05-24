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

#include "notes.h"

#include <QFile>
#include <QTextStream>
#include <QGridLayout>
#include <KTextEdit>
#include <Plasma/Frame>
#include <Plasma/TextEdit>
#include <KDebug>

static const QSizeF s_minimumsize = QSizeF(256, 256);
static const QString s_defaultpopupicon = QString::fromLatin1("knotes");

void kSetTextEditFont(KTextEdit *ktextedit, const QString &fontstring)
{
    QFont font;
    if (!font.fromString(fontstring)) {
        kWarning() << "could not create QFont from" << fontstring;
        return;
    }
    ktextedit->setEnabled(false);
    // if only it was that easy..
    ktextedit->setFont(font);
    // have to save the cursor, select all, change the text format and move the cursor back in
    // position
    const QTextCursor oldtextcursor = ktextedit->textCursor();
    ktextedit->selectAll();
    ktextedit->setCurrentFont(font);
    ktextedit->setTextCursor(oldtextcursor);
    ktextedit->setEnabled(true);
}

class NotesAppletWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    NotesAppletWidget(QGraphicsWidget *parent);

    Plasma::TextEdit* textEdit() const;
    KTextEdit* nativeTextEdit() const;

private:
    QGraphicsLinearLayout* m_layout;
    Plasma::Frame* m_frame;
    QGraphicsLinearLayout* m_framelayout;
    Plasma::TextEdit* m_textedit;
    KTextEdit* m_nativetextedit;
};

NotesAppletWidget::NotesAppletWidget(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
    m_layout(nullptr),
    m_frame(nullptr),
    m_framelayout(nullptr),
    m_textedit(nullptr),
    m_nativetextedit(nullptr)
{
    m_layout = new QGraphicsLinearLayout(this);
    m_layout->setOrientation(Qt::Vertical);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_frame = new Plasma::Frame(this);
    m_frame->setFrameShadow(Plasma::Frame::Sunken);
    m_frame->setMinimumSize(s_minimumsize);
    m_frame->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_framelayout = new QGraphicsLinearLayout(m_frame);
    m_framelayout->setContentsMargins(0, 0, 0, 0);
    m_frame->setLayout(m_framelayout);

    m_textedit = new Plasma::TextEdit(m_frame);
    m_framelayout->addItem(m_textedit);
    m_layout->addItem(m_frame);

    m_nativetextedit = m_textedit->nativeWidget();
    m_nativetextedit->setAcceptRichText(false);

    setLayout(m_layout);

    adjustSize();
}

Plasma::TextEdit* NotesAppletWidget::textEdit() const
{
    return m_textedit;
}

KTextEdit* NotesAppletWidget::nativeTextEdit() const
{
    return m_nativetextedit;
}


NotesApplet::NotesApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_noteswidget(nullptr),
    m_fontrequester(nullptr),
    m_spacer(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_notes");
    setAspectRatioMode(Plasma::AspectRatioMode::IgnoreAspectRatio);
    setHasConfigurationInterface(true);
    setStatus(Plasma::ItemStatus::ActiveStatus);
    setPopupIcon(s_defaultpopupicon);
}

void NotesApplet::init()
{
    Plasma::PopupApplet::init();

    m_noteswidget = new NotesAppletWidget(this);

    const QVariantList args = startupArguments();
    if (args.size() > 0) {
        // drop, the first argument is a path to temporary file;
        const QString filepath = args.at(0).toString();
        QFile file(filepath);
        if (!file.open(QIODevice::ReadOnly)) {
            kWarning() << "could not open" << filepath << file.errorString();
        } else {
            QTextStream textstream(&file);
            Plasma::TextEdit* plasmatextedit = m_noteswidget->textEdit();
            plasmatextedit->setText(textstream.readAll());
        }
    }

    configChanged();
}

void NotesApplet::createConfigurationInterface(KConfigDialog *parent)
{
    KTextEdit* nativetextedit = m_noteswidget->nativeTextEdit();

    QWidget* widget = new QWidget();
    QGridLayout* widgetlayout = new QGridLayout(widget);
    QLabel* fontlabel = new QLabel(widget);
    fontlabel->setText(i18n("Font:"));
    widgetlayout->addWidget(fontlabel, 0, 0, Qt::AlignRight | Qt::AlignVCenter);
    m_fontrequester = new KFontRequester(widget);
    m_fontrequester->setFont(nativetextedit->font());
    widgetlayout->addWidget(m_fontrequester, 0, 1);

    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
    widgetlayout->addItem(m_spacer, 2, 0, 1, 2);

    widget->setLayout(widgetlayout);
    parent->addPage(widget, i18n("Notes"), s_defaultpopupicon);

    connect(parent, SIGNAL(applyClicked()), this, SLOT(slotConfigAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(slotConfigAccepted()));
    connect(m_fontrequester, SIGNAL(fontSelected(QFont)), parent, SLOT(settingsModified()));
}

QGraphicsWidget* NotesApplet::graphicsWidget()
{
    return m_noteswidget;
}

void NotesApplet::configChanged()
{
    if (!m_noteswidget) {
        return;
    }
    Plasma::TextEdit* plasmatextedit = m_noteswidget->textEdit();
    KTextEdit* nativetextedit = m_noteswidget->nativeTextEdit();
    KConfigGroup configgroup = config();
    const bool checkSpellingEnabled = configgroup.readEntry("checkspelling", nativetextedit->checkSpellingEnabled());
    nativetextedit->setCheckSpellingEnabled(checkSpellingEnabled);
    if (nativetextedit->toPlainText().isEmpty()) {
        const QString text = configgroup.readEntry("text", QString());
        plasmatextedit->setText(text);
    }
    const QString configfont = configgroup.readEntry("font", nativetextedit->font().toString());
    kSetTextEditFont(nativetextedit, configfont);
}

void NotesApplet::slotConfigAccepted()
{
    Q_ASSERT(m_fontrequester != nullptr);
    Plasma::TextEdit* plasmatextedit = m_noteswidget->textEdit();
    KTextEdit* nativetextedit = plasmatextedit->nativeWidget();
    KConfigGroup configgroup = config();
    const QString fontrequesterfont = m_fontrequester->font().toString();
    configgroup.writeEntry("font", fontrequesterfont);
    kSetTextEditFont(nativetextedit, fontrequesterfont);
    emit configNeedsSaving();
}

void NotesApplet::saveState(KConfigGroup &group) const
{
    if (!m_noteswidget) {
        return;
    }
    Plasma::TextEdit* plasmatextedit = m_noteswidget->textEdit();
    KTextEdit* nativetextedit = plasmatextedit->nativeWidget();
    group.writeEntry("text", plasmatextedit->text());
    group.writeEntry("checkspelling", nativetextedit->checkSpellingEnabled());
    group.writeEntry("font", nativetextedit->font().toString());
    Plasma::PopupApplet::saveState(group);
}

K_EXPORT_PLASMA_APPLET(notes, NotesApplet)

#include "notes.moc"
#include "moc_notes.cpp"

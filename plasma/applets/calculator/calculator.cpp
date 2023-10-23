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

#include "calculator.h"

#include <QGraphicsGridLayout>
#include <QGraphicsLinearLayout>
#include <Plasma/Label>
#include <Plasma/Frame>
#include <Plasma/PushButton>
#include <Plasma/ToolTipManager>
#include <KDebug>

static const QString s_decimal = QString::fromLatin1(".");
static const QLatin1String s_zero = QLatin1String("0");

static QString kAddNumber(const QString &string, const ushort number)
{
    if (string.size() > 6) {
        // hard-limit
        return string;
    }
    if (string == s_zero) {
        return QString::number(number);
    }
    return string + QString::number(number);
}

static QFont kLabelFont()
{
    QFont labelfont = KGlobalSettings::generalFont();
    labelfont.setBold(true);
    labelfont.setPointSize(labelfont.pointSize() * 2);
    return labelfont;
}

class CalculatorAppletWidget : public QGraphicsWidget
{
    Q_OBJECT
public:
    enum CalculatorOperator {
        OperatorNone = 0,
        OperatorDiv = 1,
        OperatorMul = 2,
        OperatorMinus = 3,
        OperatorPlus = 4
    };

    CalculatorAppletWidget(QGraphicsWidget *parent);

public Q_SLOTS:
    void slotClear();
    void slotDiv();
    void slotMul();
    void slotClearAll();
    void slot7();
    void slot8();
    void slot9();
    void slotMinus();
    void slot4();
    void slot5();
    void slot6();
    void slotPlus();
    void slot1();
    void slot2();
    void slot3();
    void slotEqual();
    void slot0();
    void slotDec();
    void slotUpdateFonts();

private:
    QGraphicsGridLayout* m_layout;
    Plasma::Frame* m_frame;
    QGraphicsLinearLayout* m_framelayout;
    Plasma::Label* m_label;
    Plasma::PushButton* m_cbutton;
    Plasma::PushButton* m_divbutton;
    Plasma::PushButton* m_mulbutton;
    Plasma::PushButton* m_acbutton;
    Plasma::PushButton* m_7button;
    Plasma::PushButton* m_8button;
    Plasma::PushButton* m_9button;
    Plasma::PushButton* m_minusbutton;
    Plasma::PushButton* m_4button;
    Plasma::PushButton* m_5button;
    Plasma::PushButton* m_6button;
    Plasma::PushButton* m_plusbutton;
    Plasma::PushButton* m_1button;
    Plasma::PushButton* m_2button;
    Plasma::PushButton* m_3button;
    Plasma::PushButton* m_equalbutton;
    Plasma::PushButton* m_0button;
    Plasma::PushButton* m_decbutton;
    qreal m_savednumber;
    CalculatorOperator m_operator;
};

CalculatorAppletWidget::CalculatorAppletWidget(QGraphicsWidget *parent)
    : QGraphicsWidget(parent),
    m_layout(nullptr),
    m_frame(nullptr),
    m_framelayout(nullptr),
    m_label(nullptr),
    m_cbutton(nullptr),
    m_divbutton(nullptr),
    m_mulbutton(nullptr),
    m_acbutton(nullptr),
    m_7button(nullptr),
    m_8button(nullptr),
    m_9button(nullptr),
    m_minusbutton(nullptr),
    m_4button(nullptr),
    m_5button(nullptr),
    m_6button(nullptr),
    m_plusbutton(nullptr),
    m_1button(nullptr),
    m_2button(nullptr),
    m_3button(nullptr),
    m_equalbutton(nullptr),
    m_0button(nullptr),
    m_decbutton(nullptr),
    m_savednumber(0.0),
    m_operator(CalculatorAppletWidget::OperatorNone)
{
    m_layout = new QGraphicsGridLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_frame = new Plasma::Frame(this);
    m_frame->setFrameShadow(Plasma::Frame::Sunken);
    m_frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_framelayout = new QGraphicsLinearLayout(Qt::Horizontal, m_frame);
    m_label = new Plasma::Label(m_frame);
    m_label->setFont(kLabelFont());
    m_label->setText(QString::fromLatin1("0"));
    m_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_framelayout->addItem(m_label);
    m_layout->addItem(m_frame, 0, 0, 1, 4);

    m_cbutton = new Plasma::PushButton(this);
    m_cbutton->setText(i18nc("Text of the clear button", "C"));
    connect(
        m_cbutton, SIGNAL(released()),
        this, SLOT(slotClear())
    );
    m_layout->addItem(m_cbutton, 1, 0, 1, 1);
    m_divbutton = new Plasma::PushButton(this);
    m_divbutton->setText(i18nc("Text of the division button", "÷"));
    connect(
        m_divbutton, SIGNAL(released()),
        this, SLOT(slotDiv())
    );
    m_layout->addItem(m_divbutton, 1, 1, 1, 1);
    m_mulbutton = new Plasma::PushButton(this);
    m_mulbutton->setText(i18nc("Text of the multiplication button", "×"));
    connect(
        m_mulbutton, SIGNAL(released()),
        this, SLOT(slotMul())
    );
    m_layout->addItem(m_mulbutton, 1, 2, 1, 1);
    m_acbutton = new Plasma::PushButton(this);
    m_acbutton->setText(i18nc("Text of the all clear button", "AC"));
    connect(
        m_acbutton, SIGNAL(released()),
        this, SLOT(slotClearAll())
    );
    m_layout->addItem(m_acbutton, 1, 3, 1, 1);

    m_7button = new Plasma::PushButton(this);
    m_7button->setText(QString::fromLatin1("7"));
    connect(
        m_7button, SIGNAL(released()),
        this, SLOT(slot7())
    );
    m_layout->addItem(m_7button, 2, 0, 1, 1);
    m_8button = new Plasma::PushButton(this);
    m_8button->setText(QString::fromLatin1("8"));
    connect(
        m_8button, SIGNAL(released()),
        this, SLOT(slot8())
    );
    m_layout->addItem(m_8button, 2, 1, 1, 1);
    m_9button = new Plasma::PushButton(this);
    m_9button->setText(QString::fromLatin1("9"));
    connect(
        m_9button, SIGNAL(released()),
        this, SLOT(slot9())
    );
    m_layout->addItem(m_9button, 2, 2, 1, 1);
    m_minusbutton = new Plasma::PushButton(this);
    m_minusbutton->setText(i18nc("Text of the minus button", "-"));
    connect(
        m_minusbutton, SIGNAL(released()),
        this, SLOT(slotMinus())
    );
    m_layout->addItem(m_minusbutton, 2, 3, 1, 1);

    m_4button = new Plasma::PushButton(this);
    m_4button->setText(QString::fromLatin1("4"));
    connect(
        m_4button, SIGNAL(released()),
        this, SLOT(slot4())
    );
    m_layout->addItem(m_4button, 3, 0, 1, 1);
    m_5button = new Plasma::PushButton(this);
    m_5button->setText(QString::fromLatin1("5"));
    connect(
        m_5button, SIGNAL(released()),
        this, SLOT(slot5())
    );
    m_layout->addItem(m_5button, 3, 1, 1, 1);
    m_6button = new Plasma::PushButton(this);
    m_6button->setText(QString::fromLatin1("6"));
    connect(
        m_6button, SIGNAL(released()),
        this, SLOT(slot6())
    );
    m_layout->addItem(m_6button, 3, 2, 1, 1);
    m_plusbutton = new Plasma::PushButton(this);
    m_plusbutton->setText(i18nc("Text of the plus button", "+"));
    connect(
        m_plusbutton, SIGNAL(released()),
        this, SLOT(slotPlus())
    );
    m_layout->addItem(m_plusbutton, 3, 3, 1, 1);

    m_1button = new Plasma::PushButton(this);
    m_1button->setText(QString::fromLatin1("1"));
    connect(
        m_1button, SIGNAL(released()),
        this, SLOT(slot1())
    );
    m_layout->addItem(m_1button, 4, 0, 1, 1);
    m_2button = new Plasma::PushButton(this);
    m_2button->setText(QString::fromLatin1("2"));
    connect(
        m_2button, SIGNAL(released()),
        this, SLOT(slot2())
    );
    m_layout->addItem(m_2button, 4, 1, 1, 1);
    m_3button = new Plasma::PushButton(this);
    m_3button->setText(QString::fromLatin1("3"));
    connect(
        m_3button, SIGNAL(released()),
        this, SLOT(slot3())
    );
    m_layout->addItem(m_3button, 4, 2, 1, 1);
    m_equalbutton = new Plasma::PushButton(this);
    m_equalbutton->setText(i18nc("Text of the equals button", "="));
    connect(
        m_equalbutton, SIGNAL(released()),
        this, SLOT(slotEqual())
    );
    m_layout->addItem(m_equalbutton, 4, 3, 2, 1);

    m_0button = new Plasma::PushButton(this);
    m_0button->setText(QString::fromLatin1("0"));
    connect(
        m_0button, SIGNAL(released()),
        this, SLOT(slot0())
    );
    m_layout->addItem(m_0button, 5, 0, 1, 2);
    m_decbutton = new Plasma::PushButton(this);
    m_decbutton->setText(KGlobal::locale()->toLocale().decimalPoint());
    connect(
        m_decbutton, SIGNAL(released()),
        this, SLOT(slotDec())
    );
    m_layout->addItem(m_decbutton, 5, 2, 1, 1);

    setLayout(m_layout);

    adjustSize();

    connect(
        KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()),
        this, SLOT(slotUpdateFonts())
    );
}

void CalculatorAppletWidget::slotClear()
{
    m_label->setText(QString::fromLatin1("0"));
}

void CalculatorAppletWidget::slotDiv()
{
    if (m_label->text() == s_zero) {
        return;
    }
    m_savednumber = m_label->text().toFloat();
    m_operator = CalculatorAppletWidget::OperatorDiv;
    slotClear();
}

void CalculatorAppletWidget::slotMul()
{
    if (m_label->text() == s_zero) {
        return;
    }
    m_savednumber = m_label->text().toFloat();
    m_operator = CalculatorAppletWidget::OperatorMul;
    slotClear();
}

void CalculatorAppletWidget::slotClearAll()
{
    m_savednumber = 0.0;
    m_operator = CalculatorAppletWidget::OperatorNone;
    m_label->setText(QString::fromLatin1("0"));
}

void CalculatorAppletWidget::slot7()
{
    m_label->setText(kAddNumber(m_label->text(), 7));
}

void CalculatorAppletWidget::slot8()
{
    m_label->setText(kAddNumber(m_label->text(), 8));
}

void CalculatorAppletWidget::slot9()
{
    m_label->setText(kAddNumber(m_label->text(), 9));
}

void CalculatorAppletWidget::slotMinus()
{
    if (m_label->text() == s_zero) {
        return;
    }
    m_savednumber = m_label->text().toFloat();
    m_operator = CalculatorAppletWidget::OperatorMinus;
    slotClear();
}

void CalculatorAppletWidget::slot4()
{
    m_label->setText(kAddNumber(m_label->text(), 4));
}

void CalculatorAppletWidget::slot5()
{
    m_label->setText(kAddNumber(m_label->text(), 5));
}

void CalculatorAppletWidget::slot6()
{
    m_label->setText(kAddNumber(m_label->text(), 6));
}

void CalculatorAppletWidget::slotPlus()
{
    if (m_label->text() == s_zero) {
        return;
    }
    m_savednumber = m_label->text().toFloat();
    m_operator = CalculatorAppletWidget::OperatorPlus;
    slotClear();
}

void CalculatorAppletWidget::slot1()
{
    m_label->setText(kAddNumber(m_label->text(), 1));
}

void CalculatorAppletWidget::slot2()
{
    m_label->setText(kAddNumber(m_label->text(), 2));
}

void CalculatorAppletWidget::slot3()
{
    m_label->setText(kAddNumber(m_label->text(), 3));
}

void CalculatorAppletWidget::slotEqual()
{
    switch (m_operator) {
        case CalculatorAppletWidget::OperatorNone: {
            break;
        }
        case CalculatorAppletWidget::OperatorDiv: {
            const qreal currentnumber = m_label->text().toFloat();
            m_label->setText(QString::number(m_savednumber / currentnumber));
            m_operator = CalculatorAppletWidget::OperatorNone;
            break;
        }
        case CalculatorAppletWidget::OperatorMul: {
            const qreal currentnumber = m_label->text().toFloat();
            m_label->setText(QString::number(m_savednumber * currentnumber));
            m_operator = CalculatorAppletWidget::OperatorNone;
            break;
        }
        case CalculatorAppletWidget::OperatorMinus: {
            const qreal currentnumber = m_label->text().toFloat();
            m_label->setText(QString::number(m_savednumber - currentnumber));
            m_operator = CalculatorAppletWidget::OperatorNone;
            break;
        }
        case CalculatorAppletWidget::OperatorPlus: {
            const qreal currentnumber = m_label->text().toFloat();
            m_label->setText(QString::number(m_savednumber + currentnumber));
            m_operator = CalculatorAppletWidget::OperatorNone;
            break;
        }
    }
}

void CalculatorAppletWidget::slot0()
{
    m_label->setText(kAddNumber(m_label->text(), 0));
}

void CalculatorAppletWidget::slotDec()
{
    const QString currenttext = m_label->text();
    if (currenttext.contains(s_decimal)) {
        return;
    }
    m_label->setText(currenttext + s_decimal);
}

void CalculatorAppletWidget::slotUpdateFonts()
{
    m_label->setFont(kLabelFont());
}


CalculatorApplet::CalculatorApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_calculatorwidget(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_calculator");
    setAspectRatioMode(Plasma::AspectRatioMode::KeepAspectRatio);
    setStatus(Plasma::ItemStatus::AcceptingInputStatus);
    setPopupIcon("accessories-calculator");

    m_calculatorwidget = new CalculatorAppletWidget(this);
}

CalculatorApplet::~CalculatorApplet()
{
    delete m_calculatorwidget;
}

QGraphicsWidget* CalculatorApplet::graphicsWidget()
{
    return m_calculatorwidget;
}

// TODO: two ideas come into mind:
// 1. increment/decrement the number on mouse wheel event
// 2. instead of calling slots directly trigger buttons press and then release as indicator why the
// number changed (even tho it will happen very quickly)
void CalculatorApplet::keyPressEvent(QKeyEvent *event)
{
    bool eventhandled = false;
    switch (event->key()) {
        case Qt::Key_0: {
            m_calculatorwidget->slot0();
            eventhandled = true;
            break;
        }
        case Qt::Key_1: {
            m_calculatorwidget->slot1();
            eventhandled = true;
            break;
        }
        case Qt::Key_2: {
            m_calculatorwidget->slot2();
            eventhandled = true;
            break;
        }
        case Qt::Key_3: {
            m_calculatorwidget->slot3();
            eventhandled = true;
            break;
        }
        case Qt::Key_4: {
            m_calculatorwidget->slot4();
            eventhandled = true;
            break;
        }
        case Qt::Key_5: {
            m_calculatorwidget->slot5();
            eventhandled = true;
            break;
        }
        case Qt::Key_6: {
            m_calculatorwidget->slot6();
            eventhandled = true;
            break;
        }
        case Qt::Key_7: {
            m_calculatorwidget->slot7();
            eventhandled = true;
            break;
        }
        case Qt::Key_8: {
            m_calculatorwidget->slot8();
            eventhandled = true;
            break;
        }
        case Qt::Key_9: {
            m_calculatorwidget->slot9();
            eventhandled = true;
            break;
        }
        case Qt::Key_Escape: {
            m_calculatorwidget->slotClearAll();
            eventhandled = true;
            break;
        }
        case Qt::Key_Delete: {
            m_calculatorwidget->slotClear();
            eventhandled = true;
            break;
        }
        case Qt::Key_Plus: {
            m_calculatorwidget->slotPlus();
            eventhandled = true;
            break;
        }
        case Qt::Key_Minus: {
            m_calculatorwidget->slotMinus();
            eventhandled = true;
            break;
        }
        case Qt::Key_Asterisk: {
            m_calculatorwidget->slotMul();
            eventhandled = true;
            break;
        }
        case Qt::Key_Slash: {
            m_calculatorwidget->slotDiv();
            eventhandled = true;
            break;
        }
        case Qt::Key_Period: {
            m_calculatorwidget->slotDec();
            eventhandled = true;
            break;
        }
        case Qt::Key_Equal:
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            m_calculatorwidget->slotEqual();
            eventhandled = true;
            break;
        }
        default: {
            break;
        }
    }
    if (eventhandled) {
        event->accept();
    } else {
        Plasma::PopupApplet::keyPressEvent(event);
    }
}

#include "moc_calculator.cpp"
#include "calculator.moc"

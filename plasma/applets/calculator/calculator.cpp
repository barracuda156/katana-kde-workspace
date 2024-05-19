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
#include <Plasma/Theme>
#include <KDebug>

static const QString s_decimal = QString::fromLatin1(".");
static const QString s_zero = QString::fromLatin1("0");
// hard-limit
static const int s_limit = 9;

static QString kLimitNumber(const QString &string)
{
    return string.mid(0, s_limit);
}

static QString kDoubleNumber(const float number)
{
    return QString::number(number, 'g', s_limit);
}

static QString kAddNumber(const QString &string, const short number)
{
    if (string == s_zero) {
        return QString::number(number);
    }
    return kLimitNumber(string + QString::number(number));
}

#if !defined(Q_MOC_RUN)
template<class T>
class CalculatorWidgetBase : public T
{
public:
    CalculatorWidgetBase(QGraphicsWidget *parent)
        : T(parent),
        m_textcolor(Plasma::Theme::TextColor),
        m_alignment(Qt::AlignCenter),
        m_fontscale(0.5)
    {
    }

    void setup(const Plasma::Theme::ColorRole textcolor, const Qt::Alignment alignment, const qreal fontscale)
    {
        m_textcolor = textcolor;
        m_alignment = alignment;
        m_fontscale = fontscale;
        T::update();
    }

    void setPaintText(const QString &text)
    {
        m_painttext = text;
        T::update();
    }

    QString paintText() const
    {
        return m_painttext;
    }

protected:
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) final
    {
        T::paint(p, option, widget);
        const QRectF rect(QPointF(0, 0), T::size());
        QFont textfont = KGlobalSettings::generalFont();
        textfont.setPointSize(qMax(qreal(textfont.pointSize()), rect.height()) * m_fontscale);
        p->setPen(Plasma::Theme::defaultTheme()->color(m_textcolor));
        QFontMetricsF fontmetrics(textfont);
        while (fontmetrics.width(m_painttext) > rect.width()) {
            textfont.setPointSize(textfont.pointSize() - 1);
            fontmetrics = QFontMetricsF(textfont);
        }
        p->setFont(textfont);
        p->drawText(rect, m_alignment, m_painttext);
    }

private:
    QString m_painttext;
    Plasma::Theme::ColorRole m_textcolor;
    Qt::Alignment m_alignment;
    qreal m_fontscale;
};

class CalculatorButton : public CalculatorWidgetBase<Plasma::PushButton>
{
public:
    CalculatorButton(QGraphicsWidget *parent)
        : CalculatorWidgetBase<Plasma::PushButton>(parent)
    {
        setup(Plasma::Theme::ButtonTextColor, Qt::AlignCenter, 0.5);
    }
};

class CalculatorLabel : public CalculatorWidgetBase<Plasma::Label>
{
public:
    CalculatorLabel(QGraphicsWidget *parent)
        : CalculatorWidgetBase<Plasma::Label>(parent)
    {
        setup(Plasma::Theme::TextColor, Qt::AlignRight | Qt::AlignVCenter, 0.8);
    }
};

#endif // Q_MOC_RUN

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

    void addToNumber(const short number);

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
    CalculatorLabel* m_label;
    CalculatorButton* m_cbutton;
    CalculatorButton* m_divbutton;
    CalculatorButton* m_mulbutton;
    CalculatorButton* m_acbutton;
    CalculatorButton* m_7button;
    CalculatorButton* m_8button;
    CalculatorButton* m_9button;
    CalculatorButton* m_minusbutton;
    CalculatorButton* m_4button;
    CalculatorButton* m_5button;
    CalculatorButton* m_6button;
    CalculatorButton* m_plusbutton;
    CalculatorButton* m_1button;
    CalculatorButton* m_2button;
    CalculatorButton* m_3button;
    CalculatorButton* m_equalbutton;
    CalculatorButton* m_0button;
    CalculatorButton* m_decbutton;
    double m_savednumber;
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
    m_label = new CalculatorLabel(m_frame);
    m_label->setPaintText(s_zero);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_framelayout->addItem(m_label);
    m_layout->addItem(m_frame, 0, 0, 1, 4);
    m_layout->setRowStretchFactor(0, 2);

    m_cbutton = new CalculatorButton(this);
    m_cbutton->setPaintText(i18nc("Text of the clear button", "C"));
    connect(
        m_cbutton, SIGNAL(released()),
        this, SLOT(slotClear())
    );
    m_layout->addItem(m_cbutton, 1, 0, 1, 1);
    m_divbutton = new CalculatorButton(this);
    m_divbutton->setPaintText(i18nc("Text of the division button", "÷"));
    connect(
        m_divbutton, SIGNAL(released()),
        this, SLOT(slotDiv())
    );
    m_layout->addItem(m_divbutton, 1, 1, 1, 1);
    m_mulbutton = new CalculatorButton(this);
    m_mulbutton->setPaintText(i18nc("Text of the multiplication button", "×"));
    connect(
        m_mulbutton, SIGNAL(released()),
        this, SLOT(slotMul())
    );
    m_layout->addItem(m_mulbutton, 1, 2, 1, 1);
    m_acbutton = new CalculatorButton(this);
    m_acbutton->setPaintText(i18nc("Text of the all clear button", "AC"));
    connect(
        m_acbutton, SIGNAL(released()),
        this, SLOT(slotClearAll())
    );
    m_layout->addItem(m_acbutton, 1, 3, 1, 1);
    m_layout->setRowStretchFactor(1, 1);

    m_7button = new CalculatorButton(this);
    m_7button->setPaintText(QString::fromLatin1("7"));
    connect(
        m_7button, SIGNAL(released()),
        this, SLOT(slot7())
    );
    m_layout->addItem(m_7button, 2, 0, 1, 1);
    m_8button = new CalculatorButton(this);
    m_8button->setPaintText(QString::fromLatin1("8"));
    connect(
        m_8button, SIGNAL(released()),
        this, SLOT(slot8())
    );
    m_layout->addItem(m_8button, 2, 1, 1, 1);
    m_9button = new CalculatorButton(this);
    m_9button->setPaintText(QString::fromLatin1("9"));
    connect(
        m_9button, SIGNAL(released()),
        this, SLOT(slot9())
    );
    m_layout->addItem(m_9button, 2, 2, 1, 1);
    m_minusbutton = new CalculatorButton(this);
    m_minusbutton->setPaintText(i18nc("Text of the minus button", "-"));
    connect(
        m_minusbutton, SIGNAL(released()),
        this, SLOT(slotMinus())
    );
    m_layout->addItem(m_minusbutton, 2, 3, 1, 1);
    m_layout->setRowStretchFactor(2, 1);

    m_4button = new CalculatorButton(this);
    m_4button->setPaintText(QString::fromLatin1("4"));
    connect(
        m_4button, SIGNAL(released()),
        this, SLOT(slot4())
    );
    m_layout->addItem(m_4button, 3, 0, 1, 1);
    m_5button = new CalculatorButton(this);
    m_5button->setPaintText(QString::fromLatin1("5"));
    connect(
        m_5button, SIGNAL(released()),
        this, SLOT(slot5())
    );
    m_layout->addItem(m_5button, 3, 1, 1, 1);
    m_6button = new CalculatorButton(this);
    m_6button->setPaintText(QString::fromLatin1("6"));
    connect(
        m_6button, SIGNAL(released()),
        this, SLOT(slot6())
    );
    m_layout->addItem(m_6button, 3, 2, 1, 1);
    m_plusbutton = new CalculatorButton(this);
    m_plusbutton->setPaintText(i18nc("Text of the plus button", "+"));
    connect(
        m_plusbutton, SIGNAL(released()),
        this, SLOT(slotPlus())
    );
    m_layout->addItem(m_plusbutton, 3, 3, 1, 1);
    m_layout->setRowStretchFactor(3, 1);

    m_1button = new CalculatorButton(this);
    m_1button->setPaintText(QString::fromLatin1("1"));
    connect(
        m_1button, SIGNAL(released()),
        this, SLOT(slot1())
    );
    m_layout->addItem(m_1button, 4, 0, 1, 1);
    m_2button = new CalculatorButton(this);
    m_2button->setPaintText(QString::fromLatin1("2"));
    connect(
        m_2button, SIGNAL(released()),
        this, SLOT(slot2())
    );
    m_layout->addItem(m_2button, 4, 1, 1, 1);
    m_3button = new CalculatorButton(this);
    m_3button->setPaintText(QString::fromLatin1("3"));
    connect(
        m_3button, SIGNAL(released()),
        this, SLOT(slot3())
    );
    m_layout->addItem(m_3button, 4, 2, 1, 1);
    m_equalbutton = new CalculatorButton(this);
    m_equalbutton->setPaintText(i18nc("Text of the equals button", "="));
    connect(
        m_equalbutton, SIGNAL(released()),
        this, SLOT(slotEqual())
    );
    m_layout->addItem(m_equalbutton, 4, 3, 2, 1);
    m_layout->setRowStretchFactor(4, 1);

    m_0button = new CalculatorButton(this);
    m_0button->setPaintText(s_zero);
    connect(
        m_0button, SIGNAL(released()),
        this, SLOT(slot0())
    );
    m_layout->addItem(m_0button, 5, 0, 1, 2);
    m_decbutton = new CalculatorButton(this);
    m_decbutton->setPaintText(KGlobal::locale()->toLocale().decimalPoint());
    connect(
        m_decbutton, SIGNAL(released()),
        this, SLOT(slotDec())
    );
    m_layout->addItem(m_decbutton, 5, 2, 1, 1);
    m_layout->setRowStretchFactor(5, 1);

    setLayout(m_layout);

    adjustSize();

    connect(
        KGlobalSettings::self(), SIGNAL(kdisplayFontChanged()),
        this, SLOT(slotUpdateFonts())
    );
}

void CalculatorAppletWidget::addToNumber(const short number)
{
    m_label->setPaintText(kLimitNumber(kDoubleNumber(m_label->paintText().toDouble() + number)));
}

void CalculatorAppletWidget::slotClear()
{
    m_label->setPaintText(s_zero);
}

void CalculatorAppletWidget::slotDiv()
{
    if (m_label->paintText() == s_zero) {
        return;
    }
    m_savednumber = m_label->paintText().toDouble();
    m_operator = CalculatorAppletWidget::OperatorDiv;
    slotClear();
}

void CalculatorAppletWidget::slotMul()
{
    if (m_label->paintText() == s_zero) {
        return;
    }
    m_savednumber = m_label->paintText().toDouble();
    m_operator = CalculatorAppletWidget::OperatorMul;
    slotClear();
}

void CalculatorAppletWidget::slotClearAll()
{
    m_savednumber = 0.0;
    m_operator = CalculatorAppletWidget::OperatorNone;
    m_label->setPaintText(s_zero);
}

void CalculatorAppletWidget::slot7()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 7));
}

void CalculatorAppletWidget::slot8()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 8));
}

void CalculatorAppletWidget::slot9()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 9));
}

void CalculatorAppletWidget::slotMinus()
{
    if (m_label->paintText() == s_zero) {
        return;
    }
    m_savednumber = m_label->paintText().toDouble();
    m_operator = CalculatorAppletWidget::OperatorMinus;
    slotClear();
}

void CalculatorAppletWidget::slot4()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 4));
}

void CalculatorAppletWidget::slot5()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 5));
}

void CalculatorAppletWidget::slot6()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 6));
}

void CalculatorAppletWidget::slotPlus()
{
    if (m_label->paintText() == s_zero) {
        return;
    }
    m_savednumber = m_label->paintText().toDouble();
    m_operator = CalculatorAppletWidget::OperatorPlus;
    slotClear();
}

void CalculatorAppletWidget::slot1()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 1));
}

void CalculatorAppletWidget::slot2()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 2));
}

void CalculatorAppletWidget::slot3()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 3));
}

void CalculatorAppletWidget::slotEqual()
{
    switch (m_operator) {
        case CalculatorAppletWidget::OperatorNone: {
            break;
        }
        case CalculatorAppletWidget::OperatorDiv: {
            const double currentnumber = m_label->paintText().toDouble();
            m_label->setPaintText(kLimitNumber(kDoubleNumber(m_savednumber / currentnumber)));
            m_operator = CalculatorAppletWidget::OperatorNone;
            break;
        }
        case CalculatorAppletWidget::OperatorMul: {
            const double currentnumber = m_label->paintText().toDouble();
            m_label->setPaintText(kLimitNumber(kDoubleNumber(m_savednumber * currentnumber)));
            m_operator = CalculatorAppletWidget::OperatorNone;
            break;
        }
        case CalculatorAppletWidget::OperatorMinus: {
            const double currentnumber = m_label->paintText().toDouble();
            m_label->setPaintText(kLimitNumber(kDoubleNumber(m_savednumber - currentnumber)));
            m_operator = CalculatorAppletWidget::OperatorNone;
            break;
        }
        case CalculatorAppletWidget::OperatorPlus: {
            const double currentnumber = m_label->paintText().toDouble();
            m_label->setPaintText(kLimitNumber(kDoubleNumber(m_savednumber + currentnumber)));
            m_operator = CalculatorAppletWidget::OperatorNone;
            break;
        }
    }
}

void CalculatorAppletWidget::slot0()
{
    m_label->setPaintText(kAddNumber(m_label->paintText(), 0));
}

void CalculatorAppletWidget::slotDec()
{
    const QString currenttext = m_label->paintText();
    if (currenttext.contains(s_decimal) || (currenttext.size() + 1) >= s_limit) {
        return;
    }
    m_label->setPaintText(currenttext + s_decimal);
}

void CalculatorAppletWidget::slotUpdateFonts()
{
    m_label->update();
    m_cbutton->update();
    m_divbutton->update();
    m_mulbutton->update();
    m_acbutton->update();
    m_7button->update();
    m_8button->update();
    m_9button->update();
    m_minusbutton->update();
    m_4button->update();
    m_5button->update();
    m_6button->update();
    m_plusbutton->update();
    m_1button->update();
    m_2button->update();
    m_3button->update();
    m_equalbutton->update();
    m_0button->update();
    m_decbutton->update();
}


CalculatorApplet::CalculatorApplet(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
    m_calculatorwidget(nullptr)
{
    KGlobal::locale()->insertCatalog("plasma_applet_calculator");
    setAspectRatioMode(Plasma::AspectRatioMode::KeepAspectRatio);
    setStatus(Plasma::ItemStatus::ActiveStatus);
    setPopupIcon("accessories-calculator");
}

CalculatorApplet::~CalculatorApplet()
{
    delete m_calculatorwidget;
}

void CalculatorApplet::init()
{
    Plasma::PopupApplet::init();

    m_calculatorwidget = new CalculatorAppletWidget(this);
}

QGraphicsWidget* CalculatorApplet::graphicsWidget()
{
    return m_calculatorwidget;
}

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

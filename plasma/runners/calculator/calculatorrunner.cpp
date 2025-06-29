/*
 *   Copyright (C) 2007 Barış Metin <baris@pardus.org.tr>
 *   Copyright (C) 2006 David Faure <faure@kde.org>
 *   Copyright (C) 2007 Richard Moore <rich@kde.org>
 *   Copyright (C) 2010 Matteo Agostinelli <agostinelli@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "calculatorrunner.h"
#include "qalculate_engine.h"

#include <KIcon>
#include <KDebug>

#include <Plasma/QueryMatch>

CalculatorRunner::CalculatorRunner( QObject* parent, const QVariantList &args )
    : Plasma::AbstractRunner(parent, args)
{
    m_engine = new QalculateEngine();
    setSpeed(SlowSpeed);

    setObjectName( QLatin1String("Calculator" ));
    setIgnoredTypes(Plasma::RunnerContext::Directory | Plasma::RunnerContext::File |
                         Plasma::RunnerContext::NetworkLocation | Plasma::RunnerContext::Executable |
                         Plasma::RunnerContext::ShellCommand);

    QString description = i18n("Calculates the value of :q: when :q: is made up of numbers and "
                               "mathematical symbols such as +, -, /, * and ^.");
    addSyntax(Plasma::RunnerSyntax(":q:", description));
    addSyntax(Plasma::RunnerSyntax("=:q:", description));
    addSyntax(Plasma::RunnerSyntax(":q:=", description));
}

CalculatorRunner::~CalculatorRunner()
{
    delete m_engine;
}

void CalculatorRunner::powSubstitutions(QString& cmd)
{
    if (cmd.contains("e+", Qt::CaseInsensitive)) {
        cmd = cmd.replace("e+", "*10^", Qt::CaseInsensitive);
    }

    if (cmd.contains("e-", Qt::CaseInsensitive)) {
        cmd = cmd.replace("e-", "*10^-", Qt::CaseInsensitive);
    }

    // the below code is scary mainly because we have to honor priority
    // honor decimal numbers and parenthesis.
    while (cmd.contains('^')) {
        int where = cmd.indexOf('^');
        cmd = cmd.replace(where, 1, ',');
        int preIndex = where - 1;
        int postIndex = where + 1;
        int count = 0;

        QChar decimalSymbol = KGlobal::locale()->toLocale().decimalPoint();
        // avoid out of range on weird commands
        preIndex = qMax(0, preIndex);
        postIndex = qMin(postIndex, cmd.length()-1);

        // go backwards looking for the beginning of the number or expression
        while (preIndex != 0) {
            QChar current = cmd.at(preIndex);
            QChar next = cmd.at(preIndex-1);
            // kDebug() << "index " << preIndex << " char " << current;
            if (current == ')') {
                count++;
            } else if (current == '(') {
                count--;
            } else {
                if (((next <= '9' ) && (next >= '0')) || next == decimalSymbol) {
                    preIndex--;
                    continue;
                }
            }
            if (count == 0) {
                // check for functions
                if (!((next <= 'z' ) && (next >= 'a'))) {
                    break;
                }
            }
            preIndex--;
        }

       //go forwards looking for the end of the number or expression
        count = 0;
        while (postIndex != cmd.size() - 1) {
            QChar current=cmd.at(postIndex);
            QChar next=cmd.at(postIndex + 1);

            //check for functions
            if ((count == 0) && (current <= 'z') && (current >= 'a')) {
                postIndex++;
                continue;
            }

            if (current == '(') {
                count++;
            } else if (current == ')') {
                count--;
            } else {
                if (((next <= '9' ) && (next >= '0')) || next == decimalSymbol) {
                    postIndex++;
                    continue;
                 }
            }
            if (count == 0) {
                break;
            }
            postIndex++;
        }

        preIndex = qMax(0, preIndex);
        postIndex = qMin(postIndex, cmd.length());

        cmd.insert(preIndex,"pow(");
        // +1 +4 == next position to the last number after we add 4 new characters pow(
        cmd.insert(postIndex + 1 + 4, ')');
        // kDebug() << "from" << preIndex << " to " << postIndex << " got: " << cmd;
    }
}

void CalculatorRunner::hexSubstitutions(QString& cmd)
{
    if (cmd.contains("0x")) {
        //Append +0 so that the calculator can serve also as a hex converter
        cmd.append("+0");
        bool ok;
        int pos = 0;
        QString hex;

        while (cmd.contains("0x")) {
            hex.clear();
            pos = cmd.indexOf("0x", pos);

            for (int q = 0; q < cmd.size(); q++) {//find end of hex number
                QChar current = cmd[pos+q+2];
                if (((current <= '9' ) && (current >= '0')) || ((current <= 'F' ) && (current >= 'A')) || ((current <= 'f' ) && (current >= 'a'))) { //Check if valid hex sign
                    hex[q] = current;
                } else {
                    break;
                }
            }
            cmd = cmd.replace(pos, 2+hex.length(), QString::number(hex.toInt(&ok,16))); //replace hex with decimal
        }
    }
}

void CalculatorRunner::userFriendlySubstitutions(QString& cmd)
{
    const QChar decimalSymbol = KGlobal::locale()->toLocale().decimalPoint();
    if (cmd.contains(decimalSymbol, Qt::CaseInsensitive)) {
         cmd = cmd.replace(decimalSymbol, QChar('.'), Qt::CaseInsensitive);
    }
}

void CalculatorRunner::match(Plasma::RunnerContext &context)
{
    const QString term = context.query();
    QString cmd = term;

    // no meanless space between friendly guys: helps simplify code
    cmd = cmd.trimmed().remove(' ');

    if (cmd.length() < 3) {
        return;
    }

    bool toHex = cmd.startsWith(QLatin1String("hex="));
    bool startsWithEquals = !toHex && cmd[0] == '=';

    if (toHex || startsWithEquals) {
        cmd.remove(0, cmd.indexOf('=') + 1);
    } else if (cmd.endsWith('=')) {
        cmd.chop(1);
    } else {
        bool foundDigit = false;
        for (int i = 0; i < cmd.length(); ++i) {
            QChar c = cmd.at(i);
            if (c.isLetter()) {
                // not just numbers and symbols, so we return
                return;
            }
            if (c.isDigit()) {
                foundDigit = true;
            }
        }
        if (!foundDigit) {
            return;
        }
    }

    if (cmd.isEmpty()) {
        return;
    }

    userFriendlySubstitutions(cmd);

    QString result = calculate(cmd);
    if (!result.isEmpty() && result != cmd) {
        if (toHex) {
            result = "0x" + QString::number(result.toInt(), 16).toUpper();
        }

        Plasma::QueryMatch match(this);
        match.setIcon(KIcon("accessories-calculator"));
        match.setText(result);
        match.setData(result);
        match.setId(term);
        match.setEnabled(false);
        context.addMatch(match);
    }
}

QMimeData* CalculatorRunner::mimeDataForMatch(const Plasma::QueryMatch &match)
{
    QMimeData *result = new QMimeData();
    result->setText(match.text());
    return result;
}

QString CalculatorRunner::calculate(const QString& term)
{
    const QChar decimalSymbol = KGlobal::locale()->toLocale().decimalPoint();
    QString result = m_engine->evaluate(term);
    return result.replace('.', decimalSymbol, Qt::CaseInsensitive);
}

#include "moc_calculatorrunner.cpp"

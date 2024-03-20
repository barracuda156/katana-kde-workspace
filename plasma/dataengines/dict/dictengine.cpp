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


#include "dictengine.h"

#include <KDebug>
#include <KLocale>
#include <KIO/NetAccess>
#include <QJsonDocument>

DictEngine::DictEngine(QObject* parent, const QVariantList& args)
    : Plasma::DataEngine(parent, args)
{
}

bool DictEngine::sourceRequestEvent(const QString &query)
{
    // qDebug() << Q_FUNC_INFO << query;

    setData(query, QString("text"), QString());
    setData(query, QString("definition"), QString());
    setData(query, QString("example"), QString());
    setData(QString("list-dictionaries"), QString("dictionaries"), QString());

    const QStringList splitquery = query.split(QLatin1Char(':'));
    QString queryword = query;
    if (splitquery.size() == 2) {
        queryword = splitquery.at(1);
    }

    if (queryword.isEmpty()) {
        return false;
    } else if (queryword.contains(' ')) {
        setError(query, QLatin1String("Only words can be queried"));
        return true;
    }

    const KUrl queryurl = QString::fromLatin1("https://api.dictionaryapi.dev/api/v2/entries/en/") + queryword;
    KIO::StoredTransferJob *kiojob = KIO::storedGet(queryurl, KIO::HideProgressInfo);
    kiojob->setAutoDelete(false);
    kiojob->setProperty("dictquery", query);
    connect(kiojob, SIGNAL(finished(KJob*)), this, SLOT(slotFinished(KJob*)));
    return true;
}

void DictEngine::slotFinished(KJob *kjob)
{
    KIO::StoredTransferJob *kiojob = qobject_cast<KIO::StoredTransferJob*>(kjob);
    const QString query = kiojob->property("dictquery").toString();
    if (kiojob->error() != KJob::NoError) {
        kWarning() << "KIO job failed";
        setError(query, QLatin1String("Cannot get meaning"));
        kiojob->deleteLater();
        return;
    }

    const QJsonDocument jsondocument = QJsonDocument::fromJson(kiojob->data());
    kiojob->deleteLater();
    if (jsondocument.isNull()) {
        kWarning() << jsondocument.errorString();
        setError(query, QLatin1String("Cannot parse JSON"));
        return;
    }

    const QVariantList rootlist = jsondocument.toVariant().toList();
    if (rootlist.isEmpty()) {
        setError(query, QLatin1String("Unexpected JSON data"));
        return;
    }
    const QVariantList meaningslist = rootlist.first().toMap().value("meanings").toList();
    if (meaningslist.isEmpty()) {
        setError(query, QLatin1String("Unexpected meanings data"));
        return;
    }
    // qDebug() << Q_FUNC_INFO << "meanings" << meaningslist;
    const QVariantList definitionslist = meaningslist.first().toMap().value("definitions").toList();
    if (definitionslist.isEmpty()) {
        setError(query, QLatin1String("Unexpected definitions data"));
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
    // qDebug() << Q_FUNC_INFO << "meaning" << meaning;

    setData(query, QString("text"), meaning);
    setData(query, QString("definition"), definition);
    setData(query, QString("example"), example);
    setData(QString("list-dictionaries"), QString("dictionaries"), QString("en"));
}

void DictEngine::setError(const QString &query, const QString &message)
{
    setData(query, QString("text"), QString::fromLatin1("<p>\n<dl><b>%1</b>\n</dl></p>\n").arg(message));
    setData(query, QString("definition"), QString());
    setData(query, QString("example"), QString());
    setData(QString("list-dictionaries"), QString("dictionaries"), QString("en"));
}

#include "moc_dictengine.cpp"

/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef __MAIN_H__
#define __MAIN_H__

#include <kcmodule.h>
#include <ksharedconfig.h>
#include <ktemporaryfile.h>

#include <QLabel>

#include "kwin_interface.h"

#include "ui_main.h"

class KPluginSelector;
class KActionCollection;

namespace KWin
{

class KWinCompositingConfig : public KCModule
{
    Q_OBJECT
public:
    KWinCompositingConfig(QWidget *parent, const QVariantList &args);

    QString quickHelp() const final;

public slots:
    void currentTabChanged(int tab);

    void load() final;
    void save() final;
    void defaults() final;
    void reparseConfiguration(const QByteArray& conf);

    void loadGeneralTab();
    void loadEffectsTab();
    void loadAdvancedTab();
    void saveGeneralTab();
    void saveEffectsTab();
    bool saveAdvancedTab();

    void checkLoadedEffects();
    void configChanged(bool reinitCompositing);
    void initEffectSelector();

    void warn(QString message, QString details, QString dontAgainKey);

private slots:
    void alignGuiToCompositingType(int compositingType);
    void toggleEffectShortcutChanged(const QKeySequence &seq);
    void updateStatusUI(bool compositingIsPossible);
    void showDetailedEffectLoadingInformation();
    void blockFutureWarnings();

private:
    bool effectEnabled(const QString& effect, const KConfigGroup& cfg) const;

    KSharedConfigPtr mKWinConfig;
    Ui::KWinCompositingConfig ui;

    QMap<QString, QString> mPreviousConfig;
    KTemporaryFile mTmpConfigFile;
    KSharedConfigPtr mTmpConfig;
    KActionCollection* m_actionCollection;
    QAction *m_showDetailedErrors;
    QAction *m_dontShowAgain;
    QString m_externErrorDetails;
};

} // namespace

#endif

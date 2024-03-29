#ifndef NETPREF_H
#define NETPREF_H

#include <kcmodule.h>

#include <QGroupBox>
#include <QCheckBox>

class KIntNumInput;

class KIOPreferences : public KCModule
{
    Q_OBJECT

public:
    KIOPreferences(QWidget *parent, const QVariantList &args);
    ~KIOPreferences();

    void load();
    void save();
    void defaults();

    QString quickHelp() const;

protected Q_SLOTS:
    void configChanged() { emit changed(true); }

private:
    QGroupBox* gb_Ftp;
    QGroupBox* gb_Timeout;
    QGroupBox* gb_Misc;

    QCheckBox* cb_ftpEnablePasv;
    QCheckBox* cb_ftpMarkPartial;

    KIntNumInput* sb_serverConnect;
    KIntNumInput* sb_serverResponse;

    KIntNumInput* sb_minimumKeepSize;
    QCheckBox* cb_AutoResume;
};

#endif // NETPREF_H

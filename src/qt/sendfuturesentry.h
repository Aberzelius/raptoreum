// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SENDFUTURESENTRY_H
#define BITCOIN_QT_SENDFUTURESENTRY_H

#include "walletmodel.h"

#include <QStackedWidget>

class WalletModel;
class PlatformStyle;

namespace Ui {
    class SendFuturesEntry;
}

/**
 * A single entry in the dialog for sending bitcoins.
 * Stacked widget, with different UIs for payment requests
 * with a strong payee identity.
 */
class SendFuturesEntry : public QStackedWidget
{
    Q_OBJECT

public:
    explicit SendFuturesEntry(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~SendFuturesEntry();

    void setModel(WalletModel *model);
    bool validate();
    SendFuturesRecipient getValue();

    /** Return whether the entry is still empty and unedited */
    bool isClear();

    void setValue(const SendFuturesRecipient &value);
    void setAddress(const QString &address);

    /** Set up the tab chain manually, as Qt messes up the tab chain by default in some cases
     *  (issue https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget *setupTabChain(QWidget *prev);

    void setFocus();

public Q_SLOTS:
    void clear();

Q_SIGNALS:
    void removeEntry(SendFuturesEntry *entry);
    void payAmountChanged();
    void payFromChanged(const QString &address);
    void subtractFeeFromAmountChanged();

private Q_SLOTS:
    void deleteClicked();
    void on_payTo_textChanged(const QString &address);
    void on_addressBookButton_clicked();
    void on_pasteButton_clicked();
    void updateDisplayUnit();
    void updateLockTimeField(const QDateTime &dateTime);
    void setupPayFrom();

private:
    SendFuturesRecipient recipient;
    Ui::SendFuturesEntry *ui;
    WalletModel *model;
    const PlatformStyle *platformStyle;

    bool updateLabel(const QString &address);
    
};

#endif // BITCOIN_QT_SENDFUTURESENTRY_H

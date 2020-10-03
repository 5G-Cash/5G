// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OVERVIEWPAGE_H
#define BITCOIN_QT_OVERVIEWPAGE_H

#include "amount.h"
#include "uint256.h"

#include <QWidget>
#include <memory>

#include "walletmodel.h"

#include <QSettings>

#define NUM_ITEMS 5
#define TOKEN_SIZE 40
#define MARGIN 5
#define SYMBOL_WIDTH 80

#define TX_SIZE 40
#define DECORATION_SIZE 20
#define DATE_WIDTH 110
#define TYPE_WIDTH 140
#define AMOUNT_WIDTH 205

#define BUTTON_ICON_SIZE 24

class ClientModel;
class TransactionFilterProxy;
class TxViewDelegate;
class PlatformStyle;
class WalletModel;

namespace Ui {
    class OverviewPage;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(const PlatformStyle *platformStyle, QWidget *parent = 0);
    ~OverviewPage();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);
    void showOutOfSyncWarning(bool fShow);
    void UpdatePropertyBalance(unsigned int propertyId, uint64_t available, uint64_t reserved);

public Q_SLOTS:
    void setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,const CAmount& stake,
                    const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance);
    void setSigmaBalance();
    //void updateElysium();
    //void reinitElysium();

Q_SIGNALS:
    void transactionClicked(const QModelIndex &index);
    void enabledTorChanged();
    void elysiumTransactionClicked(const uint256& txid);

private:
    Ui::OverviewPage *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;
    CAmount currentBalance;
    CAmount currentUnconfirmedBalance;
    CAmount currentImmatureBalance;
    CAmount currentStake;
    CAmount currentWatchOnlyBalance;
    CAmount currentWatchUnconfBalance;
    CAmount currentWatchImmatureBalance;
    CAmount currentSigmaBalance;
    CAmount currentSigmaUnconfirmedBalance;

    QSettings settings;

    TxViewDelegate *txdelegate;
    std::unique_ptr<TransactionFilterProxy> filter;

private Q_SLOTS:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);
    void handleEnabledTorChanged();
    void updateAlerts(const QString &warnings);
    void updateWatchOnlyLabels(bool showWatchOnly);
    void updateCoins(const std::vector<CMintMeta>& spendable, const std::vector<CMintMeta>& pending);
};

#endif // BITCOIN_QT_OVERVIEWPAGE_H

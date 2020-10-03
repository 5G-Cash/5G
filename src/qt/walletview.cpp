// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletview.h"

#include "addressbookpage.h"
#include "zerocoinpage.h"
#include "sigmadialog.h"
#include "zc2sigmapage.h"
#include "askpassphrasedialog.h"
#include "bitcoingui.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "platformstyle.h"
#include "receivecoinsdialog.h"
#include "sendcoinsdialog.h"
#include "metadexcanceldialog.h"
#include "metadexdialog.h"
#include "signverifymessagedialog.h"
#include "tradehistorydialog.h"
#include "transactiontablemodel.h"
#include "transactionview.h"
#include "walletmodel.h"

#include "ui_interface.h"

#ifdef ENABLE_ELYSIUM
#include "lookupaddressdialog.h"
#include "lookupspdialog.h"
#include "lookuptxdialog.h"
#include "sendmpdialog.h"
#include "txhistorydialog.h"

#include "../elysium/elysium.h"
#endif

#include <QAction>
#include <QActionGroup>
#include <QDebug>
#include <QDialog>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

WalletView::WalletView(const PlatformStyle *platformStyle, QWidget *parent):
    QStackedWidget(parent),
    clientModel(0),
    walletModel(0),
    overviewPage(0),
#ifdef ENABLE_ELYSIUM
    elysiumTransactionsView(0),
    transactionTabs(0),
    sendElysiumView(0),
    sendCoinsTabs(0),
#endif
    sigmaView(0),
    blankSigmaView(0),
    zc2SigmaPage(0),
    indexTransactionsView(0),
    platformStyle(platformStyle)
{
    overviewPage = new OverviewPage(platformStyle);
    transactionsPage = new QWidget(this);
#ifdef ENABLE_ELYSIUM
    elyAssetsPage = new ElyAssetsDialog();
#endif
    receiveCoinsPage = new ReceiveCoinsDialog(platformStyle);
    usedSendingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
    usedReceivingAddressesPage = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);
    zerocoinPage = new ZerocoinPage(platformStyle, ZerocoinPage::ForEditing, this);
    sigmaPage = new QWidget(this);
    zc2SigmaPage = new Zc2SigmaPage(platformStyle, this);
    sendCoinsPage = new QWidget(this);
#ifdef ENABLE_ELYSIUM
    toolboxPage = new QWidget(this);
#endif
    fivegnodeListPage = new FivegnodeList(platformStyle);

    setupTransactionPage();
    setupSendCoinPage();
#ifdef ENABLE_ELYSIUM
    setupToolboxPage();
#endif
    setupSigmaPage();

    addWidget(overviewPage);
#ifdef ENABLE_ELYSIUM
    addWidget(elyAssetsPage);
#endif
    addWidget(transactionsPage);
    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);
    addWidget(zerocoinPage);
    addWidget(sigmaPage);
    addWidget(zc2SigmaPage);
#ifdef ENABLE_ELYSIUM
    addWidget(toolboxPage);
#endif
    addWidget(fivegnodeListPage);

    // Clicking on a transaction on the overview pre-selects the transaction on the transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(focusBitcoinHistoryTab(QModelIndex)));
    #ifdef ENABLE_ELYSIUM
    connect(overviewPage, SIGNAL(elysiumTransactionClicked(uint256)), this, SLOT(focusElysiumTransaction(uint256)));
    #endif
}

WalletView::~WalletView()
{
}

void WalletView::setupTransactionPage()
{
    // Create Index transactions list
    indexTransactionList = new TransactionView(platformStyle);

    connect(indexTransactionList, SIGNAL(doubleClicked(QModelIndex)), indexTransactionList, SLOT(showDetails()));
    connect(indexTransactionList, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

    // Create export panel for Index transactions
    auto exportButton = new QPushButton(tr("&Export"));

    exportButton->setToolTip(tr("Export the data in the current tab to a file"));

    if (platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }

    connect(exportButton, SIGNAL(clicked()), indexTransactionList, SLOT(exportClicked()));

    auto exportLayout = new QHBoxLayout();
    exportLayout->addStretch();
    exportLayout->addWidget(exportButton);

    // Compose transaction list and export panel together
    auto indexLayout = new QVBoxLayout();
    indexLayout->addWidget(indexTransactionList);
    indexLayout->addLayout(exportLayout);

    indexTransactionsView = new QWidget();
    indexTransactionsView->setLayout(indexLayout);

#ifdef ENABLE_ELYSIUM
    // Create tabs for transaction categories
    if (isElysiumEnabled()) {
        elysiumTransactionsView = new TXHistoryDialog();

        transactionTabs = new QTabWidget();
        transactionTabs->addTab(indexTransactionsView, tr("Fiveg"));
        transactionTabs->addTab(elysiumTransactionsView, tr("Elysium"));
    }
#endif

    // Set layout for transaction page
    auto pageLayout = new QVBoxLayout();

#ifdef ENABLE_ELYSIUM
    if (transactionTabs) {
        pageLayout->addWidget(transactionTabs);
    } else
#endif
        pageLayout->addWidget(indexTransactionsView);

    transactionsPage->setLayout(pageLayout);
}

void WalletView::setupSendCoinPage()
{
    sendZcoinView = new SendCoinsDialog(platformStyle);

    connect(sendZcoinView, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

#ifdef ENABLE_ELYSIUM
    // Create tab for coin type
    if (isElysiumEnabled()) {
        sendElysiumView = new SendMPDialog(platformStyle);

        sendCoinsTabs = new QTabWidget();
        sendCoinsTabs->addTab(sendZcoinView, tr("Fiveg"));
        sendCoinsTabs->addTab(sendElysiumView, tr("Elysium"));
    }
#endif

    // Set layout for send coin page
    auto pageLayout = new QVBoxLayout();

#ifdef ENABLE_ELYSIUM
    if (sendCoinsTabs) {
        pageLayout->addWidget(sendCoinsTabs);
    } else
#endif
        pageLayout->addWidget(sendZcoinView);

    sendCoinsPage->setLayout(pageLayout);
}

void WalletView::setupSigmaPage()
{
    // Set layout for Sigma page
    auto pageLayout = new QVBoxLayout();

    if (pwalletMain->IsHDSeedAvailable()) {
        sigmaView = new SigmaDialog(platformStyle);
        connect(sigmaView, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));
        pageLayout->addWidget(sigmaView);
        sigmaPage->setLayout(pageLayout);
    } else {
        blankSigmaView = new BlankSigmaDialog();
        pageLayout->addWidget(blankSigmaView);
        sigmaPage->setLayout(pageLayout);
    }
}

#ifdef ENABLE_ELYSIUM
void WalletView::setupToolboxPage()
{
    // Create tools widget
    auto lookupAddress = new LookupAddressDialog();
    auto lookupProperty = new LookupSPDialog();
    auto lookupTransaction = new LookupTXDialog();

    // Create tab for each tool
    auto tabs = new QTabWidget();

    tabs->addTab(lookupAddress, tr("Lookup Address"));
    tabs->addTab(lookupProperty, tr("Lookup Property"));
    tabs->addTab(lookupTransaction, tr("Lookup Transaction"));

    // Set layout for toolbox page
    auto pageLayout = new QVBoxLayout();
    pageLayout->addWidget(tabs);
    toolboxPage->setLayout(pageLayout);
}
#endif

void WalletView::setBitcoinGUI(BitcoinGUI *gui)
{
    if (gui)
    {
        // Clicking on a transaction on the overview page simply sends you to transaction history page
        connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), gui, SLOT(gotoBitcoinHistoryTab()));
        #ifdef ENABLE_ELYSIUM
        connect(overviewPage, SIGNAL(elysiumTransactionClicked(uint256)), gui, SLOT(gotoElysiumHistoryTab()));
        #endif

        // Receive and report messages
        connect(this, SIGNAL(message(QString,QString,unsigned int)), gui, SLOT(message(QString,QString,unsigned int)));

        // Pass through encryption status changed signals
        connect(this, SIGNAL(encryptionStatusChanged(int)), gui, SLOT(setEncryptionStatus(int)));

        // Pass through transaction notifications
        connect(this, SIGNAL(incomingTransaction(QString,int,CAmount,QString,QString,QString)), gui, SLOT(incomingTransaction(QString,int,CAmount,QString,QString,QString)));
    }
}

void WalletView::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;

    overviewPage->setClientModel(clientModel);
    sendZcoinView->setClientModel(clientModel);
    fivegnodeListPage->setClientModel(clientModel);
#ifdef ENABLE_ELYSIUM
    elyAssetsPage->setClientModel(clientModel);
#endif
    if (pwalletMain->IsHDSeedAvailable()) {
        sigmaView->setClientModel(clientModel);
    }
    zc2SigmaPage->setClientModel(clientModel);

#ifdef ENABLE_ELYSIUM
    if (elysiumTransactionsView) {
        elysiumTransactionsView->setClientModel(clientModel);
    }

    if (sendElysiumView) {
        sendElysiumView->setClientModel(clientModel);
    }
#endif
}

void WalletView::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;

    // Put transaction list in tabs
    indexTransactionList->setModel(walletModel);
    overviewPage->setWalletModel(walletModel);
    receiveCoinsPage->setModel(walletModel);
    zerocoinPage->setModel(walletModel->getAddressTableModel());
    if (pwalletMain->IsHDSeedAvailable()) {
        sigmaView->setWalletModel(walletModel);
    }
    zc2SigmaPage->createModel();
    usedReceivingAddressesPage->setModel(walletModel->getAddressTableModel());
    usedSendingAddressesPage->setModel(walletModel->getAddressTableModel());
    fivegnodeListPage->setWalletModel(walletModel);
    sendZcoinView->setModel(walletModel);
    zc2SigmaPage->setWalletModel(walletModel);
#ifdef ENABLE_ELYSIUM
    elyAssetsPage->setWalletModel(walletModel);

    if (elysiumTransactionsView) {
        elysiumTransactionsView->setWalletModel(walletModel);
    }

    if (sendElysiumView) {
        sendElysiumView->setWalletModel(walletModel);
    }
#endif

    if (walletModel)
    {
        // Receive and pass through messages from wallet model
        connect(walletModel, SIGNAL(message(QString,QString,unsigned int)), this, SIGNAL(message(QString,QString,unsigned int)));

        // Handle changes in encryption status
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SIGNAL(encryptionStatusChanged(int)));
        updateEncryptionStatus();

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(processNewTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

        // Show progress dialog
        connect(walletModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)));
    }
}

void WalletView::processNewTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress
    if (!walletModel || !clientModel || clientModel->inInitialBlockDownload())
        return;

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QModelIndex index = ttm->index(start, 0, parent);
    QString address = ttm->data(index, TransactionTableModel::AddressRole).toString();
    QString label = ttm->data(index, TransactionTableModel::LabelRole).toString();

    Q_EMIT incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address, label);
}

void WalletView::gotoOverviewPage()
{
    setCurrentWidget(overviewPage);
}

#ifdef ENABLE_ELYSIUM
void WalletView::gotoElyAssetsPage()
{
    setCurrentWidget(elyAssetsPage);
}
#endif

void WalletView::gotoHistoryPage()
{
    setCurrentWidget(transactionsPage);
}

#ifdef ENABLE_ELYSIUM
void WalletView::gotoElysiumHistoryTab()
{
    if (!transactionTabs) {
        return;
    }

    setCurrentWidget(transactionsPage);
    transactionTabs->setCurrentIndex(1);
}
#endif

void WalletView::gotoBitcoinHistoryTab()
{
    setCurrentWidget(transactionsPage);

#ifdef ENABLE_ELYSIUM
    if (transactionTabs) {
        transactionTabs->setCurrentIndex(0);
    }
#endif
}

#ifdef ENABLE_ELYSIUM
void WalletView::focusElysiumTransaction(const uint256& txid)
{
    if (!elysiumTransactionsView) {
        return;
    }

    gotoElysiumHistoryTab();
    elysiumTransactionsView->focusTransaction(txid);
}
#endif

void WalletView::focusBitcoinHistoryTab(const QModelIndex &idx)
{
    gotoBitcoinHistoryTab();
    indexTransactionList->focusTransaction(idx);
}

void WalletView::gotoFivegnodePage()
{
    setCurrentWidget(fivegnodeListPage);
}

void WalletView::gotoReceiveCoinsPage()
{
    setCurrentWidget(receiveCoinsPage);
}

void WalletView::gotoZerocoinPage()
{
    setCurrentWidget(zerocoinPage);
}

void WalletView::gotoSigmaPage()
{
    setCurrentWidget(sigmaPage);
}

void WalletView::gotoZc2SigmaPage()
{
    if (pwalletMain->IsHDSeedAvailable()) {
        setCurrentWidget(zc2SigmaPage);
    } else {
        setCurrentWidget(sigmaPage);
    }
}

#ifdef ENABLE_ELYSIUM
void WalletView::gotoToolboxPage()
{
    setCurrentWidget(toolboxPage);
}
#endif

void WalletView::gotoSendCoinsPage(QString addr)
{
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty()){
        sendZcoinView->setAddress(addr);
    }
}

void WalletView::gotoSignMessageTab(QString addr)
{
    // calls show() in showTab_SM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
    // calls show() in showTab_VM()
    SignVerifyMessageDialog *signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
#ifdef ENABLE_ELYSIUM
    if (sendCoinsTabs) {
        sendCoinsTabs->setCurrentIndex(0);
    }
#endif

    return sendZcoinView->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus()
{
    Q_EMIT encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void WalletView::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    updateEncryptionStatus();
}

void WalletView::backupWallet()
{
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Backup Wallet"), QString(),
        tr("Wallet Data (*.dat)"), NULL);

    if (filename.isEmpty())
        return;

    if (!walletModel->backupWallet(filename)) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(filename),
            CClientUIInterface::MSG_ERROR);
        }
    else {
        Q_EMIT message(tr("Backup Successful"), tr("The wallet data was successfully saved to %1.").arg(filename),
            CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::unlockWallet(bool iconClicked,bool fStaking)
{
    if(!walletModel)
        return;
    // Unlock wallet when requested by wallet model
	if (walletModel->getEncryptionStatus() == WalletModel::Locked || (!iconClicked && walletModel->getEncryptionStatus() == WalletModel::UnlockedForStaking))
            {
        AskPassphraseDialog dlg(fStaking ? AskPassphraseDialog::UnlockManual : AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void WalletView::lockWallet()
{
    if(!walletModel)
        return;
    walletModel->lockWallet();
}

void WalletView::usedSendingAddresses()
{
    if(!walletModel)
        return;

    usedSendingAddressesPage->show();
    usedSendingAddressesPage->raise();
    usedSendingAddressesPage->activateWindow();
}

void WalletView::usedReceivingAddresses()
{
    if(!walletModel)
        return;

    usedReceivingAddressesPage->show();
    usedReceivingAddressesPage->raise();
    usedReceivingAddressesPage->activateWindow();
}

void WalletView::showProgress(const QString &title, int nProgress)
{
    if (nProgress == 0)
    {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    }
    else if (nProgress == 100)
    {
        if (progressDialog)
        {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    }
    else if (progressDialog)
        progressDialog->setValue(nProgress);
}
WalletModel* WalletView::getWalletModel(){
    return walletModel;
}

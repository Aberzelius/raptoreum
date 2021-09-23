// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2014-2019 The Dash Core developers
// Copyright (c) 2020-2021 The Raptoreum developers 
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sendfuturesentry.h"
#include "ui_sendfuturesentry.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "walletmodel.h"

#include "future/fee.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QComboBox>
#include <QStandardItemModel>

SendFuturesEntry::SendFuturesEntry(const PlatformStyle *_platformStyle, QWidget *parent) :
    QStackedWidget(parent),
    ui(new Ui::SendFuturesEntry),
    model(0),
    platformStyle(_platformStyle)
{
    ui->setupUi(this);

    //Setup maturity locktime datetime field
    ui->ftxLockTime->setDateTime( QDateTime::currentDateTime() );
    ui->ftxLockTime->setMinimumDate( QDate::currentDate() );

    // This field holds the maturity locktime, programatically controlled, no need to show to user
    ui->ftxLockTimeField->hide();

    //hide unused UI elements for futures
    ui->deleteButton->hide();
    ui->deleteButton_is->hide();
    ui->deleteButton_s->hide();
    ui->checkboxSubtractFeeFromAmount->hide();

    setCurrentWidget(ui->SendFutures);

    if (platformStyle->getUseExtraSpacing())
        ui->payToLayout->setSpacing(4);
#if QT_VERSION >= 0x040700
    ui->addAsLabel->setPlaceholderText(tr("Enter a label for this address to add it to your address book"));
#endif

    // These icons are needed on Mac also!
    ui->addressBookButton->setIcon(QIcon(":/icons/address-book"));
    ui->pasteButton->setIcon(QIcon(":/icons/editpaste"));
    ui->deleteButton->setIcon(QIcon(":/icons/remove"));
    ui->deleteButton_is->setIcon(QIcon(":/icons/remove"));
    ui->deleteButton_s->setIcon(QIcon(":/icons/remove"));
      
    // normal raptoreum address field
    GUIUtil::setupAddressWidget(ui->payTo, this);
    // just a label for displaying raptoreum address(es)
    ui->payTo_is->setFont(GUIUtil::fixedPitchFont());
    ui->payFrom->setFont(GUIUtil::fixedPitchFont());

    // Connect signals
    connect(ui->payAmount, SIGNAL(valueChanged()), this, SIGNAL(payAmountChanged()));
    connect(ui->checkboxSubtractFeeFromAmount, SIGNAL(toggled(bool)), this, SIGNAL(subtractFeeFromAmountChanged()));
    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->deleteButton_is, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->deleteButton_s, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    //Connect signals for future tx pay from field
    connect(ui->payFrom, SIGNAL(currentTextChanged(const QString &)), this, SIGNAL(payFromChanged(const QString &)));

    //Connect signals for new maturity fields
    connect (ui->ftxLockTime, SIGNAL (dateTimeChanged (QDateTime)), this , SLOT (getLockTime (QDateTime)));

}

SendFuturesEntry::~SendFuturesEntry()
{
    delete ui;
}

void SendFuturesEntry::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void SendFuturesEntry::on_addressBookButton_clicked()
{
    if(!model)
        return;
    AddressBookPage dlg(platformStyle, AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if(dlg.exec())
    {
        ui->payTo->setText(dlg.getReturnValue());
        ui->payAmount->setFocus();
    }
}

void SendFuturesEntry::on_payTo_textChanged(const QString &address)
{
    updateLabel(address);
}

void SendFuturesEntry::setModel(WalletModel *_model)
{
    this->model = _model;

    if (_model && _model->getOptionsModel()) {
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        connect(_model, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)), this, SLOT(balanceChange(CAmount)));
        setupPayFrom(0);
    }

    clear();
}

void SendFuturesEntry::clear()
{
    // clear UI elements for future payment
    ui->payTo->clear();
    ui->addAsLabel->clear();
    ui->payAmount->clear();
    ui->checkboxSubtractFeeFromAmount->setCheckState(Qt::Unchecked);
    ui->messageTextLabel->clear();
    ui->messageTextLabel->hide();
    ui->messageLabel->hide();
    ui->ftxMaturity->setValue(-1);
    ui->ftxLockTime->setDateTime( QDateTime::currentDateTime() );
    // clear UI elements for unauthenticated payment request
    ui->payTo_is->clear();
    ui->memoTextLabel_is->clear();
    ui->payAmount_is->clear();
    // clear UI elements for authenticated payment request
    ui->payTo_s->clear();
    ui->memoTextLabel_s->clear();
    ui->payAmount_s->clear();

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void SendFuturesEntry::deleteClicked()
{
    Q_EMIT removeEntry(this);
}

bool SendFuturesEntry::validate()
{

    if (!model)
        return false;

    // Check input validity
    bool retval = true;

    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized())
        return retval;

    if (!model->validateAddress(ui->payFrom->currentText()))
    {
        retval = false;
    }

    if (!model->validateAddress(ui->payTo->text()))
    {
        ui->payTo->setValid(false);
        retval = false;
    }

    if (!ui->payAmount->validate())
    {
        retval = false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->value(0) <= 0)
    {
        ui->payAmount->setValid(false);
        retval = false;
    }

    // Reject dust outputs:
    if (retval && GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        retval = false;
    }

    return retval;
}

SendCoinsRecipient SendFuturesEntry::getValue()
{
    // Payment request
    if (recipient.paymentRequest.IsInitialized())
        return recipient;

    // Future payment
    recipient.payFrom = ui->payFrom->currentText();
    recipient.address = ui->payTo->text();
    recipient.label = ui->addAsLabel->text();
    recipient.amount = ui->payAmount->value();
    recipient.maturity = ui->ftxMaturity->value();
    recipient.locktime = ui->ftxLockTimeField->text().toInt();
    recipient.message = ui->messageTextLabel->text();
    recipient.fSubtractFeeFromAmount = (ui->checkboxSubtractFeeFromAmount->checkState() == Qt::Checked);

    return recipient;
}

QWidget *SendFuturesEntry::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, ui->payTo);
    QWidget::setTabOrder(ui->payTo, ui->addAsLabel);
    QWidget *w = ui->payAmount->setupTabChain(ui->addAsLabel);
    QWidget::setTabOrder(w, ui->checkboxSubtractFeeFromAmount);
    QWidget::setTabOrder(ui->checkboxSubtractFeeFromAmount, ui->addressBookButton);
    QWidget::setTabOrder(ui->addressBookButton, ui->pasteButton);
    QWidget::setTabOrder(ui->pasteButton, ui->deleteButton);
    return ui->deleteButton;
}

void SendFuturesEntry::setValue(const SendCoinsRecipient &value)
{
    recipient = value;

    if (recipient.paymentRequest.IsInitialized()) // payment request
    {
        if (recipient.authenticatedMerchant.isEmpty()) // unauthenticated
        {
            ui->payTo_is->setText(recipient.address);
            ui->memoTextLabel_is->setText(recipient.message);
            ui->payAmount_is->setValue(recipient.amount);
            ui->payAmount_is->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_UnauthenticatedPaymentRequest);
        }
        else // authenticated
        {
            ui->payTo_s->setText(recipient.authenticatedMerchant);
            ui->memoTextLabel_s->setText(recipient.message);
            ui->payAmount_s->setValue(recipient.amount);
            ui->payAmount_s->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_AuthenticatedPaymentRequest);
        }
    }
    else // normal payment
    {
        // message
        ui->messageTextLabel->setText(recipient.message);
        ui->messageTextLabel->setVisible(!recipient.message.isEmpty());
        ui->messageLabel->setVisible(!recipient.message.isEmpty());

        ui->addAsLabel->clear();
        ui->payTo->setText(recipient.address); // this may set a label from addressbook
        if (!recipient.label.isEmpty()) // if a label had been set from the addressbook, don't overwrite with an empty label
            ui->addAsLabel->setText(recipient.label);
        ui->payAmount->setValue(recipient.amount);
    }
}

void SendFuturesEntry::setAddress(const QString &address)
{
    ui->payTo->setText(address);
    ui->payAmount->setFocus();
}

bool SendFuturesEntry::isClear()
{
    return ui->payTo->text().isEmpty() && ui->payTo_is->text().isEmpty() && ui->payTo_s->text().isEmpty();
}

void SendFuturesEntry::setFocus()
{
    ui->payTo->setFocus();
}

void SendFuturesEntry::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        // Update payAmount with the current unit
        ui->payAmount->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_is->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_s->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());

        setupPayFrom(ui->payFrom->currentIndex());
    }
}

bool SendFuturesEntry::updateLabel(const QString &address)
{
    if(!model)
        return false;

    // Fill in label from address book, if address has an associated label
    QString associatedLabel = model->getAddressTableModel()->labelForAddress(address);
    if(!associatedLabel.isEmpty())
    {
        ui->addAsLabel->setText(associatedLabel);
        return true;
    }

    return false;
}

//Calculate Future tx locktime
void SendFuturesEntry::getLockTime(const QDateTime & dateTime)
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    //Calculate seconds from now to the chosen datetime value
    qint64 futureTime = currentDateTime.secsTo(dateTime) > 0 ? currentDateTime.secsTo(dateTime) : -1;
    QString int_string = QString::number(futureTime);
    
    //set the seconds in this field for handling
    ui->ftxLockTimeField->setText(int_string);

    //QTextStream(stdout) << futureTime << endl;
}

//Future coin control: update combobox
void SendFuturesEntry::setupPayFrom(int selected)
{
    if (!model || !model->getOptionsModel())
        return;

    CAmount futureFee = getFutureFees();
    CAmount nMinAmount = futureFee;

    std::map<CTxDestination, CAmount> balances = model->getAddressBalances();

    if(balances.empty())
    {
        ui->payFrom->setDisabled(true);
        return;
    }

    //Build table for dropdown
    QStandardItemModel *itemModel = new QStandardItemModel( this );
    QStringList horzHeaders;
    horzHeaders << "Address" << "Label" << BitcoinUnits::getAmountColumnTitle(model->getOptionsModel()->getDisplayUnit());

    QList<QStandardItem *> placeholder;
    placeholder.append(new QStandardItem( "Select a Raptoreum address" ) );
    itemModel->appendRow(placeholder);

    for (auto& balance : balances) {
        if (balance.second >= nMinAmount) {
            QList<QStandardItem *> items;
            QString associatedLabel = model->getAddressTableModel()->labelForAddress(balance.first);

            QStandardItem *balanceAmount = new QStandardItem();
            balanceAmount->setData(quint64(balance.second));
            balanceAmount->setText(BitcoinUnits::format(model->getOptionsModel()->getDisplayUnit(), balance.second, false, BitcoinUnits::separatorAlways));

            items.append(new QStandardItem( GUIUtil::HtmlEscape(CBitcoinAddress(balance.first).ToString()) ) );
            items.append(new QStandardItem( associatedLabel ));
            items.append(balanceAmount);
            itemModel->appendRow(items);
        }
    }

    itemModel->setHorizontalHeaderLabels( horzHeaders );
    //Table settings
    QTableView* tableView = new QTableView( this );
    tableView->setObjectName("payFromTable");
    tableView->setModel( itemModel );
    tableView->resizeColumnsToContents();
    tableView->setColumnWidth(1,160);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    tableView->setSortingEnabled(true);
    tableView->setFont(GUIUtil::fixedPitchFont());
    tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setAutoScroll(false);
    tableView->hideRow(0);
    
    ui->payFrom->setModel( itemModel );
    ui->payFrom->setView( tableView );

    ui->payFrom->setCurrentIndex(selected);
}

void SendFuturesEntry::balanceChange(const CAmount& balance)
{
    setupPayFrom(0);
}

/*void SendFuturesEntry::updatePayFromBalanceLabel()
{
    if (!model || !model->getOptionsModel())
        return;
    
    std::map<CTxDestination, CAmount> balances = model->getAddressBalances();
    CAmount addressBalance = 0;
    CAmount futureFee = getFutureFees();
    CAmount sendAmount = ui->payAmount->value();

    //find balance for matched address in payfrom field
    for (auto& balance : balances) {
        //std::cout << CBitcoinAddress(balance.first).ToString() << ": " << ui->payFrom->currentText().toStdString() << endl;
        if (CBitcoinAddress(balance.first).ToString() == ui->payFrom->currentText().toStdString()) {
            addressBalance += balance.second;
        }
    }

    ui->ftxPayFromBalance->setText(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), addressBalance));

    if(addressBalance > sendAmount + futureFee) {
        ui->ftxPayFromBalance->setStyleSheet(GUIUtil::getThemedStyleQString(GUIUtil::ThemedStyle::TS_PRIMARY));
    } else {
        ui->ftxPayFromBalance->setStyleSheet(GUIUtil::getThemedStyleQString(GUIUtil::ThemedStyle::TS_ERROR));
    }
}*/
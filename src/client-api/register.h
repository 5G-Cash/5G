// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCOIN_APIREGISTER_H
#define ZCOIN_APIREGISTER_H

/** These are in one header file to avoid creating tons of single-function
 * headers for everything under src/client-api/ */
class CAPITable;

/** Register misc API commands */
void RegisterMiscAPICommands(CAPITable &tableAPI);

/** Register wallet API commands */
void RegisterWalletAPICommands(CAPITable &tableAPI);

/** Register blockchain API commands */
void RegisterBlockchainAPICommands(CAPITable &tableAPI);

/** Register send API commands */
void RegisterSendAPICommands(CAPITable &tableAPI);

/** Register settings API commands */
void RegisterSettingsAPICommands(CAPITable &tableAPI);

/** Register fivegnode API commands */
void RegisterFivegnodeAPICommands(CAPITable &tableAPI);

/** Register sigma API commands */
void RegisterSigmaAPICommands(CAPITable &tableAPI);

static inline void RegisterAllCoreAPICommands(CAPITable &tableAPI)
{
    RegisterMiscAPICommands(tableAPI);
    RegisterWalletAPICommands(tableAPI);
    RegisterBlockchainAPICommands(tableAPI);
    RegisterSendAPICommands(tableAPI);
    RegisterSettingsAPICommands(tableAPI);
    RegisterFivegnodeAPICommands(tableAPI);
    RegisterSigmaAPICommands(tableAPI);
}

#endif

// Copyright (c) 2014-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMSBASE_H
#define BITCOIN_CHAINPARAMSBASE_H

#include <string>
#include <vector>

/**
 * CBaseChainParams defines the base parameters (shared between bitcoin-cli and bitcoind)
 * of a given instance of the Bitcoin system.
 */
class CBaseChainParams
{
public:
    /** BIP70 chain name strings (main, test or regtest) */
    static const std::string MAIN;
    static const std::string TESTNET;
    static const std::string REGTEST;

    const std::string& DataDir() const { return strDataDir; }
    int RPCPort() const { return nRPCPort; }
    int APIAuthREPPort() const { return nAPIAuthREPPort; }
    int APIOpenREPPort() const { return nAPIOpenREPPort; }
    int APIAuthPUBPort() const { return nAPIAuthPUBPort; }
    int APIOpenPUBPort() const { return nAPIOpenPUBPort; }
    std::string APIAddr() const { return nAPIAddr; }

protected:
    CBaseChainParams() {}

    std::string nAPIAddr;
    int nAPIAuthREPPort;
    int nAPIOpenREPPort;
    int nAPIAuthPUBPort;
    int nAPIOpenPUBPort;
    int nRPCPort;
    std::string strDataDir;
};

/**
 * Append the help messages for the chainparams options to the
 * parameter string.
 */
void AppendParamsHelpMessages(std::string& strUsage, bool debugHelp=true);

/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CBaseChainParams& BaseParams();

CBaseChainParams& BaseParams(const std::string& chain);

/** Sets the params returned by Params() to those for the given network. */
void SelectBaseParams(const std::string& chain);

/**
 * Looks for -regtest, -testnet and returns the appropriate BIP70 chain name.
 * @return CBaseChainParams::MAX_NETWORK_TYPES if an invalid combination is given. CBaseChainParams::MAIN by default.
 */
std::string ChainNameFromCommandLine();

/**
 * Checks if the port parameter is the same as any ZMQ port for base params.
 */
#ifdef ENABLE_CLIENTAPI
bool IsZMQPort(int64_t port);
#endif

/**
 * Return true if SelectBaseParamsFromCommandLine() has been called to select
 * a network.
 */
bool AreBaseParamsConfigured();

#endif // BITCOIN_CHAINPARAMSBASE_H

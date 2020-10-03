// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SRC_FIVEGNODECONFIG_H_
#define SRC_FIVEGNODECONFIG_H_

#include "fs.h"

#include <univalue.h>

class CFivegnodeConfig;
extern CFivegnodeConfig fivegnodeConfig;

class CFivegnodeConfig
{

public:

    class CFivegnodeEntry {

    private:
        std::string alias;
        std::string ip;
        std::string privKey;
        std::string txHash;
        std::string outputIndex;
    public:

        CFivegnodeEntry(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex) {
            this->alias = alias;
            this->ip = ip;
            this->privKey = privKey;
            this->txHash = txHash;
            this->outputIndex = outputIndex;
        }

        const std::string& getAlias() const {
            return alias;
        }

        void setAlias(const std::string& alias) {
            this->alias = alias;
        }

        const std::string& getOutputIndex() const {
            return outputIndex;
        }

        void setOutputIndex(const std::string& outputIndex) {
            this->outputIndex = outputIndex;
        }

        const std::string& getPrivKey() const {
            return privKey;
        }

        void setPrivKey(const std::string& privKey) {
            this->privKey = privKey;
        }

        const std::string& getTxHash() const {
            return txHash;
        }

        void setTxHash(const std::string& txHash) {
            this->txHash = txHash;
        }

        const std::string& getIp() const {
            return ip;
        }

        void setIp(const std::string& ip) {
            this->ip = ip;
        }

        UniValue ToJSON(){
            UniValue ret(UniValue::VOBJ);
            UniValue outpoint(UniValue::VOBJ);
            UniValue authorityObj(UniValue::VOBJ);

            std::string authority = getIp();
            std::string ip   = authority.substr(0, authority.find(":"));
            std::string port = authority.substr(authority.find(":")+1, authority.length());

            outpoint.push_back(Pair("txid", getTxHash().substr(0,64)));
            outpoint.push_back(Pair("index", getOutputIndex()));
            authorityObj.push_back(Pair("ip", ip));
            authorityObj.push_back(Pair("port", port));

            ret.push_back(Pair("label", getAlias()));
            ret.push_back(Pair("isMine", true));
            ret.push_back(Pair("outpoint", outpoint));
            ret.push_back(Pair("authority", authorityObj));

            return ret;
        }
    };

    CFivegnodeConfig() {
        entries = std::vector<CFivegnodeEntry>();
    }

    void clear();
    bool read(std::string& strErr);
    void add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex);

    std::vector<CFivegnodeEntry>& getEntries() {
        return entries;
    }

    int getCount() {
        return (int)entries.size();
    }

private:
    std::vector<CFivegnodeEntry> entries;


};


#endif /* SRC_FIVEGNODECONFIG_H_ */

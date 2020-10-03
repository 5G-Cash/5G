// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVEFIVEGNODE_H
#define ACTIVEFIVEGNODE_H

#include "net.h"
#include "key.h"
#include "wallet/wallet.h"

class CActiveFivegnode;

static const int ACTIVE_FIVEGNODE_INITIAL          = 0; // initial state
static const int ACTIVE_FIVEGNODE_SYNC_IN_PROCESS  = 1;
static const int ACTIVE_FIVEGNODE_INPUT_TOO_NEW    = 2;
static const int ACTIVE_FIVEGNODE_NOT_CAPABLE      = 3;
static const int ACTIVE_FIVEGNODE_STARTED          = 4;

extern CActiveFivegnode activeFivegnode;

// Responsible for activating the Fivegnode and pinging the network
class CActiveFivegnode
{
public:
    enum fivegnode_type_enum_t {
        FIVEGNODE_UNKNOWN = 0,
        FIVEGNODE_REMOTE  = 1,
        FIVEGNODE_LOCAL   = 2
    };

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    fivegnode_type_enum_t eType;

    bool fPingerEnabled;

    /// Ping Fivegnode
    bool SendFivegnodePing();

public:
    // Keys for the active Fivegnode
    CPubKey pubKeyFivegnode;
    CKey keyFivegnode;

    // Initialized while registering Fivegnode
    CTxIn vin;
    CService service;

    int nState; // should be one of ACTIVE_FIVEGNODE_XXXX
    std::string strNotCapableReason;

    CActiveFivegnode()
        : eType(FIVEGNODE_UNKNOWN),
          fPingerEnabled(false),
          pubKeyFivegnode(),
          keyFivegnode(),
          vin(),
          service(),
          nState(ACTIVE_FIVEGNODE_INITIAL)
    {}

    /// Manage state of active Fivegnode
    void ManageState();

    // Change state if different and publish update
    void ChangeState(int newState);

    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string GetTypeString() const;

private:
    void ManageStateInitial();
    void ManageStateRemote();
    void ManageStateLocal();
};

#endif

// Copyright (c) 2012-2014 The Bitcoin Core developers
// Copyright (c) 2020 The FivegX Project developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VERSION_H
#define BITCOIN_VERSION_H

/**
 * network protocol versioning
 */

static const int PROTOCOL_VERSION = 90012;

//! initial proto version, to be increased after version/verack negotiation
static const int INIT_PROTO_VERSION = 90011;

//! In this version, 'getheaders' was introduced.
static const int GETHEADERS_VERSION = 90010;

//! disconnect from peers older than this proto version
static const int MIN_PEER_PROTO_VERSION = PROTOCOL_VERSION - 1;

//! disconnect from all older peers after Fivegnode payment HF
static const int MIN_PEER_PROTO_VERSION_AFTER_UPDATE = PROTOCOL_VERSION;

//! nTime field added to CAddress, starting with this version;
//! if possible, avoid requesting addresses nodes older than this
static const int CADDR_TIME_VERSION = 90010;

//! BIP 0031, pong message, is enabled for all versions AFTER this one
static const int BIP0031_VERSION = 90010;

//! "mempool" command, enhanced "getdata" behavior starts with this version
static const int MEMPOOL_GD_VERSION = 90010;

//! "filter*" commands are disabled without NODE_BLOOM after and including this version
static const int NO_BLOOM_VERSION = 90010;

//! "sendheaders" command and announcing blocks with headers starts with this version
static const int SENDHEADERS_VERSION = 90010;

//! "feefilter" tells peers to filter invs to you by fee starts with this version
static const int FEEFILTER_VERSION = 90010;

//! shord-id-based block download starts with this version
static const int SHORT_IDS_BLOCKS_VERSION = 90010;

//! not banning for invalid compact blocks starts with this version
static const int INVALID_CB_NO_BAN_VERSION = 90010;

//! minimum version of official client to connect to
static const int MIN_ZCOIN_CLIENT_VERSION = 1000000; //1.0.0.0

#endif // BITCOIN_VERSION_H

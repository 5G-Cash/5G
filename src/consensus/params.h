// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"
#include <map>
#include <string>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_CSV, // Deployment of BIP68, BIP112, and BIP113.
    DEPLOYMENT_SEGWIT, // Deployment of BIP141, BIP143, and BIP147.

    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;
};

/**
 * Type of chain
 */
enum ChainType {
    chainMain,
    chainTestnet,
    chainRegtest
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    ChainType chainType;

    uint256 hashGenesisBlock;
    /** First subsidy halving */
    int nSubsidyHalvingFirst;
    /** Subsequent subsidy halving intervals */
    int nSubsidyHalvingInterval;
    /** Stop subsidy at this block number */
    int nSubsidyHalvingStopBlock;
    /** Used to check majorities for block version upgrade */
    int nMajorityEnforceBlockUpgrade;
    int nMajorityRejectBlockOutdated;
    int nMajorityWindow;
    uint256 BIP34Hash;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargetting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    uint256 powLimit;
    uint256 posLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int nDgwPastBlocks;
    int nInstantSendKeepLock; // in blocks
    int nFivegnodeMinimumConfirmations;
    int nFivegnodePaymentsStartBlock;
    int nDisableZCoinClientCheckTime;
    int nBlacklistEnableHeight;
    int nBlockLimitUpgradeHeight;
	/** Zerocoin-related block numbers when features are changed */
    int nCheckBugFixedAtBlock;
    int nFivegnodePaymentsBugFixedAtBlock;
	int nSpendV15StartBlock;
	int nSpendV2ID_1, nSpendV2ID_10, nSpendV2ID_25, nSpendV2ID_50, nSpendV2ID_100;

	int nModulusV2StartBlock;
    int nModulusV1MempoolStopBlock;
	int nModulusV1StopBlock;

    int nMultipleSpendInputsInOneTxStartBlock;

    int nDontAllowDupTxsStartBlock;

    // Values for dandelion.

    // The minimum amount of time a Dandelion transaction is embargoed (seconds).
    uint32_t nDandelionEmbargoMinimum;

    // The average additional embargo time beyond the minimum amount (seconds).
    uint32_t nDandelionEmbargoAvgAdd;

    // Maximum number of outbound peers designated as Dandelion destinations.
    uint32_t nDandelionMaxDestinations;
    
    // Expected time between Dandelion routing shuffles (in seconds).
    uint32_t nDandelionShuffleInterval;

    // Probability (percentage) that a Dandelion transaction enters fluff phase.
    uint32_t nDandelionFluff;

    // Values for sigma implementation.

    // The block number after which sigma are accepted.
    int nSigmaStartBlock;

    int nSigmaPaddingBlock;

    int nDisableUnpaddedSigmaBlock;

    // The block number after which old sigma clients are banned.
    int nOldSigmaBanBlock;

    // Number of blocks after nSigmaMintStartBlock during which we still accept zerocoin V2 mints into mempool.
    int nZerocoinV2MintMempoolGracefulPeriod;

    // Number of blocks after nSigmaMintStartBlock during which we still accept zerocoin V2 mints to newly mined blocks.
    int nZerocoinV2MintGracefulPeriod;

    // Number of blocks after nSigmaMintStartBlock during which we still accept zerocoin V2 spend into mempool.
    int nZerocoinV2SpendMempoolGracefulPeriod;

    // Number of blocks after nSigmaMintStartBlock during which we still accept zerocoin V2 spend to newly mined blocks.
    int nZerocoinV2SpendGracefulPeriod;

    // Amount of maximum sigma spend per block.
    unsigned nMaxSigmaInputPerBlock;

    // Value of maximum sigma spend per block.
    int64_t nMaxValueSigmaSpendPerBlock;

    // Amount of maximum sigma spend per transaction.
    unsigned nMaxSigmaInputPerTransaction;

    // Value of maximum sigma spend per transaction.
    int64_t nMaxValueSigmaSpendPerTransaction;

    // Number of blocks with allowed zerocoin to sigma remint transaction (after nSigmaStartBlock)
    int nZerocoinToSigmaRemintWindowSize;

    /** block number to disable zerocoin on consensus level */
    int nDisableZerocoinStartBlock;
	
     // Params for Zawy's LWMA difficulty adjustment algorithm.

    int64_t DifficultyAdjustmentInterval(bool fMTP = false) const { return nPowTargetTimespan / nPowTargetSpacing; }
    uint256 nMinimumChainWork;
	
    // proof-of-stake
    int nFirstPOSBlock;
    int nStakeTimestampMask;
    bool IsMain() const { return chainType == chainMain; }
    bool IsTestnet() const { return chainType == chainTestnet; }
    bool IsRegtest() const { return chainType == chainRegtest; }
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H

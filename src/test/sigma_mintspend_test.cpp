#include "util.h"

#include <stdint.h>
#include <vector>

#include "chainparams.h"
#include "key.h"
#include "main.h"
#include "pubkey.h"
#include "txdb.h"
#include "txmempool.h"
#include "sigma.h"

#include "test/fixtures.h"
#include "test/testutil.h"

#include "wallet/db.h"
#include "wallet/wallet.h"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

BOOST_FIXTURE_TEST_SUITE(sigma_mintspend, ZerocoinTestingSetup200)

/*
* 1. Create one denomination pair and check it can't be spend till 6 conf of mint
* 2. Make one more mint of denom pair and check it can't be spend till 6 conf
* 3. Create two spend transactions using same mint
* 4. Double spend with previous spend in last block
*/
BOOST_AUTO_TEST_CASE(sigma_mintspend_test)
{
    sigma::CSigmaState *sigmaState = sigma::CSigmaState::GetState();
    string denomination;
    vector<uint256> vtxid;
    std::vector<string> denominations = {"0.05", "0.1", "0.5", "1", "10", "25", "100"};

    // Create 400-200+1 = 201 new empty blocks. // consensus.nMintV3SigmaStartBlock = 400
    CreateAndProcessEmptyBlocks(201, scriptPubKey);

    // foreach denom from denominations
    for(auto denomination : denominations)
    {
        printf("Testing denomination %s\n", denomination.c_str());
        string stringError;
        // Make sure that transactions get to mempool
        pwalletMain->SetBroadcastTransactions(true);

        // Verify Mint is successful
        vector<pair<std::string, int>> denominationPairs;
        std::pair<std::string, int> denominationPair(denomination, 1);
        denominationPairs.push_back(denominationPair);
        BOOST_CHECK_MESSAGE(pwalletMain->CreateZerocoinMintModel(
            stringError, denominationPairs, SIGMA), stringError + " - Create Mint failed");

        // Verify Mint gets in the mempool
        BOOST_CHECK_MESSAGE(mempool.size() == 1, "Mint was not added to mempool");

        int previousHeight = chainActive.Height();
        CBlock b = CreateAndProcessBlock({}, scriptPubKey);
        BOOST_CHECK_MESSAGE(previousHeight + 1 == chainActive.Height(), "Block not added to chain");

        previousHeight = chainActive.Height();
        // Add 5 more blocks and verify that Mint can not be spent until 6 blocks verification
        for (int i = 0; i < 5; i++)
        {
            BOOST_CHECK_MESSAGE(!pwalletMain->CreateZerocoinSpendModel(stringError, "", denomination.c_str()), "Spend succeeded although not confirmed by 6 blocks");
            BOOST_CHECK_MESSAGE(stringError == "it has to have at least two mint coins with at least 6 confirmation in order to spend a coin", stringError + " - Incorrect error message");

            CBlock b = CreateAndProcessBlock({}, scriptPubKey);
        }
        BOOST_CHECK_MESSAGE(previousHeight + 5 == chainActive.Height(), "Block not added to chain");

        BOOST_CHECK_MESSAGE(!pwalletMain->CreateZerocoinSpendModel(
            stringError, "", denomination.c_str()),
            "Spend succeeded although not at least two mints");
        BOOST_CHECK_MESSAGE(stringError == "it has to have at least two mint coins with at least 6 confirmation in order to spend a coin", stringError + " - Incorrect error message");


        BOOST_CHECK_MESSAGE(pwalletMain->CreateZerocoinMintModel(
            stringError, denominationPairs, SIGMA), stringError + "Create Mint failed");

        BOOST_CHECK_MESSAGE(mempool.size() == 1, "Mint was not added to mempool");

        previousHeight = chainActive.Height();
        b = CreateAndProcessBlock({}, scriptPubKey);
        BOOST_CHECK_MESSAGE(previousHeight + 1 == chainActive.Height(), "Block not added to chain");


        previousHeight = chainActive.Height();
        //Add 5 more blocks and verify that Mint can not be spent until 6 blocks verification
        for (int i = 0; i < 5; i++)
        {
            BOOST_CHECK_MESSAGE(!pwalletMain->CreateZerocoinSpendModel(stringError, "", denomination.c_str()), "Spend succeeded although not confirmed by 6 blocks");
            BOOST_CHECK_MESSAGE(stringError == "it has to have at least two mint coins with at least 6 confirmation in order to spend a coin", stringError + " - Incorrect error message");
            CBlock b = CreateAndProcessBlock({}, scriptPubKey);
        }

        BOOST_CHECK_MESSAGE(previousHeight + 5 == chainActive.Height(), "Block not added to chain");

        // Create two spend transactions using the same mint.
        BOOST_CHECK_MESSAGE(pwalletMain->CreateZerocoinSpendModel(
            stringError, "", denomination.c_str()), stringError + "Spend failed");
        BOOST_CHECK_MESSAGE(pwalletMain->CreateZerocoinSpendModel(
            stringError, "", denomination.c_str(), true), stringError + " - Spend failed");

        //Try to put two in the same block and it will fail, expect 1
        BOOST_CHECK_MESSAGE(mempool.size() == 1, "Spend was not added to mempool");

        //Verify spend got into mempool
        BOOST_CHECK_MESSAGE(mempool.size() == 1, "Spend was not added to mempool");

        vtxid.clear();
        b = CreateBlock({}, scriptPubKey);
        previousHeight = chainActive.Height();
        BOOST_CHECK_MESSAGE(ProcessBlock(b), "ProcessBlock failed although valid spend inside");
        BOOST_CHECK_MESSAGE(previousHeight + 1 == chainActive.Height(), "Block not added to chain");

        BOOST_CHECK_MESSAGE(mempool.size() == 0, "Mempool not cleared");

        BOOST_CHECK_MESSAGE(pwalletMain->CreateZerocoinSpendModel(stringError, "", denomination.c_str()), stringError + " - Spend failed");

        //Verify spend got into mempool
        BOOST_CHECK_MESSAGE(mempool.size() == 1, "Spend was not added to mempool");

        b = CreateBlock({}, scriptPubKey);
        previousHeight = chainActive.Height();
        BOOST_CHECK_MESSAGE(ProcessBlock(b), "ProcessBlock failed although valid spend inside");
        BOOST_CHECK_MESSAGE(previousHeight + 1 == chainActive.Height(), "Block not added to chain");

        BOOST_CHECK_MESSAGE(mempool.size() == 0, "Mempool not cleared");

        //Test double spend with previous spend in last block
        BOOST_CHECK_MESSAGE(pwalletMain->CreateZerocoinSpendModel(stringError, "", denomination.c_str(), true), "Spend created although double");
        //This confirms that double spend is blocked and cannot enter mempool
        BOOST_CHECK_MESSAGE(mempool.size() == 0, "Mempool not empty although mempool should reject double spend");

        //Temporary disable usedCoinSerials check to force double spend in mempool
        auto tempSerials = sigmaState->containers.usedCoinSerials;
        sigmaState->containers.usedCoinSerials.clear();

        BOOST_CHECK_MESSAGE(pwalletMain->CreateZerocoinSpendModel(stringError, "", denomination.c_str(), true), "Spend created although double");
        BOOST_CHECK_MESSAGE(mempool.size() == 1, "Mempool not set");
        sigmaState->containers.usedCoinSerials = tempSerials;

        BOOST_CHECK_EXCEPTION(CreateBlock({}, scriptPubKey), std::runtime_error, no_check);
        BOOST_CHECK_MESSAGE(mempool.size() == 1, "Mempool not set");
        vtxid.clear();
        mempool.queryHashes(vtxid);
        vtxid.resize(1);
        tempSerials = sigmaState->containers.usedCoinSerials;
        sigmaState->containers.usedCoinSerials.clear();
        CreateBlock(vtxid, scriptPubKey);
        sigmaState->containers.usedCoinSerials = tempSerials;

        mempool.clear();
        previousHeight = chainActive.Height();
        BOOST_CHECK_MESSAGE(ProcessBlock(b), "ProcessBlock failed");
        //This test confirms that a block containing a double spend is rejected and not added in the chain
        BOOST_CHECK_MESSAGE(previousHeight == chainActive.Height(), "Double spend - Block added to chain even though same spend in previous block");

        vtxid.clear();
        mempool.clear();
        sigmaState->Reset();
    }
}
BOOST_AUTO_TEST_SUITE_END()

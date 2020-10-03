#include "util.h"

#include "clientversion.h"
#include "primitives/transaction.h"
#include "random.h"
#include "sync.h"
#include "utilstrencodings.h"
#include "utilmoneystr.h"
#include "test/test_bitcoin.h"

#include <stdint.h>
#include <vector>
#include <iostream>

#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "key.h"
#include "main.h"
#include "miner.h"
#include "pubkey.h"
#include "random.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "rpc/server.h"
#include "rpc/register.h"
#include "zerocoin.h"

#include "test/testutil.h"
#include "test/fixtures.h"

#include "wallet/db.h"
#include "wallet/wallet.h"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

#include "zerocoin.h"
#include "sigma.h"


ZerocoinTestingSetupBase::ZerocoinTestingSetupBase():
    TestingSetup(CBaseChainParams::REGTEST, "1") {
    // Crean sigma state, just in case someone forgot to do so.
    sigma::CSigmaState *sigmaState = sigma::CSigmaState::GetState();
    sigmaState->Reset();

    // Also clean up old zerocoin state.
    CZerocoinState *zerocoinState = CZerocoinState::GetZerocoinState();
    zerocoinState->Reset();
};

ZerocoinTestingSetupBase::~ZerocoinTestingSetupBase() {
    // Clean sigma state after us.
    sigma::CSigmaState *sigmaState = sigma::CSigmaState::GetState();
    sigmaState->Reset();

    // Also clean up old zerocoin state.
    CZerocoinState *zerocoinState = CZerocoinState::GetZerocoinState();
    zerocoinState->Reset();

}

    CBlock ZerocoinTestingSetupBase::CreateBlock(
            const vector<uint256>& tx_ids,
            const CScript& scriptPubKey) {
        const CChainParams& chainparams = Params();
        CBlockTemplate *pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(
            scriptPubKey, tx_ids);
        CBlock block = pblocktemplate->block;

        // IncrementExtraNonce creates a valid coinbase and merkleRoot
        unsigned int extraNonce = 0;
        IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

        while (!CheckProofOfWork(block.GetPoWHash(), block.nBits, chainparams.GetConsensus())){
            ++block.nNonce;
        }

        delete pblocktemplate;
        return block;
    }

    bool ZerocoinTestingSetupBase::ProcessBlock(CBlock &block) {
        const CChainParams& chainparams = Params();
        CValidationState state;
        return ProcessNewBlock(state, chainparams, NULL, &block, true, NULL, false);
    }

    // Create a new block with just given transactions, coinbase paying to
    // scriptPubKey, and try to add it to the current chain.
    CBlock ZerocoinTestingSetupBase::CreateAndProcessBlock(
            const vector<uint256>& tx_ids,
            const CScript& scriptPubKey) {

        CBlock block = CreateBlock(tx_ids, scriptPubKey);
        BOOST_CHECK_MESSAGE(ProcessBlock(block), "Processing block failed");
        return block;
    }

    void ZerocoinTestingSetupBase::CreateAndProcessEmptyBlocks(size_t block_numbers, const CScript& script) {
        while (block_numbers--) {
            CreateAndProcessBlock({}, script);
        }
    }

 ZerocoinTestingSetup200::ZerocoinTestingSetup200()
    {
        BOOST_CHECK(pwalletMain->GetKeyFromPool(pubkey));

        string strAddress = CBitcoinAddress(pubkey.GetID()).ToString();
        pwalletMain->SetAddressBook(CBitcoinAddress(strAddress).Get(), "",
                               ( "receive"));

        //Mine 200 blocks so that we have funds for creating mints and we are over these limits:
        //mBlockHeightConstants["ZC_V1_5_STARTING_BLOCK"] = 150;
        //mBlockHeightConstants["ZC_CHECK_BUG_FIXED_AT_BLOCK"] = 140;
        // Since sigma V3 implementation also over consensus.nMintV3SigmaStartBlock = 180;

        printf("Balance before %ld\n", pwalletMain->GetBalance());
        scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
        for (int i = 0; i < 200; i++)
        {
            CBlock b = CreateAndProcessBlock({}, scriptPubKey);
            coinbaseTxns.push_back(b.vtx[0]);
            LOCK(cs_main);
            {
                LOCK(pwalletMain->cs_wallet);
                pwalletMain->AddToWalletIfInvolvingMe(b.vtx[0], &b, true);
            }
        }

        printf("Balance after 200 blocks: %ld\n", pwalletMain->GetBalance());
    }


 ZerocoinTestingSetup109::ZerocoinTestingSetup109()
    {
        CPubKey newKey;
        BOOST_CHECK(pwalletMain->GetKeyFromPool(newKey));

        string strAddress = CBitcoinAddress(newKey.GetID()).ToString();
        pwalletMain->SetAddressBook(CBitcoinAddress(strAddress).Get(), "",
                               ( "receive"));

        printf("Balance before %ld\n", pwalletMain->GetBalance());
        scriptPubKey = CScript() <<  ToByteVector(newKey/*coinbaseKey.GetPubKey()*/) << OP_CHECKSIG;
        for (int i = 0; i < 109; i++)
        {
            CBlock b = CreateAndProcessBlock({}, scriptPubKey);
            coinbaseTxns.push_back(b.vtx[0]);
            LOCK(cs_main);
            {
                LOCK(pwalletMain->cs_wallet);
                pwalletMain->AddToWalletIfInvolvingMe(b.vtx[0], &b, true);
            }
        }

        printf("Balance after 109 blocks: %ld\n", pwalletMain->GetBalance());
    }

MtpMalformedTestingSetup::MtpMalformedTestingSetup()
    {
        CPubKey newKey;
        BOOST_CHECK(pwalletMain->GetKeyFromPool(newKey));

        string strAddress = CBitcoinAddress(newKey.GetID()).ToString();
        pwalletMain->SetAddressBook(CBitcoinAddress(strAddress).Get(), "",
                               ( "receive"));

        printf("Balance before %ld\n", pwalletMain->GetBalance());
        scriptPubKey = CScript() <<  ToByteVector(newKey/*coinbaseKey.GetPubKey()*/) << OP_CHECKSIG;
        bool mtp = false;
        CBlock b;
        for (int i = 0; i < 150; i++)
        {
            b = CreateAndProcessBlock({}, scriptPubKey, mtp);
            coinbaseTxns.push_back(b.vtx[0]);
            LOCK(cs_main);
            {
                LOCK(pwalletMain->cs_wallet);
                pwalletMain->AddToWalletIfInvolvingMe(b.vtx[0], &b, true);
            }
        }
        printf("Balance after 150 blocks: %ld\n", pwalletMain->GetBalance());
    }

        CBlock MtpMalformedTestingSetup::CreateBlock(
            const vector<uint256>& tx_ids,
            const CScript& scriptPubKeyMtpMalformed, bool mtp = false) {
        const CChainParams& chainparams = Params();
        CBlockTemplate *pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKeyMtpMalformed, tx_ids);
        CBlock& block = pblocktemplate->block;

        // IncrementExtraNonce creates a valid coinbase and merkleRoot
        unsigned int extraNonce = 0;
        IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);

        while (!CheckProofOfWork(block.GetPoWHash(), block.nBits, chainparams.GetConsensus())){
            ++block.nNonce;
        }

        while (!CheckProofOfWork(block.GetPoWHash(), block.nBits, chainparams.GetConsensus())){
                ++block.nNonce;
        }
        

        //delete pblocktemplate;
        return block;
    }


    // Create a new block with just given transactions, coinbase paying to
    // scriptPubKeyMtpMalformed, and try to add it to the current chain.
    CBlock MtpMalformedTestingSetup::CreateAndProcessBlock(
            const vector<uint256>& tx_ids,
            const CScript& scriptPubKeyMtpMalformed,
            bool mtp = false) {

        CBlock block = CreateBlock(tx_ids, scriptPubKeyMtpMalformed, mtp);
        BOOST_CHECK_MESSAGE(ProcessBlock(block), "Processing block failed");
        return block;
    }

#include "chainlocks.h"
#include "chain.h"
#include "fivegnodeman.h"
#include "key.h"
#include "hash.h"
#include "main.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(chainlocks_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(process_new_chainlock_marks_block)
{
    // Setup fake masternode quorum
    mnodeman.Clear();
    std::vector<CKey> keys(CHAINLOCKS_QUORUM_SIZE);
    for (size_t i = 0; i < CHAINLOCKS_QUORUM_SIZE; ++i) {
        keys[i].MakeNewKey(true);
        CFivegnode mn;
        mn.vin = CTxIn(COutPoint(uint256S(strprintf("0x%02x", i + 1)), 0));
        mn.pubKeyFivegnode = keys[i].GetPubKey();
        mnodeman.Add(mn);
    }

    CBlockIndex index;
    index.nHeight = 5;
    uint256 hash = uint256S("0x1");
    mapBlockIndex[hash] = &index;

    // Build quorum signatures for the block
    CHashWriter ss(SER_GETHASH, 0);
    ss << index.nHeight;
    ss << hash;
    uint256 msgHash = ss.GetHash();

    std::vector<unsigned char> aggSig;
    for (size_t i = 0; i < CHAINLOCKS_THRESHOLD; ++i) {
        std::vector<unsigned char> sig;
        keys[i].Sign(msgHash, sig);
        aggSig.push_back(sig.size());
        aggSig.insert(aggSig.end(), sig.begin(), sig.end());
    }

    g_chainlocks.ProcessNewChainLock(index.nHeight, hash, aggSig);

    BOOST_CHECK_EQUAL(g_chainlocks.GetBestChainLockHash(), hash);
    BOOST_CHECK(index.fChainLocked);

    mapBlockIndex.erase(hash);
    mnodeman.Clear();
}

BOOST_AUTO_TEST_SUITE_END()

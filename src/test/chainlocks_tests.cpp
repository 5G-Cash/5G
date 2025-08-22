#include "chainlocks.h"
#include "chain.h"
#include "main.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(chainlocks_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(process_new_chainlock_marks_block)
{
    CBlockIndex index;
    index.nHeight = 5;
    uint256 hash = uint256S("0x1");
    mapBlockIndex[hash] = &index;

    g_chainlocks.ProcessNewChainLock(index.nHeight, hash, std::vector<unsigned char>());

    BOOST_CHECK_EQUAL(g_chainlocks.GetBestChainLockHash(), hash);

    mapBlockIndex.erase(hash);
}

BOOST_AUTO_TEST_SUITE_END()

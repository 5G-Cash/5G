#include "fivegnode-payments.h"
#include "chainparams.h"
#include "main.h"
#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(blockreward_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(block_reward_exact_match)
{
    const Consensus::Params& params = Params().GetConsensus();
    CAmount reward = GetBlockSubsidy(2501, params);

    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vout.resize(1);
    tx.vout[0].nValue = reward;

    CBlock block;
    block.vtx.push_back(CTransaction(tx));

    std::string err;
    BOOST_CHECK(IsBlockValueValid(block, 2501, reward, err));

    tx.vout[0].nValue = reward - 1;
    block.vtx[0] = CTransaction(tx);
    BOOST_CHECK(!IsBlockValueValid(block, 2501, reward, err));
}

BOOST_AUTO_TEST_SUITE_END()

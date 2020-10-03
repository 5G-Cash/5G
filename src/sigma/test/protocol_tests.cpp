#include "../params.h"
#include "../sigmaplus_prover.h"
#include "../sigmaplus_verifier.h"

#include <boost/test/unit_test.hpp>

#include "../../test/fixtures.h"

BOOST_FIXTURE_TEST_SUITE(sigma_protocol_tests, ZerocoinTestingSetup200)

BOOST_AUTO_TEST_CASE(one_out_of_n)
{
    auto params = sigma::Params::get_default();
    int N = 16384;
    int n = params->get_n();
    int m = params->get_m();
    int index = 0;

    secp_primitives::GroupElement g;
    g.randomize();
    std::vector<secp_primitives::GroupElement> h_gens;
    h_gens.resize(n * m);
    for(int i = 0; i < n * m; ++i ){
        h_gens[i].randomize();
    }
    secp_primitives::Scalar r;
    r.randomize();
    sigma::SigmaPlusProver<secp_primitives::Scalar,secp_primitives::GroupElement> prover(g,h_gens, n, m);

    std::vector<secp_primitives::GroupElement> commits;
    for(int i = 0; i < N; ++i){
        if(i == index){
            secp_primitives::GroupElement c;
            secp_primitives::Scalar zero(uint64_t(0));
            c = sigma::SigmaPrimitives<secp_primitives::Scalar,secp_primitives::GroupElement>::commit(g, zero, h_gens[0], r);
            commits.push_back(c);

        }
        else{
            commits.push_back(secp_primitives::GroupElement());
            commits[i].randomize();
        }
    }
    sigma::SigmaPlusProof<secp_primitives::Scalar,secp_primitives::GroupElement> proof(n, m);

    prover.proof(commits, index, r, true, proof);

    sigma::SigmaPlusVerifier<secp_primitives::Scalar,secp_primitives::GroupElement> verifier(g, h_gens, n, m);

    BOOST_CHECK(verifier.verify(commits, proof, true));
}

BOOST_AUTO_TEST_CASE(one_out_of_n_padding)
{
    auto params = sigma::Params::get_default();
    int N = 10000;
    int n = params->get_n();
    int m = params->get_m();
    int index = 9999;

    secp_primitives::GroupElement g;
    g.randomize();
    std::vector<secp_primitives::GroupElement> h_gens;
    h_gens.resize(n * m);
    for(int i = 0; i < n * m; ++i ){
        h_gens[i].randomize();
    }
    secp_primitives::Scalar r;
    r.randomize();
    sigma::SigmaPlusProver<secp_primitives::Scalar,secp_primitives::GroupElement> prover(g,h_gens, n, m);

    std::vector<secp_primitives::GroupElement> commits;
    for(int i = 0; i < N; ++i){
        if(i == index){
            secp_primitives::GroupElement c;
            secp_primitives::Scalar zero(uint64_t(0));
            c = sigma::SigmaPrimitives<secp_primitives::Scalar,secp_primitives::GroupElement>::commit(g, zero, h_gens[0], r);
            commits.push_back(c);

        }
        else{
            commits.push_back(secp_primitives::GroupElement());
            commits[i].randomize();
        }
    }
    sigma::SigmaPlusProof<secp_primitives::Scalar,secp_primitives::GroupElement> proof(n, m);

    prover.proof(commits, index, r, true, proof);

    sigma::SigmaPlusVerifier<secp_primitives::Scalar,secp_primitives::GroupElement> verifier(g, h_gens, n, m);

    BOOST_CHECK(verifier.verify(commits, proof, true));

    sigma::SigmaPlusProof<secp_primitives::Scalar,secp_primitives::GroupElement> proofNew(n, m);
    prover.proof(commits, 11111, r, true, proofNew);
    BOOST_CHECK(verifier.verify(commits, proofNew, true));
}

BOOST_AUTO_TEST_CASE(prove_and_verify_in_different_set)
{
    auto params = sigma::Params::get_default();
    int N = 16384;
    int n = params->get_n();
    int m = params->get_m();
    int index = 0;

    secp_primitives::GroupElement g;
    g.randomize();
    std::vector<secp_primitives::GroupElement> h_gens;
    h_gens.resize(n * m);
    for(int i = 0; i < n * m; ++i ){
        h_gens[i].randomize();
    }
    secp_primitives::Scalar r;
    r.randomize();
    sigma::SigmaPlusProver<secp_primitives::Scalar,secp_primitives::GroupElement> prover(g,h_gens, n, m);

    std::vector<secp_primitives::GroupElement> commits;
    for(int i = 0; i < N; ++i){
        if(i == index){
            secp_primitives::GroupElement c;
            secp_primitives::Scalar zero(uint64_t(0));
            c = sigma::SigmaPrimitives<secp_primitives::Scalar,secp_primitives::GroupElement>::commit(g, zero, h_gens[0], r);
            commits.push_back(c);

        }
        else{
            commits.push_back(secp_primitives::GroupElement());
            commits[i].randomize();
        }
    }

    sigma::SigmaPlusProof<secp_primitives::Scalar,secp_primitives::GroupElement> proof(n, m);

    prover.proof(commits, index, r, true, proof);

    sigma::SigmaPlusVerifier<secp_primitives::Scalar,secp_primitives::GroupElement> verifier(g, h_gens, n, m);

    // Add more commit
    secp_primitives::GroupElement c;
    secp_primitives::Scalar zero(uint64_t(0));
    c = sigma::SigmaPrimitives<secp_primitives::Scalar,secp_primitives::GroupElement>::commit(g, zero, h_gens[0], r);
    commits.push_back(c);

    BOOST_CHECK(!verifier.verify(commits, proof, true));
}

BOOST_AUTO_TEST_CASE(prove_coin_out_of_index)
{
    auto params = sigma::Params::get_default();
    int N = 16384;
    int n = params->get_n();
    int m = params->get_m();

    secp_primitives::GroupElement g;
    g.randomize();
    std::vector<secp_primitives::GroupElement> h_gens;
    h_gens.resize(n * m);
    for(int i = 0; i < n * m; ++i ){
        h_gens[i].randomize();
    }
    secp_primitives::Scalar r;
    r.randomize();
    sigma::SigmaPlusProver<secp_primitives::Scalar,secp_primitives::GroupElement> prover(g,h_gens, n, m);

    std::vector<secp_primitives::GroupElement> commits;
    for(int i = 0; i < N; ++i){
        commits.push_back(secp_primitives::GroupElement());
        commits[i].randomize();
    }

    sigma::SigmaPlusProof<secp_primitives::Scalar,secp_primitives::GroupElement> proof(n, m);

    prover.proof(commits, commits.size(), r, true, proof);

    sigma::SigmaPlusVerifier<secp_primitives::Scalar,secp_primitives::GroupElement> verifier(g, h_gens, n, m);
    BOOST_CHECK(!verifier.verify(commits, proof, true));
}

BOOST_AUTO_TEST_CASE(prove_coin_not_in_set)
{
    auto params = sigma::Params::get_default();
    int N = 16384;
    int n = params->get_n();
    int m = params->get_m();
    int index = 0;
    secp_primitives::GroupElement g;
    g.randomize();
    std::vector<secp_primitives::GroupElement> h_gens;
    h_gens.resize(n * m);
    for(int i = 0; i < n * m; ++i ){
        h_gens[i].randomize();
    }
    secp_primitives::Scalar r;
    r.randomize();
    sigma::SigmaPlusProver<secp_primitives::Scalar,secp_primitives::GroupElement> prover(g,h_gens, n, m);

    std::vector<secp_primitives::GroupElement> commits;
    for(int i = 0; i < N; ++i){
        commits.push_back(secp_primitives::GroupElement());
        commits[i].randomize();
    }

    sigma::SigmaPlusProof<secp_primitives::Scalar,secp_primitives::GroupElement> proof(n, m);

    prover.proof(commits, index, r, true, proof);

    sigma::SigmaPlusVerifier<secp_primitives::Scalar,secp_primitives::GroupElement> verifier(g, h_gens, n, m);
    BOOST_CHECK(!verifier.verify(commits, proof, true));
}

BOOST_AUTO_TEST_SUITE_END()

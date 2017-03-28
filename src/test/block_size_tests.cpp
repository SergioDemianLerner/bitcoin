// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "validation.h"
#include "miner.h"
#include "pubkey.h"
#include "uint256.h"
#include "policy/policy.h"
#include "consensus/merkle.h"
#include "util.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(block_size_tests, TestingSetup)

static int Megabyte =1000*1000;

static CFeeRate blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);


static BlockAssembler AssemblerForTest(const CChainParams& params) {
    BlockAssembler::Options options;

    options.nBlockMaxWeight = MAX_BLOCK_WEIGHT;
    options.nBlockMaxSize = MAX_BLOCK_SERIALIZED_SIZE;
    options.blockMinFeeRate = blockMinFeeRate;
    return BlockAssembler(params, options);
}


// Fill block with dummy transactions until it's serialized size is exactly nSize
static void
FillBlock(CBlock& block, unsigned int nSize)
{
    assert(block.vtx.size() > 0); // Start with at least a coinbase

    unsigned int nBlockSize = ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
    if (nBlockSize > nSize) {
        block.vtx.resize(1); // passed in block is too big, start with just coinbase
        nBlockSize = ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
    }

    // Create a block that is exactly 1,000,000 bytes, serialized:
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig = CScript() << OP_11;
    tx.vin[0].prevout.hash = block.vtx[0]->GetHash(); // passes CheckBlock, would fail if we checked inputs.
    tx.vin[0].prevout.n = 0;
    tx.vout.resize(1);
    tx.vout[0].nValue = 1LL;
    tx.vout[0].scriptPubKey = block.vtx[0]->vout[0].scriptPubKey;

    unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
    uint256 txhash = tx.GetHash();

    // ... add copies of tx to the block to get close to 1MB:
    while (nBlockSize+nTxSize < nSize) {
        block.vtx.push_back(MakeTransactionRef(tx));
        nBlockSize += nTxSize;
        tx.vin[0].prevout.hash = txhash; // ... just to make each transaction unique
        txhash = tx.GetHash();
    }
    // Make the last transaction exactly the right size by making the scriptSig bigger.
    block.vtx.pop_back();
    nBlockSize = ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
    unsigned int nFill = nSize - nBlockSize - nTxSize;
    for (unsigned int i = 0; i < nFill; i++)
        tx.vin[0].scriptSig << OP_11;
    block.vtx.push_back(MakeTransactionRef(std::move(tx)));
    nBlockSize = ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
    assert(nBlockSize == nSize);
}

static bool TestCheckBlock(CBlock& block, const CChainParams& params, uint64_t nTime, unsigned int nSize)
{
    SetMockTime(nTime);
    block.nTime = nTime;
    FillBlock(block, nSize);
    CValidationState validationState;
    bool fResult = CheckBlock(block, validationState, params.GetConsensus(),false, false) && validationState.IsValid();
    SetMockTime(0);
    return fResult;
}

/*
static bool TestCheckEmptyBlock(CBlock& block, const CChainParams& params, uint64_t nTime)
{
    SetMockTime(nTime);
    block.nTime = nTime;
    CValidationState validationState;
    bool fResult = CheckBlock(block, validationState, params.GetConsensus(),false, false) && validationState.IsValid();
    SetMockTime(0);
    return fResult;
}
*/


uint32_t VersionBitsMask(const Consensus::Params& params,const Consensus::DeploymentPos id)  
{ 
  return ((uint32_t)1) << params.vDeployments[id].bit; 
}

//
// Unit test CheckBlock() for conditions around the block size hard fork
//
BOOST_AUTO_TEST_CASE(TwoMegFork)
{
    const CChainParams& chainparams = Params(CBaseChainParams::MAIN);
    CScript scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    std::unique_ptr<CBlockTemplate> pblocktemplate;
    std::unique_ptr<CBlockTemplate> pblocktemplate2;
    
    LOCK(cs_main);
   
    BOOST_CHECK(pblocktemplate2 = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey));
    CBlock *pblock2 = &pblocktemplate2->block;
    pblock2->nVersion |= VersionBitsMask(chainparams.GetConsensus(),Consensus::DEPLOYMENT_SEGWIT_AND_2MB_BLOCKS);   
    
    //for(int i=0;i<2016*2;i++)
    //  BOOST_CHECK(TestCheckEmptyBlock(*pblock2, HardForkTime-2,chainparams)); // 2016*2 blocks are enough to lock-in and activate fork

    
    //for (unsigned int i = 0; i < sizeof(blockinfo)/sizeof(*blockinfo); ++i)
    for(int i=0;i<2016*2;i++)
    {
        CBlock *pblock = pblock2; // pointer for convenience
        pblock->nTime = chainActive.Tip()->GetMedianTimePast()+1;
        CMutableTransaction txCoinbase(*pblock->vtx[0]);
        txCoinbase.nVersion = 1;
        txCoinbase.vout.resize(1); // Ignore the (optional) segwit commitment added by CreateNewBlock (as the hardcoded nonces don't account for this)
        txCoinbase.vout[0].scriptPubKey = CScript();
	 txCoinbase.vin[0].scriptSig = CScript();
        txCoinbase.vin[0].scriptSig.push_back((unsigned char )0); // blockinfo[i].
        std::cout << "hashprev:" << pblock->hashPrevBlock.GetHex() << "\n";
	 std::cout << "height:" << chainActive.Height() << "\n";
	 txCoinbase.vin[0].scriptSig.push_back(chainActive.Height());
	 pblock->vtx[0] = MakeTransactionRef(txCoinbase);
	 pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
        pblock->nNonce = 0; 
	 std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
	 std::cout << "hash:" << pblock->GetHash().GetHex() << "\n";
	 BOOST_CHECK(ProcessNewBlock(chainparams, shared_pblock, true, NULL,false)); // do not check PoW
        std::cout << "hash-after:" << pblock->GetHash().GetHex() << "\n";
	 pblock->hashPrevBlock = pblock->GetHash();
    }
    
    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey));
    CBlock *pblock = &pblocktemplate->block;
    int64_t HardForkTime =chainparams.GetConsensus().HardForkTime;
    // Before fork time...
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime-1, Megabyte)); // 1MB : valid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime-1, Megabyte+1)); // >1MB : invalid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime-1, 2*Megabyte)); // 2MB : invalid

    
    // Exactly at fork time (without segwit2mb activation)...
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime, Megabyte)); // 1MB : valid
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime, 2*Megabyte)); // 2MB : valid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime, 2*Megabyte+1)); // >2MB : invalid
    
    // Exactly at fork time...
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime, Megabyte)); // 1MB : valid
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime, 2*Megabyte)); // 2MB : valid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime, 2*Megabyte+1)); // >2MB : invalid

    // Fork height + 10 min...
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime+600, 2*Megabyte+20)); // 2MB+20 : invalid

    // A year after fork time:
    unsigned int yearAfter = HardForkTime + (365 * 24 * 60 * 60);
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,yearAfter, Megabyte)); // 1MB : valid
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,yearAfter, 2*Megabyte)); // 2MB : valid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,yearAfter, 2*Megabyte+1)); // >2MB : invalid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,yearAfter, 3*Megabyte)); // 3MB : invalid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,yearAfter, 4*Megabyte)); // 4MB : invalid
}

//
// Unit test CheckBlock() for conditions around the block size hard fork
//
BOOST_AUTO_TEST_CASE(NoSegwit2MbActivation)
{
    const CChainParams& chainparams = Params(CBaseChainParams::MAIN);
    CScript scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    std::unique_ptr<CBlockTemplate> pblocktemplate;

    LOCK(cs_main);

    BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey));
    CBlock *pblock = &pblocktemplate->block;

    int64_t HardForkTime =chainparams.GetConsensus().HardForkTime;
    
    // Before fork time...
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime-1, Megabyte)); // 1MB : valid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime-1, Megabyte+1)); // >1MB : invalid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime-1, 2*Megabyte)); // 2MB : invalid

    // Exactly at fork time (without segwit2mb activation)...
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime, Megabyte)); // 1MB : valid
    BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime, 2*Megabyte)); // 2MB : valid
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime, 2*Megabyte+1)); // >2MB : invalid
    
    // Fork height + 10 min...
    BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime+600, 2*Megabyte)); // 2MB: invalid
}

BOOST_AUTO_TEST_SUITE_END()

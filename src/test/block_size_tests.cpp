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

	BOOST_FIXTURE_TEST_SUITE(block_size_tests, TestChainSetup)

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
	FillBlock(CBlock& block, unsigned int nSize,uint256 txhash)
	{
            assert(block.vtx.size() >= 1); // Start with at least a coinbase

	    unsigned int nBlockSize = ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
	    if (nBlockSize > nSize) {
		block.vtx.resize(1); // passed in block is too big, start with just coinbase
		nBlockSize = ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
	    }

	    // Create a block that is exactly 1,000,000 bytes, serialized:
	    CMutableTransaction tx;
	    tx.vin.resize(1);
	    tx.vin[0].scriptSig = CScript() << OP_11;
	    tx.vin[0].prevout.hash = txhash; // passes CheckBlock, would fail if we checked inputs.
	    tx.vin[0].prevout.n = 0;
	    tx.vout.resize(1);
	    tx.vout[0].nValue = 1LL;
	    tx.vout[0].scriptPubKey = block.vtx[0]->vout[0].scriptPubKey;

	    unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

	    // ... add copies of tx to the block to get close to 1MB:
	    while (nBlockSize+nTxSize < nSize) {
		block.vtx.push_back(MakeTransactionRef(tx));
		nBlockSize += nTxSize;
	    	txhash = tx.GetHash();
		if (nBlockSize+nTxSize >= nSize)    break;
	   	tx.vin[0].prevout.hash = txhash; // ... just to make each transaction unique, it consumes the previous tx output
	    	txhash = tx.GetHash();

	    }
	    // Make the last transaction exactly the right size by making the scriptSig bigger.
	    block.vtx.pop_back();
	    nBlockSize = ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
	    unsigned int nFill = nSize - nBlockSize - nTxSize;
	    for (unsigned int i = 0; i < nFill; i++)
		tx.vin[0].scriptSig << OP_11;
	    txhash = tx.GetHash();
	    block.vtx.push_back(MakeTransactionRef(std::move(tx)));
	    nBlockSize = ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
	    assert(nBlockSize == nSize);
	}

	bool  mineBlock(CBlock *pblock, const CChainParams& chainparams,uint256 & txhash);

	static bool TestCheckBlock(CBlock& block, const CChainParams& params, uint64_t nTime, unsigned int nSize,uint256 txhash)
	{
	    SetMockTime(nTime);
	    block.nTime = nTime;
	    FillBlock(block, nSize,txhash);
	    bool fResult = mineBlock(&block, params,txhash);
            SetMockTime(0);
	    return fResult;
	}

	uint32_t VersionBitsMask(const Consensus::Params& params,const Consensus::DeploymentPos id)  
	{ 
	  return ((uint32_t)1) << params.vDeployments[id].bit; 
	}
        
        bool hfActive(int64_t HardForkTime) {
              return chainActive.Tip()->GetMedianTimePast() >= HardForkTime;
        }
	bool  mineBlock(CBlock *pblock, const CChainParams& chainparams,uint256 & txhash)
        {
            CMutableTransaction txCoinbase(*pblock->vtx[0]);
            txCoinbase.nVersion = 1;
            txCoinbase.vout.resize(1); // Ignore the (optional) segwit commitment added by CreateNewBlock (as the hardcoded nonces don't account for this)
            txCoinbase.vout[0].scriptPubKey = CScript();
            txCoinbase.vin[0].scriptSig = CScript();
            txCoinbase.vin[0].scriptSig.push_back((unsigned char )0); // blockinfo[i].
            txCoinbase.vin[0].scriptSig.push_back(chainActive.Height());
            txhash = txCoinbase.GetHash();
            pblock->vtx[0] = MakeTransactionRef(txCoinbase);
            pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
            pblock->nNonce = 0;
            while (!CheckProofOfWork(pblock->GetHash(), pblock->nBits, chainparams.GetConsensus())) ++pblock->nNonce;
            std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
            int prevHeight =  chainActive.Height();
            bool ret = ProcessNewBlock(chainparams, shared_pblock, true, NULL);
            // blocks of erroneous size produce internally a CValidationState.DoS with msg:
            // bad-blk-length / size limits failed
	    //  However this is not propagated to ProcessNewBlock (the reason is unclear to me)
            // so we must use the blockchain height as an indicator of success
            if (ret) ret = chainActive.Height()>prevHeight;
            if (ret)
            {
               pblock->vtx.resize(1); // start with just coinbase
               pblock->hashPrevBlock = pblock->GetHash();
            }
            return ret;
        }
        //
	// Unit test CheckBlock() for conditions around the block size hard fork
	//
	BOOST_AUTO_TEST_CASE(TwoMegFork)
	{
	    const CChainParams& chainparams = Params(CBaseChainParams::REGTEST);
	    CScript scriptPubKey = CScript() ;
	    std::unique_ptr<CBlockTemplate> pblocktemplate;
	    std::unique_ptr<CBlockTemplate> pblocktemplate2;
            BOOST_CHECK(pblocktemplate = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey));
	    
	    LOCK(cs_main);
	   
	    CBlock *pblock2= NULL;
	    int nBlocks = 432;
            uint256 txhash ;
            std::vector<uint256> spend;
	    for(int i=0;i<nBlocks;i++)
	    {
		if ((i==0) || (((i+1) % chainparams.GetConsensus().nSubsidyHalvingInterval)==0))
	        { 
                 BOOST_CHECK(pblocktemplate2 = AssemblerForTest(chainparams).CreateNewBlock(scriptPubKey));
	         pblock2 = &pblocktemplate2->block;
                 // only advertise SEGWIT_2MB_BLOCKS. Nothing else.
	         pblock2->nVersion = 0x20000000 | VersionBitsMask(chainparams.GetConsensus(),Consensus::DEPLOYMENT_SEGWIT_AND_2MB_BLOCKS);   
                }
                bool isWitnessEnabled  = IsWitnessEnabled(chainActive.Tip(),chainparams.GetConsensus());
                if (i<=430) 
                   BOOST_CHECK(!isWitnessEnabled);
                else
                   BOOST_CHECK(isWitnessEnabled);
                
                BOOST_CHECK(!Is2MbBlocksEnabled(chainActive.Tip(),chainparams.GetConsensus()));

		// Create the blocks after SEGWIT_2MB start time
                if (i==0)
                   pblock2->nTime = chainparams.GetConsensus().vDeployments[Consensus::DEPLOYMENT_SEGWIT_AND_2MB_BLOCKS].nStartTime;
                   else
                   pblock2->nTime = chainActive.Tip()->GetMedianTimePast()+1;
	        SetMockTime(pblock2->nTime);//this allows the block to be accepted when nStartTime is in the future
		BOOST_CHECK(mineBlock(pblock2,chainparams,txhash));
                spend.push_back(txhash);
             }

             BOOST_CHECK(!Is2MbBlocksEnabled(chainActive.Tip(),chainparams.GetConsensus()));
             CBlock *pblock = &pblocktemplate->block;
             pblock->hashPrevBlock = pblock2->hashPrevBlock;
             int64_t HardForkTime =chainparams.GetConsensus().HardForkTime;
             //fPrintToConsole = true;
	     BOOST_CHECK(pblock2->nTime<HardForkTime);
             int cn = 101;  // after maturity

             // Before fork time...
             BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime-10, Megabyte,spend[cn++])); // 1MB : valid
             BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime-9, Megabyte+1,spend[cn++])); // >1MB : invalid

             // Exactly at fork time  and afterwards (without segwit2mb activation)...
             BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime, Megabyte,spend[cn++])); // 1MB : valid
             BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime, 2*Megabyte,spend[cn++])); // 2MB : invalid

             // The fork is activated when a previous block has MedianTime >= HardForkTime, so we need to
             // mine about 11 blocks.
             pblock2->hashPrevBlock = pblock->hashPrevBlock;
             for(int h=0;h<11;h++) {
               pblock2->nTime =HardForkTime+h;
               SetMockTime(pblock2->nTime);
               BOOST_CHECK(mineBlock(pblock2,chainparams,txhash));
             }
             pblock->hashPrevBlock = pblock2->hashPrevBlock;
             
             BOOST_CHECK(Is2MbBlocksEnabled(chainActive.Tip(),chainparams.GetConsensus()));
             
             // after the hard-fork has been locked in
             BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime+20, Megabyte,spend[cn++])); // 1MB : valid
             BOOST_CHECK(TestCheckBlock(*pblock, chainparams,HardForkTime+21, 2*Megabyte,spend[cn++])); // 2MB : valid
             BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime+22, 2*Megabyte+1,spend[cn++])); // >2MB : invali`d
    
             // Fork height + 10 min...
              BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,HardForkTime+600, 2*Megabyte+20,spend[cn++])); // 2MB+20 : invalid

              // A year after fork time:
              unsigned int yearAfter = HardForkTime + (365 * 24 * 60 * 60);
              BOOST_CHECK(TestCheckBlock(*pblock, chainparams,yearAfter, Megabyte,spend[cn++])); // 1MB : valid
              BOOST_CHECK(TestCheckBlock(*pblock, chainparams,yearAfter, 2*Megabyte,spend[cn++])); // 2MB : valid
              BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,yearAfter, 2*Megabyte+1,spend[cn++])); // >2MB : invalid
              BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,yearAfter, 3*Megabyte,spend[cn++])); // 3MB : invalid
              BOOST_CHECK(!TestCheckBlock(*pblock, chainparams,yearAfter, 4*Megabyte,spend[cn++])); // 4MB : invalid
}

BOOST_AUTO_TEST_SUITE_END()

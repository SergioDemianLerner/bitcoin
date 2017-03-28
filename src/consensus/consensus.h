// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONSENSUS_H
#define BITCOIN_CONSENSUS_CONSENSUS_H

#include <stdint.h>

/** The maximum allowed size for a serialized block for a 1Mb block, in bytes (only for buffer size limits) */
static const unsigned int MAX_BLOCK1_SERIALIZED_SIZE = 4000000;

/** The maximum allowed size for a serialized block, in bytes (only for buffer size limits) */
static const unsigned int MAX_BLOCK2_SERIALIZED_SIZE = 2*MAX_BLOCK1_SERIALIZED_SIZE;

/** The maximum allowed size for a serialized block, in bytes (only for buffer size limits) */
static const unsigned int MAX_BLOCK_SERIALIZED_SIZE = MAX_BLOCK2_SERIALIZED_SIZE;


/** The maximum allowed weight for a block, see BIP 141 (network rule) */
static const unsigned int MAX_BLOCK1_WEIGHT = 4000000;

/** The maximum allowed weight for a block, see BIP 141 (network rule) after the 2Mb fork*/
static const unsigned int MAX_BLOCK2_WEIGHT = 2*MAX_BLOCK1_WEIGHT;

/** The maximum allowed weight for a block, see BIP 141 (network rule) after the 2Mb fork*/
static const unsigned int MAX_BLOCK_WEIGHT = MAX_BLOCK2_WEIGHT;


/** The maximum allowed size for a block excluding witness data, in bytes (network rule) */
static const unsigned int MAX_BLOCK1_BASE_SIZE = 1000000;

/** The maximum allowed size for a block excluding witness data, in bytes (network rule) after the 2Mb fork */
static const unsigned int MAX_BLOCK2_BASE_SIZE = 2*MAX_BLOCK1_BASE_SIZE;

/** The maximum allowed size for a block excluding witness data, in bytes (network rule) after the 2Mb fork */
static const unsigned int MAX_BLOCK_BASE_SIZE = MAX_BLOCK2_BASE_SIZE;

/** The maximum allowed size for a transaction. Does not change when the block size increases */
static const unsigned int MAX_TRANSACTION_BASE_SIZE = 1000000;

/** The maximum allowed number of signature check operations in a block (network rule) */
/** The cost is four times the actual number of signature operations */
static const int64_t MAX_BLOCK1_SIGOPS_COST = 80000;

/** The maximum allowed number of signature check operations in a 2Mb block (network rule) */
static const int64_t MAX_BLOCK2_SIGOPS_COST = 2*MAX_BLOCK1_SIGOPS_COST;

/** The maximum allowed number of signature check operations in a 2Mb block (network rule) */
static const int64_t MAX_BLOCK_SIGOPS_COST = MAX_BLOCK2_SIGOPS_COST;

/** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
static const int COINBASE_MATURITY = 100;

/** Flags for nSequence and nLockTime locks */
enum {
    /* Interpret sequence numbers as relative lock-time constraints. */
    LOCKTIME_VERIFY_SEQUENCE = (1 << 0),

    /* Use GetMedianTimePast() instead of nTime for end point timestamp. */
    LOCKTIME_MEDIAN_TIME_PAST = (1 << 1),
};

#endif // BITCOIN_CONSENSUS_CONSENSUS_H

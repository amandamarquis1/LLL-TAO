/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLC/types/uint1024.h>
#include <LLC/hash/SK.h>
#include <LLC/include/random.h>

#include <LLD/include/global.h>
#include <LLD/cache/binary_lru.h>
#include <LLD/keychain/hashmap.h>
#include <LLD/templates/sector.h>

#include <Util/include/debug.h>
#include <Util/include/base64.h>

#include <openssl/rand.h>

#include <LLC/hash/argon2.h>
#include <LLC/hash/SK.h>
#include <LLC/include/flkey.h>
#include <LLC/types/bignum.h>

#include <Util/include/hex.h>

#include <iostream>

#include <TAO/Register/types/address.h>
#include <TAO/Register/types/object.h>

#include <TAO/Register/include/create.h>

#include <TAO/Ledger/types/genesis.h>
#include <TAO/Ledger/types/sigchain.h>
#include <TAO/Ledger/types/transaction.h>

#include <TAO/Ledger/include/ambassador.h>

#include <Legacy/types/address.h>
#include <Legacy/types/transaction.h>

#include <LLP/templates/ddos.h>
#include <Util/include/runtime.h>

#include <list>
#include <functional>
#include <variant>

#include <Util/include/softfloat.h>
#include <Util/include/filesystem.h>

#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/stake.h>

#include <LLP/types/tritium.h>

#include <TAO/Ledger/include/chainstate.h>
#include <TAO/Ledger/types/locator.h>

#include <LLD/templates/bloom.h>

class TestDB : public LLD::SectorDatabase<LLD::BinaryHashMap, LLD::BinaryLRU>
{
public:
    TestDB()
    : SectorDatabase("testdb"
    , LLD::FLAGS::CREATE | LLD::FLAGS::FORCE
    , 256 * 256 * 64
    , 1024 * 1024 * 8)
    {
    }

    ~TestDB()
    {

    }

    bool WriteKey(const uint1024_t& key, const uint1024_t& value)
    {
        return Write(std::make_pair(std::string("key"), key), value);
    }


    bool ReadKey(const uint1024_t& key, uint1024_t &value)
    {
        return Read(std::make_pair(std::string("key"), key), value);
    }


    bool WriteLast(const uint1024_t& last)
    {
        return Write(std::string("last"), last);
    }

    bool ReadLast(uint1024_t &last)
    {
        return Read(std::string("last"), last);
    }

};

#include <TAO/Ledger/include/genesis_block.h>

const uint256_t hashSeed = 55;

#include <bitset>

const uint32_t MAX_HASHMAP_FILES    = 128;

const uint32_t PRIMARY_BLOOM_HASHES   = 7;
const uint32_t PRIMARY_BLOOM_BITS     = 1.44 * MAX_HASHMAP_FILES * PRIMARY_BLOOM_HASHES;

const uint32_t SECONDARY_BLOOM_HASHES = 7;
const uint32_t SECONDARY_BLOOM_BITS   = 13;




uint32_t secondary_bloom_size()
{
    return ((MAX_HASHMAP_FILES * SECONDARY_BLOOM_BITS) / 8) + 1; //we want 1 byte padding here for bits that overflow
}


bool check_hashmap_available(const uint32_t nHashmap, const std::vector<uint8_t>& vBuffer, const uint32_t nOffset = 0)
{
    uint32_t nBeginIndex  = (nHashmap * SECONDARY_BLOOM_BITS) / 8;
    uint32_t nBeginOffset = (nHashmap * SECONDARY_BLOOM_BITS) % 8;

    for(uint32_t n = 0; n < SECONDARY_BLOOM_BITS; ++n)
    {
        uint32_t nIndex  = nBeginIndex + ((nBeginOffset + n) / 8);
        uint32_t nBit    = (nBeginOffset + n) % 8;

        if(vBuffer[nIndex + nOffset] & (1 << nBit))
            return false;
    }

    return true;
}

bool check_secondary_bloom(const std::vector<uint8_t>& vKey, const std::vector<uint8_t>& vBuffer, const uint32_t nHashmap, const uint32_t nOffset = 0)
{
    uint32_t nBeginIndex  = (nHashmap * SECONDARY_BLOOM_BITS) / 8;
    uint32_t nBeginOffset = (nHashmap * SECONDARY_BLOOM_BITS) % 8;

    for(uint32_t k = 0; k < SECONDARY_BLOOM_HASHES; ++k)
    {
        uint64_t nBucket = XXH3_64bits_withSeed((uint8_t*)&vKey[0], vKey.size(), k) % SECONDARY_BLOOM_BITS;

        uint32_t nIndex  = nBeginIndex + (nBeginOffset + nBucket) / 8;
        uint32_t nBit    = (nBeginOffset + nBucket) % 8;

        if(!(vBuffer[nIndex + nOffset] & (1 << nBit)))
            return false;
    }

    return true;
}

void set_secondary_bloom(const std::vector<uint8_t>& vKey, std::vector<uint8_t> &vBuffer, const uint32_t nHashmap, const uint32_t nOffset = 0)
{
    uint32_t nBeginIndex  = (nHashmap * SECONDARY_BLOOM_BITS) / 8;
    uint32_t nBeginOffset = (nHashmap * SECONDARY_BLOOM_BITS) % 8;

    for(uint32_t k = 0; k < SECONDARY_BLOOM_HASHES; ++k)
    {
        uint64_t nBucket = XXH3_64bits_withSeed((uint8_t*)&vKey[0], vKey.size(), k) % SECONDARY_BLOOM_BITS;

        uint32_t nIndex  = nBeginIndex + (nBeginOffset + nBucket) / 8;
        uint32_t nBit    = (nBeginOffset + nBucket) % 8;

        vBuffer[nIndex + nOffset] |= (1 << nBit);
    }
}


uint32_t primary_bloom_size()
{
    return (PRIMARY_BLOOM_BITS / 8) + 1; //we want 1 byte padding here for bits that overflow
}


void set_primary_bloom(const std::vector<uint8_t>& vKey, std::vector<uint8_t> &vBuffer, const uint32_t nOffset = 0)
{
    for(uint32_t k = 0; k < PRIMARY_BLOOM_HASHES; ++k)
    {
        uint64_t nBucket = XXH3_64bits_withSeed((uint8_t*)&vKey[0], vKey.size(), k) % PRIMARY_BLOOM_BITS;

        uint32_t nIndex  = (nBucket / 8);
        uint32_t nBit    = (nBucket % 8);

        if(nIndex + nOffset >= vBuffer.size())
            debug::error("OUT OF RANGE ", nIndex + nOffset, " SIZE ", vBuffer.size());

        vBuffer[nIndex + nOffset] |= (1 << nBit);
    }
}


bool check_primary_bloom(const std::vector<uint8_t>& vKey, const std::vector<uint8_t>& vBuffer, const uint32_t nOffset = 0)
{
    for(uint32_t k = 0; k < PRIMARY_BLOOM_HASHES; ++k)
    {
        uint64_t nBucket = XXH3_64bits_withSeed((uint8_t*)&vKey[0], vKey.size(), k) % PRIMARY_BLOOM_BITS;

        uint32_t nIndex  = (nBucket / 8);
        uint32_t nBit    = (nBucket % 8);

        /* Check the bitset for bloom value. */
        if(!(vBuffer[nIndex + nOffset] & (1 << nBit)))
            return false;
    }

    return true;
}


/* This is for prototyping new code. This main is accessed by building with LIVE_TESTS=1. */
int main(int argc, char** argv)
{
    config::mapArgs["-datadir"] = "/database/LIVE";

    TestDB* bloom = new TestDB();

    std::vector<uint1024_t> vKeys;
    for(int i = 0; i < MAX_HASHMAP_FILES; ++i)
        vKeys.push_back(LLC::GetRand1024());

    uint32_t nDuplicates = 0;

    std::vector<uint8_t> vBuffer(primary_bloom_size() + secondary_bloom_size(), 0);

    debug::log(0, "Allocating a buffer of size ", vBuffer.size(), " for ", vKeys.size(), " files");

    DataStream ssKey(SER_LLD, LLD::DATABASE_VERSION);
    ssKey << vKeys[0];

    uint32_t nOffset = primary_bloom_size();

    std::vector<uint8_t> vKey = ssKey.Bytes();
    set_secondary_bloom(vKey, vBuffer, 0, nOffset);

    for(uint32_t n = 1; n < vKeys.size(); ++n)
    {
        DataStream ssData(SER_LLD, LLD::DATABASE_VERSION);
        ssData << vKeys[n];

        set_primary_bloom(ssData.Bytes(), vBuffer);
        if(!check_primary_bloom(ssData.Bytes(), vBuffer))
            return debug::error("PRIMARY BLOOM FAILED AT ", HexStr(ssData.begin(), ssData.end()));

        if(!check_hashmap_available(n, vBuffer, nOffset))
            return debug::error("FILE ", n, " IS NOT EMPTY");

        set_secondary_bloom(ssData.Bytes(), vBuffer, n, nOffset);
        if(check_secondary_bloom(ssData.Bytes(), vBuffer, 0, nOffset))
            ++nDuplicates;

        if(check_hashmap_available(n, vBuffer, nOffset))
            return debug::error("FILE ", n, " EMPTY");


    }

    debug::log(0, "Created ", vKeys.size(), " Bloom filters with ",
        nDuplicates, " duplicates [", std::fixed, (nDuplicates * 100.0) / vKeys.size(), " %]");


    std::vector<uint1024_t> vKeys2;
    for(int i = 0; i < MAX_HASHMAP_FILES; ++i)
        vKeys2.push_back(LLC::GetRand1024());

    uint32_t nFalsePositives = 0;
    for(uint32_t n = 0; n < vKeys2.size(); ++n)
    {
        DataStream ssData(SER_LLD, LLD::DATABASE_VERSION);
        ssData << vKeys2[n];

        if(check_primary_bloom(ssData.Bytes(), vBuffer))
            ++nFalsePositives;
    }

    debug::log(0, nFalsePositives, " False Positives [", nFalsePositives * 100.0 / vKeys2.size(), " %]");

    return 0;

    runtime::stopwatch swTimer;
    swTimer.start();
    for(const auto& nBucket : vKeys)
    {
        bloom->WriteKey(nBucket, nBucket);
    }
    swTimer.stop();

    debug::log(0, "100k records written in ", swTimer.ElapsedMicroseconds());

    uint1024_t hashKey = 0;

    swTimer.reset();
    swTimer.start();
    for(const auto& nBucket : vKeys)
    {
        bloom->ReadKey(nBucket, hashKey);
    }
    swTimer.stop();

    debug::log(0, "100k records read in ", swTimer.ElapsedMicroseconds());

    delete bloom;

    return 0;
}

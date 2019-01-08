/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2018

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLC/include/random.h>

#include <Util/include/debug.h>
#include <Util/include/runtime.h>

namespace LLC
{

    void RandAddSeed()
    {
        // Seed with CPU performance counter
        int64_t nCounter = GetPerformanceCounter();
        RAND_add(&nCounter, sizeof(nCounter), 1.5);
        memset(&nCounter, 0, sizeof(nCounter));
    }

    void RandAddSeedPerfmon()
    {
        RandAddSeed();

        // This can take up to 2 seconds, so only do it every 10 minutes
        static int64_t nLastPerfmon;
        if (runtime::timestamp() < nLastPerfmon + 10 * 60)
            return;
        nLastPerfmon = runtime::timestamp();

    #ifdef WIN32
        // Don't need this on Linux, OpenSSL automatically uses /dev/urandom
        // Seed with the entire set of perfmon data
        uint8_t pdata[250000];
        memset(pdata, 0, sizeof(pdata));
        unsigned long nSize = sizeof(pdata);
        long ret = RegQueryValueExA(HKEY_PERFORMANCE_DATA, "Global", nullptr, nullptr, pdata, &nSize);
        RegCloseKey(HKEY_PERFORMANCE_DATA);
        if (ret == ERROR_SUCCESS)
        {
            RAND_add(pdata, nSize, nSize/100.0);
            memset(pdata, 0, nSize);
            debug::log(0, DateTimeStrFormat(runtime::timestamp()), " RandAddSeed() ", nSize, " bytes");
        }
    #endif
    }

    uint64_t GetRand(uint64_t nMax)
    {
        if (nMax == 0)
            return 0;

        // The range of the random source must be a multiple of the modulus
        // to give every possible output value an equal possibility
        uint64_t nRange = (std::numeric_limits<uint64_t>::max() / nMax) * nMax;
        uint64_t nRand = 0;
        do
        {
            RAND_bytes((uint8_t*)&nRand, sizeof(nRand));
        }
        while (nRand >= nRange);

        return (nRand % nMax);
    }

    int GetRandInt(int nMax)
    {
        return (int)GetRand(nMax);
    }


    uint256_t GetRand256()
    {
        uint256_t hash;
        RAND_bytes((uint8_t*)&hash, sizeof(hash));
        return hash;
    }


    uint512_t GetRand512()
    {
        uint512_t hash;
        RAND_bytes((uint8_t*)&hash, sizeof(hash));
        return hash;
    }


    uint1024_t GetRand1024()
    {
        uint1024_t hash;
        RAND_bytes((uint8_t*)&hash, sizeof(hash));
        return hash;
    }

}

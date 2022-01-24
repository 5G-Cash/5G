// (C) 2018 Michael Toutonghi
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
This provides the PoW hash function for 5GCash, enabling CPU mining.
*/
#ifndef VERUS_HASH_H_
#define VERUS_HASH_H_

// verbose output when defined
//#define VERUSHASHDEBUG 1

#include <cstring>
#include <vector>

#include "uint256.h"

extern "C" 
{
#include "crypto/verushash/haraka.h"
}

class CVerusHash
{
    public:
        static void Hash(void *result, const void *data, size_t len);
        static void (*haraka512Function)(unsigned char *out, const unsigned char *in);

        static void init();

        CVerusHash() { }

        CVerusHash &Write(const unsigned char *data, size_t len);

        CVerusHash &Reset()
        {
            curBuf = buf1;
            result = buf2;
            curPos = 0;
            std::fill(buf1, buf1 + sizeof(buf1), 0);
            return *this;
        }

        int64_t *ExtraI64Ptr() { return (int64_t *)(curBuf + 32); }
        void ClearExtra()
        {
            if (curPos)
            {
                std::fill(curBuf + 32 + curPos, curBuf + 64, 0);
            }
        }
        void ExtraHash(unsigned char hash[32]) { (*haraka512Function)(hash, curBuf); }

        void Finalize(unsigned char hash[32])
        {
            if (curPos)
            {
                std::fill(curBuf + 32 + curPos, curBuf + 64, 0);
                (*haraka512Function)(hash, curBuf);
            }
            else
                std::memcpy(hash, curBuf, 32);
        }

    private:
        // only buf1, the first source, needs to be zero initialized
        unsigned char buf1[64] = {0}, buf2[64];
        unsigned char *curBuf = buf1, *result = buf2;
        size_t curPos = 0;
};

extern void verus_hash(void *result, const void *data, size_t len);

#endif

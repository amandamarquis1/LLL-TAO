/*__________________________________________________________________________________________

            (c) Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++

            (c) Copyright The Nexus Developers 2014 - 2019

            Distributed under the MIT software license, see the accompanying
            file COPYING or http://www.opensource.org/licenses/mit-license.php.

            "ad vocem populi" - To the Voice of the People

____________________________________________________________________________________________*/

#include <LLC/hash/SK.h>
#include <LLC/hash/macro.h>
#include <LLC/include/eckey.h>
#include <LLC/types/bignum.h>

#include <Util/templates/datastream.h>
#include <Util/include/hex.h>
#include <Util/include/args.h>
#include <Util/include/convert.h>
#include <Util/include/runtime.h>

#include <TAO/Ledger/types/block.h>
#include <TAO/Ledger/include/prime.h>
#include <TAO/Ledger/include/chainstate.h>
#include <TAO/Ledger/include/constants.h>
#include <TAO/Ledger/include/timelocks.h>

#include <ios>
#include <iomanip>

/* Global TAO namespace. */
namespace TAO
{

    /* Ledger Layer namespace. */
    namespace Ledger
    {

        /** The default constructor. Sets block state to Null. **/
        Block::Block()
        {
            SetNull();
        }

        /** A base constructor. **/
        Block::Block(uint32_t nVersionIn, uint1024_t hashPrevBlockIn, uint32_t nChannelIn, uint32_t nHeightIn)
        : nVersion(nVersionIn)
        , hashPrevBlock(hashPrevBlockIn)
        , hashMerkleRoot()
        , nChannel(nChannelIn)
        , nHeight(nHeightIn)
        , nBits(0)
        , nNonce(0)
        , nTime(static_cast<uint32_t>(runtime::unifiedtimestamp()))
        , vchBlockSig()
        {
        }


        /** Copy constructor. **/
        Block::Block(const Block& block)
        : nVersion(block.nVersion)
        , hashPrevBlock(block.hashPrevBlock)
        , hashMerkleRoot(block.hashMerkleRoot)
        , nChannel(block.nChannel)
        , nHeight(block.nHeight)
        , nBits(block.nBits)
        , nNonce(block.nNonce)
        , nTime(block.nTime)
        , vchBlockSig(block.vchBlockSig.begin(), block.vchBlockSig.end())
        {
        }


        /** Default Destructor **/
        Block::~Block()
        {
        }


        /*  Allows polymorphic copying of blocks
         *  Derived classes should override this and return an instance of the derived type. */
        Block* Block::Clone() const
        {
            return new Block(*this);
        }


        /* Set the block state to null. */
        void Block::SetNull()
        {
            nVersion = TAO::Ledger::CurrentVersion();
            hashPrevBlock = 0;
            hashMerkleRoot = 0;
            nChannel = 0;
            nHeight = 0;
            nBits = 0;
            nNonce = 0;
            nTime = 0;
            vchBlockSig.clear();
        }


        /*  Check a block for consistency. */
        bool Block::Check() const
        {
            return true; /* No implementation in base class. */
        }


        /*  Accept a block with chain state parameters. */
        bool Block::Accept() const
        {
            return true; /* No implementation in base class. */
        }


        /* Set the channel of the block. */
        void Block::SetChannel(uint32_t nNewChannel)
        {
            nChannel = nNewChannel;
        }


        /* Get the Channel block is produced from. */
        uint32_t Block::GetChannel() const
        {
            return nChannel;
        }


        /* Check the nullptr state of the block. */
        bool Block::IsNull() const
        {
            return (nBits == 0);
        }


        /* Return the Block's current UNIX timestamp. */
        uint64_t Block::GetBlockTime() const
        {
            return (uint64_t)nTime;
        }


        /* Get the prime number of the block. */
        uint1024_t Block::GetPrime() const
        {
            return ProofHash() + nNonce;
        }


        /* Get the Proof Hash of the block. Used to verify work claims. */
        uint1024_t Block::ProofHash() const
        {
            /** Hashing template for CPU miners uses nVersion to nBits **/
            if(nChannel == 1)
                return LLC::SK1024(BEGIN(nVersion), END(nBits));

            /** Hashing template for GPU uses nVersion to nNonce **/
            return LLC::SK1024(BEGIN(nVersion), END(nNonce));
        }


        /* Get the Signarture Hash of the block. Used to verify work claims. */
        uint1024_t Block::SignatureHash() const
        {
            /* Signature hash for version 7 blocks. */
            if(nVersion >= 7)
            {
                /* Create a data stream to get the hash. */
                DataStream ss(SER_GETHASH, LLP::PROTOCOL_VERSION);
                ss.reserve(256);

                /* Serialize the data to hash into a stream. */
                ss << nVersion << hashPrevBlock << hashMerkleRoot << nChannel << nHeight << nBits << nNonce << nTime << vOffsets;

                return LLC::SK1024(ss.begin(), ss.end());
            }

            return LLC::SK1024(BEGIN(nVersion), END(nTime));
        }


        /* Generate a Hash For the Block from the Header. */
        uint1024_t Block::GetHash() const
        {
            /* Pre-Version 5 rule of being block hash. */
            if(nVersion < 5)
                return ProofHash();

            return SignatureHash();
        }


        /* Update the nTime of the current block. */
        void Block::UpdateTime()
        {
            nTime = static_cast<uint32_t>(std::max(ChainState::stateBest.load().GetBlockTime() + 1, runtime::unifiedtimestamp()));
        }


        /* Check flags for nPoS block. */
        bool Block::IsProofOfStake() const
        {
            return (nChannel == 0);
        }


        /* Check flags for PoW block. */
        bool Block::IsProofOfWork() const
        {
            return (nChannel == 1 || nChannel == 2);
        }


        /* Check flags for PoW block. */
        bool Block::IsPrivate() const
        {
            return nChannel == 3;
        }


        /* Generate the Merkle Tree from uint512_t hashes. */
        uint512_t Block::BuildMerkleTree(std::vector<uint512_t> vMerkleTree) const
        {
            uint32_t i = 0;
            uint32_t j = 0;
            uint32_t nSize = static_cast<uint32_t>(vMerkleTree.size());

            for(; nSize > 1; nSize = (nSize + 1) >> 1)
            {
                for(i = 0; i < nSize; i += 2)
                {
                    /* get the references to the left and right leaves in the merkle tree */
                    uint512_t &left_tx  = vMerkleTree[j+i];
                    uint512_t &right_tx = vMerkleTree[j + std::min(i+1, nSize-1)];

                    vMerkleTree.push_back(LLC::SK512(BEGIN(left_tx),  END(left_tx),
                                                     BEGIN(right_tx), END(right_tx)));
                }
                j += nSize;
            }
            return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
        }


        /* For debugging Purposes seeing block state data dump */
        std::string Block::ToString() const
        {
            return debug::safe_printstr(
                "Block(hash=", GetHash().SubString(),
                ", ver=", nVersion,
                ", hashPrevBlock=", hashPrevBlock.SubString(),
                ", hashMerkleRoot=", hashMerkleRoot.SubString(10),
                ", nTime=", nTime,
                std::hex, std::setfill('0'), std::setw(8), ", nBits=", nBits,
                std::dec, std::setfill(' '), std::setw(0), ", nChannel = ", nChannel,
                ", nHeight= ", nHeight,
                ", nNonce=",  nNonce,
                ", vchBlockSig=", HexStr(vchBlockSig.begin(), vchBlockSig.end()).substr(0,20), ")");
        }

        /* Dump the Block data to Console / Debug.log. */
        void Block::print() const
        {
            debug::log(0, ToString());
        }


        /* Verify the Proof of Work satisfies network requirements. */
        bool Block::VerifyWork() const
        {
            /* Check the Prime Number Proof of Work for the Prime Channel. */
            if(nChannel == 1)
            {
                /* Check prime minimum origins. */
                if(nVersion >= 5 && ProofHash() < bnPrimeMinOrigins.getuint1024())
                    return debug::error(FUNCTION, "prime origins below 1016-bits");

                /* Check proof of work limits. */
                uint32_t nPrimeBits = GetPrimeBits(GetPrime());
                if(nPrimeBits < bnProofOfWorkLimit[1])
                    return debug::error(FUNCTION, "prime-cluster below minimum work" "(", nPrimeBits, ")");

                /* Check the prime difficulty target. */
                if(nPrimeBits < nBits)
                    return debug::error(FUNCTION, "prime-cluster below target ", "(proof: ", nPrimeBits, " target: ", nBits, ")");

                return true;
            }
            if(nChannel == 2)
            {
                /* Get the hash target. */
                LLC::CBigNum bnTarget;
                bnTarget.SetCompact(nBits);

                /* Check that the hash is within range. */
                if(bnTarget <= 0 || bnTarget > bnProofOfWorkLimit[2])
                    return debug::error(FUNCTION, "proof-of-work hash not in range");


                /* Check that the that enough work was done on this block. */
                if(ProofHash() > bnTarget.getuint1024())
                    return debug::error(FUNCTION, "proof-of-work hash below target");

                return true;
            }

            /* Check for a private block work claims. */
            if(IsPrivate() && !config::GetBoolArg("-private"))
                return debug::error(FUNCTION, "Invalid channel: ", nChannel);

            return true;
        }


        /* Sign the block with the key that found the block. */
        bool Block::GenerateSignature(const LLC::FLKey& key)
        {
            return key.Sign(GetHash().GetBytes(), vchBlockSig);
        }


        /* Check that the block signature is a valid signature. */
        bool Block::VerifySignature(const LLC::FLKey& key) const
        {
            if(vchBlockSig.empty())
                return false;

            return key.Verify(GetHash().GetBytes(), vchBlockSig);
        }

        /* Sign the block with the key that found the block. */
        bool Block::GenerateSignature(const LLC::ECKey& key)
        {
            return key.Sign((nVersion == 4) ? SignatureHash() : GetHash(), vchBlockSig, 1024);
        }


        /* Check that the block signature is a valid signature. */
        bool Block::VerifySignature(const LLC::ECKey& key) const
        {
            if(vchBlockSig.empty())
                return false;

            return key.Verify((nVersion == 4) ? SignatureHash() : GetHash(), vchBlockSig, 1024);
        }


        /*  Convert the Header of a Block into a Byte Stream for
         *  Reading and Writing Across Sockets. */
        std::vector<uint8_t> Block::Serialize() const
        {
            std::vector<uint8_t> VERSION  = convert::uint2bytes(nVersion);
            std::vector<uint8_t> PREVIOUS = hashPrevBlock.GetBytes();
            std::vector<uint8_t> MERKLE   = hashMerkleRoot.GetBytes();
            std::vector<uint8_t> CHANNEL  = convert::uint2bytes(nChannel);
            std::vector<uint8_t> HEIGHT   = convert::uint2bytes(nHeight);
            std::vector<uint8_t> BITS     = convert::uint2bytes(nBits);
            std::vector<uint8_t> NONCE    = convert::uint2bytes64(nNonce);

            std::vector<uint8_t> vData;
            vData.insert(vData.end(), VERSION.begin(),   VERSION.end());
            vData.insert(vData.end(), PREVIOUS.begin(), PREVIOUS.end());
            vData.insert(vData.end(), MERKLE.begin(),     MERKLE.end());
            vData.insert(vData.end(), CHANNEL.begin(),   CHANNEL.end());
            vData.insert(vData.end(), HEIGHT.begin(),     HEIGHT.end());
            vData.insert(vData.end(), BITS.begin(),         BITS.end());
            vData.insert(vData.end(), NONCE.begin(),       NONCE.end());

            return vData;
        }


        /*  Convert Byte Stream into Block Header. */
        void Block::Deserialize(const std::vector<uint8_t>& vData)
        {
            nVersion = convert::bytes2uint(std::vector<uint8_t>(vData.begin(), vData.begin() + 4));

            hashPrevBlock.SetBytes (std::vector<uint8_t>(vData.begin() + 4, vData.begin() + 132));
            hashMerkleRoot.SetBytes(std::vector<uint8_t>(vData.begin() + 132, vData.end() - 20));

            nChannel = convert::bytes2uint(std::vector<uint8_t>( vData.end() - 20, vData.end() - 16));
            nHeight  = convert::bytes2uint(std::vector<uint8_t>( vData.end() - 16, vData.end() - 12));
            nBits    = convert::bytes2uint(std::vector<uint8_t>( vData.end() - 12, vData.end() - 8));
            nNonce   = convert::bytes2uint64(std::vector<uint8_t>(vData.end() -  8, vData.end()));
        }


        /* Generates the StakeHash for this block from a uint256_t hashGenesis*/
        uint1024_t Block::StakeHash(bool fIsGenesis, const uint256_t& hashGenesis) const
        {
            /* Create a data stream to get the hash. */
            DataStream ss(SER_GETHASH, LLP::PROTOCOL_VERSION);
            ss.reserve(256);

            /* Serialize the data to hash into a stream. */
            ss << nVersion << hashPrevBlock << nChannel << nHeight << nBits << hashGenesis << nNonce;

            return LLC::SK1024(ss.begin(), ss.end());
        }


        /* Generates the StakeHash for this block from a uint256_t hashGenesis*/
        uint1024_t Block::StakeHash(bool fIsGenesis, const uint576_t &trustKey) const
        {
            /* Create a data stream to get the hash. */
            DataStream ss(SER_GETHASH, LLP::PROTOCOL_VERSION);
            ss.reserve(256);

            /* Trust Key is part of stake hash if not genesis. */
            if(nHeight > 2392970 && fIsGenesis)
            {
                /* Genesis must hash a prvout of 0. */
                uint512_t hashPrevout = 0;

                /* Serialize the data to hash into a stream. */
                ss << nVersion << hashPrevBlock << nChannel << nHeight << nBits << hashPrevout << nNonce;

                return LLC::SK1024(ss.begin(), ss.end());
            }

            /* Serialize the data to hash into a stream. */
            ss << nVersion << hashPrevBlock << nChannel << nHeight << nBits << trustKey << nNonce;

            return LLC::SK1024(ss.begin(), ss.end());
        }
    }
}

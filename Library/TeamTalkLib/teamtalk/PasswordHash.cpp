/*
 * Copyright (c) 2005-2018, BearWare.dk
 * 
 * Contact Information:
 *
 * Bjoern D. Rasmussen
 * Kirketoften 5
 * DK-8260 Viby J
 * Denmark
 * Email: contact@bearware.dk
 * Phone: +45 20 20 54 59
 * Web: http://www.bearware.dk
 *
 * This source code is part of the TeamTalk SDK owned by
 * BearWare.dk. Use of this file, or its compiled unit, requires a
 * TeamTalk SDK License Key issued by BearWare.dk.
 *
 * The TeamTalk SDK License Agreement along with its Terms and
 * Conditions are outlined in the file License.txt included with the
 * TeamTalk SDK distribution.
 *
 */

#include "PasswordHash.h"

#include <array>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

#if defined(ENABLE_ENCRYPTION)
#include <openssl/rand.h>
#endif

/* Argon2id (RFC 9106) with a bundled BLAKE2b implementation. Keeping
 * the hash function in-tree avoids a hard dependency on libargon2,
 * which is not available on all platforms TeamTalk supports.
 */
namespace teamtalk
{
namespace password
{
namespace detail
{
    constexpr uint32_t ARGON2_BLOCK_SIZE = 1024;
    constexpr uint32_t ARGON2_QWORDS_IN_BLOCK = 128;
    constexpr uint32_t ARGON2_SYNC_POINTS = 4;
    constexpr uint32_t ARGON2_VERSION = 0x13;
    constexpr uint32_t ARGON2_TYPE_ID = 2;
    constexpr uint32_t ARGON2_ADDRESSES_IN_BLOCK = 128;

    // Sanity bounds when parsing hashes from a configuration file.
    constexpr uint32_t ARGON2_MAX_MEMORY_KB = 1024 * 1024;
    constexpr uint32_t ARGON2_MAX_ITERATIONS = 64;
    constexpr uint32_t ARGON2_MAX_LANES = 16;
    constexpr uint32_t ARGON2_MIN_SALT_LENGTH = 8;
    constexpr uint32_t ARGON2_MAX_SALT_LENGTH = 64;
    constexpr uint32_t ARGON2_MIN_HASH_LENGTH = 16;
    constexpr uint32_t ARGON2_MAX_HASH_LENGTH = 64;

    struct Block
    {
        uint64_t v[ARGON2_QWORDS_IN_BLOCK] = {};
    };

    uint64_t load64(const uint8_t* p)
    {
        uint64_t v = 0;
        for(int i = 7; i >= 0; --i)
            v = (v << 8) | p[i];
        return v;
    }

    void store64(uint8_t* p, uint64_t v)
    {
        for(int i = 0; i < 8; ++i)
        {
            p[i] = static_cast<uint8_t>(v & 0xFF);
            v >>= 8;
        }
    }

    void store32(std::vector<uint8_t>& buf, uint32_t v)
    {
        for(int i = 0; i < 4; ++i)
            buf.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }

    uint64_t rotr64(uint64_t w, unsigned c)
    {
        return (w >> c) | (w << (64 - c));
    }

    ///////////////////////////////////////////////////////////////////////
    // BLAKE2b (RFC 7693)
    ///////////////////////////////////////////////////////////////////////

    constexpr std::array<uint64_t, 8> BLAKE2B_IV = {
        0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
        0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
        0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
        0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
    };

    constexpr uint8_t BLAKE2B_SIGMA[10][16] = {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
        { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
        { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
        { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
        { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
        { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
        { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
        { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
        { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
        { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 }
    };

    void blake2bMix(uint64_t& a, uint64_t& b, uint64_t& c, uint64_t& d,
                    uint64_t x, uint64_t y)
    {
        a = a + b + x;
        d = rotr64(d ^ a, 32);
        c = c + d;
        b = rotr64(b ^ c, 24);
        a = a + b + y;
        d = rotr64(d ^ a, 16);
        c = c + d;
        b = rotr64(b ^ c, 63);
    }

    void blake2bCompress(uint64_t h[8], const uint8_t block[128],
                         uint64_t t0, uint64_t t1, bool last)
    {
        uint64_t m[16];
        for(int i = 0; i < 16; ++i)
            m[i] = load64(block + i * 8);

        uint64_t v[16];
        for(int i = 0; i < 8; ++i)
        {
            v[i] = h[i];
            v[i + 8] = BLAKE2B_IV[i];
        }

        v[12] ^= t0;
        v[13] ^= t1;
        if(last)
            v[14] = ~v[14];

        for(int round = 0; round < 12; ++round)
        {
            const uint8_t* s = BLAKE2B_SIGMA[round % 10];
            blake2bMix(v[0], v[4], v[8], v[12], m[s[0]], m[s[1]]);
            blake2bMix(v[1], v[5], v[9], v[13], m[s[2]], m[s[3]]);
            blake2bMix(v[2], v[6], v[10], v[14], m[s[4]], m[s[5]]);
            blake2bMix(v[3], v[7], v[11], v[15], m[s[6]], m[s[7]]);
            blake2bMix(v[0], v[5], v[10], v[15], m[s[8]], m[s[9]]);
            blake2bMix(v[1], v[6], v[11], v[12], m[s[10]], m[s[11]]);
            blake2bMix(v[2], v[7], v[8], v[13], m[s[12]], m[s[13]]);
            blake2bMix(v[3], v[4], v[9], v[14], m[s[14]], m[s[15]]);
        }

        for(int i = 0; i < 8; ++i)
            h[i] ^= v[i] ^ v[i + 8];
    }

    // Unkeyed BLAKE2b with variable digest length (1 to 64 bytes).
    void blake2b(uint8_t* out, size_t outlen,
                 const uint8_t* in, size_t inlen)
    {
        uint64_t h[8];
        for(int i = 0; i < 8; ++i)
            h[i] = BLAKE2B_IV[i];

        // fanout = 1, depth = 1, key length = 0, digest length = outlen
        h[0] ^= 0x01010000ULL ^ static_cast<uint64_t>(outlen);

        uint64_t t0 = 0, t1 = 0;
        size_t offset = 0;
        while(inlen - offset > 128)
        {
            uint64_t const prev = t0;
            t0 += 128;
            if(t0 < prev)
                t1++;
            blake2bCompress(h, in + offset, t0, t1, false);
            offset += 128;
        }

        uint8_t last[128] = {};
        size_t const remaining = inlen - offset;
        if(remaining > 0)
            std::memcpy(last, in + offset, remaining);
        uint64_t const prev = t0;
        t0 += remaining;
        if(t0 < prev)
            t1++;
        blake2bCompress(h, last, t0, t1, true);

        uint8_t full[64];
        for(int i = 0; i < 8; ++i)
            store64(full + i * 8, h[i]);

        std::memcpy(out, full, outlen);
    }

    // Argon2's variable length hash H' (variable length BLAKE2b).
    void blake2bLong(uint8_t* out, size_t outlen,
                     const uint8_t* in, size_t inlen)
    {
        std::vector<uint8_t> prefixed;
        prefixed.reserve(inlen + 4);
        store32(prefixed, static_cast<uint32_t>(outlen));
        prefixed.insert(prefixed.end(), in, in + inlen);

        if(outlen <= 64)
        {
            blake2b(out, outlen, prefixed.data(), prefixed.size());
            return;
        }

        uint8_t buffer[64];
        blake2b(buffer, sizeof(buffer), prefixed.data(), prefixed.size());

        std::memcpy(out, buffer, 32);
        out += 32;
        size_t to_produce = outlen - 32;
        while(to_produce > 64)
        {
            blake2b(buffer, sizeof(buffer), buffer, sizeof(buffer));
            std::memcpy(out, buffer, 32);
            out += 32;
            to_produce -= 32;
        }
        blake2b(out, to_produce, buffer, sizeof(buffer));
    }

    ///////////////////////////////////////////////////////////////////////
    // Argon2 block permutation
    ///////////////////////////////////////////////////////////////////////

    uint64_t blamka(uint64_t x, uint64_t y)
    {
        return x + y + 2 * (x & 0xFFFFFFFFULL) * (y & 0xFFFFFFFFULL);
    }

    void argon2Mix(uint64_t& a, uint64_t& b, uint64_t& c, uint64_t& d)
    {
        a = blamka(a, b);
        d = rotr64(d ^ a, 32);
        c = blamka(c, d);
        b = rotr64(b ^ c, 24);
        a = blamka(a, b);
        d = rotr64(d ^ a, 16);
        c = blamka(c, d);
        b = rotr64(b ^ c, 63);
    }

    void argon2Round(uint64_t* v)
    {
        argon2Mix(v[0], v[4], v[8], v[12]);
        argon2Mix(v[1], v[5], v[9], v[13]);
        argon2Mix(v[2], v[6], v[10], v[14]);
        argon2Mix(v[3], v[7], v[11], v[15]);
        argon2Mix(v[0], v[5], v[10], v[15]);
        argon2Mix(v[1], v[6], v[11], v[12]);
        argon2Mix(v[2], v[7], v[8], v[13]);
        argon2Mix(v[3], v[4], v[9], v[14]);
    }

    void fillBlock(const Block& prev, const Block& ref, Block& next, bool with_xor)
    {
        Block blockR, blockTmp;
        for(uint32_t i = 0; i < ARGON2_QWORDS_IN_BLOCK; ++i)
            blockR.v[i] = ref.v[i] ^ prev.v[i];

        for(uint32_t i = 0; i < ARGON2_QWORDS_IN_BLOCK; ++i)
            blockTmp.v[i] = with_xor ? (blockR.v[i] ^ next.v[i]) : blockR.v[i];

        // Apply the BLAKE2b round function to each of the 8 rows...
        for(uint32_t i = 0; i < 8; ++i)
            argon2Round(&blockR.v[16 * i]);

        // ... and to each of the 8 columns.
        for(uint32_t i = 0; i < 8; ++i)
        {
            uint64_t column[16];
            for(uint32_t j = 0; j < 8; ++j)
            {
                column[2 * j] = blockR.v[2 * i + 16 * j];
                column[2 * j + 1] = blockR.v[2 * i + 16 * j + 1];
            }
            argon2Round(column);
            for(uint32_t j = 0; j < 8; ++j)
            {
                blockR.v[2 * i + 16 * j] = column[2 * j];
                blockR.v[2 * i + 16 * j + 1] = column[2 * j + 1];
            }
        }

        for(uint32_t i = 0; i < ARGON2_QWORDS_IN_BLOCK; ++i)
            next.v[i] = blockTmp.v[i] ^ blockR.v[i];
    }

    void nextAddresses(Block& address, Block& input, const Block& zero)
    {
        input.v[6]++;
        fillBlock(zero, input, address, false);
        fillBlock(zero, address, address, false);
    }

    uint32_t indexAlpha(uint32_t pass, uint32_t slice, uint32_t index,
                        uint32_t pseudo_rand, bool same_lane,
                        uint32_t lane_length, uint32_t segment_length)
    {
        uint32_t reference_area_size;
        if(pass == 0)
        {
            if(slice == 0)
                reference_area_size = index - 1;
            else if(same_lane)
                reference_area_size = slice * segment_length + index - 1;
            else
                reference_area_size = slice * segment_length + ((index == 0) ? 0xFFFFFFFFU : 0);
        }
        else
        {
            if(same_lane)
                reference_area_size = lane_length - segment_length + index - 1;
            else
                reference_area_size = lane_length - segment_length + ((index == 0) ? 0xFFFFFFFFU : 0);
        }

        // Map pseudo_rand to 0..reference_area_size-1 and derive the
        // relative position (as a quadratically distributed value).
        uint64_t relative_position = pseudo_rand;
        relative_position = (relative_position * relative_position) >> 32;
        relative_position = reference_area_size - 1 -
            ((static_cast<uint64_t>(reference_area_size) * relative_position) >> 32);

        uint32_t start_position = 0;
        if(pass != 0)
        {
            start_position = (slice == ARGON2_SYNC_POINTS - 1) ?
                0 : (slice + 1) * segment_length;
        }

        return static_cast<uint32_t>((start_position + relative_position) % lane_length);
    }

    void fillSegment(std::vector<Block>& memory, uint32_t pass, uint32_t lane,
                     uint32_t slice, uint32_t lanes, uint32_t passes,
                     uint32_t memory_blocks, uint32_t lane_length,
                     uint32_t segment_length)
    {
        bool const data_independent = (pass == 0 && slice < 2);

        Block zero, input, address;
        if(data_independent)
        {
            input.v[0] = pass;
            input.v[1] = lane;
            input.v[2] = slice;
            input.v[3] = memory_blocks;
            input.v[4] = passes;
            input.v[5] = ARGON2_TYPE_ID;
        }

        uint32_t starting_index = 0;
        if(pass == 0 && slice == 0)
        {
            // The first two blocks of each lane are already generated.
            starting_index = 2;
            if(data_independent)
                nextAddresses(address, input, zero);
        }

        uint32_t curr_offset = lane * lane_length + slice * segment_length + starting_index;
        uint32_t prev_offset;
        if(curr_offset % lane_length == 0)
            prev_offset = curr_offset + lane_length - 1;
        else
            prev_offset = curr_offset - 1;

        for(uint32_t i = starting_index; i < segment_length;
            ++i, ++curr_offset, ++prev_offset)
        {
            if(curr_offset % lane_length == 1)
                prev_offset = curr_offset - 1;

            uint64_t pseudo_rand;
            if(data_independent)
            {
                if(i % ARGON2_ADDRESSES_IN_BLOCK == 0)
                    nextAddresses(address, input, zero);
                pseudo_rand = address.v[i % ARGON2_ADDRESSES_IN_BLOCK];
            }
            else
            {
                pseudo_rand = memory[prev_offset].v[0];
            }

            uint32_t const ref_lane =
                (pass == 0 && slice == 0) ? lane :
                static_cast<uint32_t>((pseudo_rand >> 32) % lanes);

            uint32_t const ref_index =
                indexAlpha(pass, slice, i, static_cast<uint32_t>(pseudo_rand & 0xFFFFFFFFULL),
                           ref_lane == lane, lane_length, segment_length);

            fillBlock(memory[prev_offset], memory[ref_lane * lane_length + ref_index],
                      memory[curr_offset], pass != 0);
        }
    }

    // Raw Argon2id hash. Exposed in detail namespace for unit testing.
    std::vector<uint8_t> argon2idHash(const uint8_t* pwd, size_t pwdlen,
                                      const uint8_t* salt, size_t saltlen,
                                      uint32_t t_cost, uint32_t m_cost_kb,
                                      uint32_t lanes, uint32_t taglen)
    {
        if(lanes == 0)
            lanes = 1;

        std::vector<uint8_t> h0input;
        h0input.reserve(pwdlen + saltlen + 40);
        store32(h0input, lanes);
        store32(h0input, taglen);
        store32(h0input, m_cost_kb);
        store32(h0input, t_cost);
        store32(h0input, ARGON2_VERSION);
        store32(h0input, ARGON2_TYPE_ID);
        store32(h0input, static_cast<uint32_t>(pwdlen));
        h0input.insert(h0input.end(), pwd, pwd + pwdlen);
        store32(h0input, static_cast<uint32_t>(saltlen));
        h0input.insert(h0input.end(), salt, salt + saltlen);
        store32(h0input, 0); // secret
        store32(h0input, 0); // associated data

        uint8_t h0[64];
        blake2b(h0, sizeof(h0), h0input.data(), h0input.size());

        uint32_t memory_blocks = m_cost_kb;
        uint32_t const min_blocks = 2 * ARGON2_SYNC_POINTS * lanes;
        if(memory_blocks < min_blocks)
            memory_blocks = min_blocks;

        uint32_t segment_length = memory_blocks / (lanes * ARGON2_SYNC_POINTS);
        memory_blocks = segment_length * lanes * ARGON2_SYNC_POINTS;
        uint32_t const lane_length = segment_length * ARGON2_SYNC_POINTS;

        std::vector<Block> memory(memory_blocks);

        // Generate the first two blocks of each lane.
        for(uint32_t lane = 0; lane < lanes; ++lane)
        {
            uint8_t seed[72];
            std::memcpy(seed, h0, sizeof(h0));
            for(uint32_t index = 0; index < 2; ++index)
            {
                seed[64] = static_cast<uint8_t>(index & 0xFF);
                seed[65] = static_cast<uint8_t>((index >> 8) & 0xFF);
                seed[66] = static_cast<uint8_t>((index >> 16) & 0xFF);
                seed[67] = static_cast<uint8_t>((index >> 24) & 0xFF);
                seed[68] = static_cast<uint8_t>(lane & 0xFF);
                seed[69] = static_cast<uint8_t>((lane >> 8) & 0xFF);
                seed[70] = static_cast<uint8_t>((lane >> 16) & 0xFF);
                seed[71] = static_cast<uint8_t>((lane >> 24) & 0xFF);

                Block& block = memory[lane * lane_length + index];
                blake2bLong(reinterpret_cast<uint8_t*>(block.v),
                            ARGON2_BLOCK_SIZE, seed, sizeof(seed));
            }
        }

        for(uint32_t pass = 0; pass < t_cost; ++pass)
        {
            for(uint32_t slice = 0; slice < ARGON2_SYNC_POINTS; ++slice)
            {
                for(uint32_t lane = 0; lane < lanes; ++lane)
                {
                    fillSegment(memory, pass, lane, slice, lanes, t_cost,
                                memory_blocks, lane_length, segment_length);
                }
            }
        }

        Block final_block = memory[lane_length - 1];
        for(uint32_t lane = 1; lane < lanes; ++lane)
        {
            Block const& last = memory[lane * lane_length + lane_length - 1];
            for(uint32_t i = 0; i < ARGON2_QWORDS_IN_BLOCK; ++i)
                final_block.v[i] ^= last.v[i];
        }

        std::vector<uint8_t> tag(taglen);
        blake2bLong(tag.data(), taglen,
                    reinterpret_cast<const uint8_t*>(final_block.v),
                    ARGON2_BLOCK_SIZE);
        return tag;
    }

    ///////////////////////////////////////////////////////////////////////
    // PHC string format
    ///////////////////////////////////////////////////////////////////////

    constexpr char BASE64_CHARS[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string base64Encode(const uint8_t* data, size_t len)
    {
        std::string out;
        out.reserve((len * 4 + 2) / 3);
        size_t i = 0;
        while(i + 2 < len)
        {
            uint32_t const n = (static_cast<uint32_t>(data[i]) << 16) |
                (static_cast<uint32_t>(data[i + 1]) << 8) |
                static_cast<uint32_t>(data[i + 2]);
            out.push_back(BASE64_CHARS[(n >> 18) & 0x3F]);
            out.push_back(BASE64_CHARS[(n >> 12) & 0x3F]);
            out.push_back(BASE64_CHARS[(n >> 6) & 0x3F]);
            out.push_back(BASE64_CHARS[n & 0x3F]);
            i += 3;
        }
        size_t const remaining = len - i;
        if(remaining == 1)
        {
            uint32_t const n = static_cast<uint32_t>(data[i]) << 16;
            out.push_back(BASE64_CHARS[(n >> 18) & 0x3F]);
            out.push_back(BASE64_CHARS[(n >> 12) & 0x3F]);
        }
        else if(remaining == 2)
        {
            uint32_t const n = (static_cast<uint32_t>(data[i]) << 16) |
                (static_cast<uint32_t>(data[i + 1]) << 8);
            out.push_back(BASE64_CHARS[(n >> 18) & 0x3F]);
            out.push_back(BASE64_CHARS[(n >> 12) & 0x3F]);
            out.push_back(BASE64_CHARS[(n >> 6) & 0x3F]);
        }
        return out;
    }

    int base64Value(char c)
    {
        if(c >= 'A' && c <= 'Z') return c - 'A';
        if(c >= 'a' && c <= 'z') return c - 'a' + 26;
        if(c >= '0' && c <= '9') return c - '0' + 52;
        if(c == '+') return 62;
        if(c == '/') return 63;
        return -1;
    }

    bool base64Decode(const std::string& in, std::vector<uint8_t>& out)
    {
        out.clear();
        uint32_t acc = 0;
        int bits = 0;
        for(char c : in)
        {
            if(c == '=')
                break;
            int const v = base64Value(c);
            if(v < 0)
                return false;
            acc = (acc << 6) | static_cast<uint32_t>(v);
            bits += 6;
            if(bits >= 8)
            {
                bits -= 8;
                out.push_back(static_cast<uint8_t>((acc >> bits) & 0xFF));
            }
        }
        return true;
    }

    bool parseUint32(const std::string& s, size_t& pos, uint32_t& value)
    {
        if(pos >= s.size() || s[pos] < '0' || s[pos] > '9')
            return false;
        uint64_t v = 0;
        while(pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
        {
            v = v * 10 + static_cast<uint64_t>(s[pos] - '0');
            if(v > std::numeric_limits<uint32_t>::max())
                return false;
            ++pos;
        }
        value = static_cast<uint32_t>(v);
        return true;
    }

    struct Argon2idParams
    {
        uint32_t memory_kb = 0;
        uint32_t iterations = 0;
        uint32_t lanes = 0;
        std::vector<uint8_t> salt;
        std::vector<uint8_t> hash;
    };

    bool parseArgon2idHash(const std::string& stored, Argon2idParams& params)
    {
        const std::string prefix = "$argon2id$v=19$";
        if(stored.compare(0, prefix.size(), prefix) != 0)
            return false;

        size_t pos = prefix.size();

        // Parameters: m=<memory>,t=<iterations>,p=<lanes>
        while(true)
        {
            size_t const key_start = pos;
            while(pos < stored.size() && stored[pos] != '=' && stored[pos] != ',')
                ++pos;
            if(pos >= stored.size() || stored[pos] != '=')
                return false;
            std::string const key = stored.substr(key_start, pos - key_start);
            ++pos;

            uint32_t value = 0;
            if(!parseUint32(stored, pos, value))
                return false;

            if(key == "m")
                params.memory_kb = value;
            else if(key == "t")
                params.iterations = value;
            else if(key == "p")
                params.lanes = value;
            else
                return false;

            if(pos < stored.size() && stored[pos] == ',')
            {
                ++pos;
                continue;
            }
            break;
        }

        if(pos >= stored.size() || stored[pos] != '$')
            return false;
        ++pos;

        size_t const salt_start = pos;
        while(pos < stored.size() && stored[pos] != '$')
            ++pos;
        if(pos >= stored.size())
            return false;
        std::string const salt_b64 = stored.substr(salt_start, pos - salt_start);
        ++pos;
        std::string const hash_b64 = stored.substr(pos);

        if(!base64Decode(salt_b64, params.salt) ||
           !base64Decode(hash_b64, params.hash))
            return false;

        if(params.memory_kb == 0 || params.memory_kb > ARGON2_MAX_MEMORY_KB)
            return false;
        if(params.iterations == 0 || params.iterations > ARGON2_MAX_ITERATIONS)
            return false;
        if(params.lanes == 0 || params.lanes > ARGON2_MAX_LANES)
            return false;
        if(params.salt.size() < ARGON2_MIN_SALT_LENGTH ||
           params.salt.size() > ARGON2_MAX_SALT_LENGTH)
            return false;
        if(params.hash.size() < ARGON2_MIN_HASH_LENGTH ||
           params.hash.size() > ARGON2_MAX_HASH_LENGTH)
            return false;

        return true;
    }

    bool constantTimeEquals(const uint8_t* a, const uint8_t* b, size_t len)
    {
        uint8_t diff = 0;
        for(size_t i = 0; i < len; ++i)
            diff |= static_cast<uint8_t>(a[i] ^ b[i]);
        return diff == 0;
    }

    bool constantTimeEquals(const std::string& a, const std::string& b)
    {
        if(a.size() != b.size())
            return false;
        return constantTimeEquals(reinterpret_cast<const uint8_t*>(a.data()),
                                  reinterpret_cast<const uint8_t*>(b.data()),
                                  a.size());
    }

    bool randomBytes(uint8_t* buf, size_t len)
    {
#if defined(ENABLE_ENCRYPTION)
        return RAND_bytes(buf, static_cast<int>(len)) == 1;
#else
        std::random_device rd;
        for(size_t i = 0; i < len; ++i)
            buf[i] = static_cast<uint8_t>(rd());
        return true;
#endif
    }

} // namespace detail

bool IsHashedPassword(const std::string& stored)
{
    return stored.compare(0, 10, "$argon2id$") == 0;
}

std::string HashPassword(const std::string& passwordUtf8)
{
    // Nothing secret to protect (e.g. BearWare.dk WebLogin accounts).
    if(passwordUtf8.empty())
        return std::string();

    std::array<uint8_t, ARGON2ID_SALT_LENGTH> salt = {};
    if(!detail::randomBytes(salt.data(), salt.size()))
        return std::string();

    std::vector<uint8_t> const tag =
        detail::argon2idHash(reinterpret_cast<const uint8_t*>(passwordUtf8.data()),
                             passwordUtf8.size(), salt.data(), salt.size(),
                             ARGON2ID_ITERATIONS, ARGON2ID_MEMORY_KB,
                             ARGON2ID_LANES, ARGON2ID_HASH_LENGTH);

    std::string hash;
    hash.reserve(128);
    hash += "$argon2id$v=19$m=";
    hash += std::to_string(ARGON2ID_MEMORY_KB);
    hash += ",t=";
    hash += std::to_string(ARGON2ID_ITERATIONS);
    hash += ",p=";
    hash += std::to_string(ARGON2ID_LANES);
    hash += '$';
    hash += detail::base64Encode(salt.data(), salt.size());
    hash += '$';
    hash += detail::base64Encode(tag.data(), tag.size());
    return hash;
}

bool VerifyPassword(const std::string& passwordUtf8, const std::string& stored)
{
    if(stored.empty())
        return passwordUtf8.empty();

    detail::Argon2idParams params;
    if(detail::parseArgon2idHash(stored, params))
    {
        std::vector<uint8_t> const tag =
            detail::argon2idHash(reinterpret_cast<const uint8_t*>(passwordUtf8.data()),
                                 passwordUtf8.size(), params.salt.data(),
                                 params.salt.size(), params.iterations,
                                 params.memory_kb, params.lanes,
                                 static_cast<uint32_t>(params.hash.size()));
        return detail::constantTimeEquals(tag.data(), params.hash.data(), tag.size());
    }

    // Legacy plain text password (configuration written by older
    // server versions). Accepted so it can be upgraded to Argon2id.
    return detail::constantTimeEquals(passwordUtf8, stored);
}

} // namespace password
} // namespace teamtalk

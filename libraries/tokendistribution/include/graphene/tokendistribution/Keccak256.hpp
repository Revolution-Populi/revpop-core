/*
 * Copyright (c) Project Nayuki
 * Copyright (c) 2020-2023 Revolution Populi Limited, and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <vector>
#include <string>

namespace graphene { namespace tokendistribution {

typedef std::vector<std::uint8_t> Bytes;

Bytes asciiBytes(const char *str);
Bytes hexBytes(const char *str);
std::string bytesHex(const Bytes &v);

/*
* Computes the Keccak-256 hash of a sequence of bytes. The hash value is 32 bytes long.
* Provides just one static method.
*/
class Keccak256 final
{
public:
    static constexpr int HASH_LEN = 32;

    static void getHash(const std::uint8_t msg[], std::size_t len, std::uint8_t hashResult[HASH_LEN]);

private:
    static constexpr int BLOCK_SIZE = 200 - HASH_LEN * 2;

    static constexpr int NUM_ROUNDS = 24;

    static void absorb(std::uint64_t state[5][5]);

    // Requires 0 <= i <= 63
    static std::uint64_t rotl64(std::uint64_t x, int i);

    Keccak256() = delete; // Not instantiable

    static const unsigned char ROTATION[5][5];
};

} }

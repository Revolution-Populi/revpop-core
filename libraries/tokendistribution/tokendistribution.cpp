/*
 * Copyright (c) Project Nayuki
 * Copyright (c) 2018-2022 Revolution Populi Limited, and contributors.
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

#include "secp256k1.h"
#include <fc/exception/exception.hpp>

#include <graphene/tokendistribution/Keccak256.hpp>

namespace graphene { namespace tokendistribution {

void preparePubKey(std::string& pubKey) {
    if (pubKey.length() == 130)
      pubKey = pubKey.erase(0, 2); // drop "04"

   if (pubKey.length() != 128)
      FC_THROW_EXCEPTION(fc::assert_exception, "Ethereum key length is incorrect. Is it a real key?");
}

std::string getAddress(std::string pubKey)
{
   preparePubKey(pubKey);

   Bytes message = hexBytes(pubKey.c_str());
   std::uint8_t hashBuff[Keccak256::HASH_LEN];
   Keccak256::getHash(message.data(), message.size(), hashBuff);
   Bytes hash(hashBuff, hashBuff + Keccak256::HASH_LEN);

   std::string address = bytesHex(hash);
   address = "0x" + address.substr(address.length() - 40); // Take the last 40 characters
   return address;
}

int verifyMessage (std::string pubKey, std::string sig) {
   std::string hello = "Hello world!";
   Bytes message = asciiBytes(hello.c_str());
   std::uint8_t actualHashBuff[Keccak256::HASH_LEN];
   Keccak256::getHash(message.data(), message.size(), actualHashBuff);
   Bytes actualHash(actualHashBuff, actualHashBuff + Keccak256::HASH_LEN);
   std::string msg32 = bytesHex(actualHash);

   preparePubKey(pubKey);

   // verifying signature
   secp256k1_context_t *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
   int result = secp256k1_ecdsa_verify(ctx,
            reinterpret_cast<const unsigned char *>(msg32.c_str()),
            reinterpret_cast<const unsigned char *>(sig.c_str()), sig.length(),
            reinterpret_cast<const unsigned char *>(pubKey.c_str()), pubKey.length());
   secp256k1_context_destroy(ctx);

   return result;
}

} }
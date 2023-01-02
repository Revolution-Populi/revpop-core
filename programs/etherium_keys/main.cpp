/*
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

#include <string>
#include <vector>
#include <iostream>

#include <graphene/tokendistribution/Keccak256.hpp>

#include <fc/crypto/aes.hpp>

using namespace graphene::tokendistribution;

void hash_test()
{
   Bytes message = asciiBytes("Welcome to RevPop");
   std::uint8_t actualHashBuff[Keccak256::HASH_LEN];
   Keccak256::getHash(message.data(), message.size(), actualHashBuff);
   Bytes actualHash(actualHashBuff, actualHashBuff + Keccak256::HASH_LEN);

   Bytes expectHash = hexBytes("5118ae58841e57c7a54d28a6623a6478a2d2f6f7b15ba9b886b54597cea71f0d");
   assert((std::memcmp(actualHash.data(), expectHash.data(), Keccak256::HASH_LEN) == 0));
   std::cout << bytesHex(actualHash) << std::endl;
   std::cout << bytesHex(expectHash) << std::endl;
}

void aes_test()
{
   try
   {
      std::string plain_text = "Hello world";
      // auto key = "259ccd9dfa0c1f43b20f13e20750127ef47f72724ea33e74e5c2ec624ee6c64c";
      //  TODO: key to pubString
      auto pubString = "04eacdfff99e185f4c17fea0a3e9398d5a2f6585486b28f3131e99a114072adedaf5bf334bcbeb09886fa68d6fd623c7d8fbe2eab9610503d4c717ba5769d8269d";

      std::vector<char> cipher_text(plain_text.size() + 16);
      auto cipher_len = fc::aes_encrypt((unsigned char *)/*plaintext*/ plain_text.data(),
                                        (int)/*plaintext_len*/ plain_text.size(),
                                        (unsigned char *)/*key*/ &pubString,
                                        ((unsigned char *)/*iv*/ &pubString) + 32,
                                        (unsigned char *)/*ciphertext*/ cipher_text.data());
      FC_ASSERT(cipher_len <= cipher_text.size());
      cipher_text.resize(cipher_len);

      std::copy(cipher_text.begin(), cipher_text.end(),
                std::ostream_iterator<int>(std::cout));
      std::cout << std::endl;
   }
   catch (const fc::exception &e)
   {
      edump((e.to_detail_string()));
   }
}

int main(int argc, char **argv)
{
   hash_test();
   return 0;
}

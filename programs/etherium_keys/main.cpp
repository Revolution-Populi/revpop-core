/*
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

#include <string>
#include <vector>
#include <iostream>

#include <graphene/tokendistribution/tokendistribution.hpp>
#include <fc/exception/exception.hpp>

using namespace graphene::tokendistribution;

int main(int argc, char **argv)
{
   std::string pubKey = "04e68acfc0253a10620dff706b0a1b1f1f5833ea3beb3bde2250d5f271f3563606672ebc45e0b7ea2e816ecb70ca03137b1c9476eec63d4632e990020b7b6fba39";
   try
   {
      std::string address = getAddress(pubKey);
      std::cout << address << std::endl;
   }
   catch (const fc::exception &e)
   {
      edump((e.to_detail_string()));
   }

   try
   {
      std::string sig = "0b149134fcb989ae11ceb2dabe86f2cdd16e04088c72bebe378f78011296b2876304d468b200b9d7925823607934cb5a6d99660f1870f802e1d4c97b25b22c7e1c";
      int result = verifyMessage(pubKey, sig);
      assert(result != 1);
   }
   catch (const fc::exception &e)
   {
      edump((e.to_detail_string()));
   }

   return 0;
}

/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
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
#include <graphene/chain/ico_balance_evaluator.hpp>
#include <graphene/protocol/pts_address.hpp>
#include <graphene/tokendistribution/tokendistribution.hpp>

namespace graphene { namespace chain {

void_result ico_balance_claim_evaluator::do_evaluate(const ico_balance_claim_operation& op)
{
   database& d = db();

   ico_balance = &op.balance_to_claim(d);
   if( !ico_balance ) return {};

   FC_ASSERT(tokendistribution::verifyMessage(op.eth_pub_key, op.eth_sign) == 1);
   FC_ASSERT(ico_balance->eth_address == tokendistribution::getAddress(op.eth_pub_key));

   //FC_ASSERT(op.total_claimed.asset_id == ico_balance->asset_type());

   return {};
}

/**
 * @note the fee is always 0 for this particular operation because once the
 * balance is claimed it frees up memory and it cannot be used to spam the network
 */
void_result ico_balance_claim_evaluator::do_apply(const ico_balance_claim_operation& op)
{
   database& d = db();

   if( ico_balance )
   {
      d.adjust_balance(op.deposit_to_account, ico_balance->balance);
   }
   d.remove(*ico_balance);

   return {};
}
} } // namespace graphene::chain

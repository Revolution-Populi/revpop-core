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
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/market_object.hpp>

#include <graphene/chain/market_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/protocol/market.hpp>

namespace graphene { namespace chain {
void_result limit_order_create_evaluator::do_evaluate(const limit_order_create_operation& op)
{ try {
   const database& d = db();

   FC_ASSERT( op.expiration >= d.head_block_time() );

   _seller        = this->fee_paying_account;
   _sell_asset    = &op.amount_to_sell.asset_id(d);
   _receive_asset = &op.min_to_receive.asset_id(d);

   if( _sell_asset->options.whitelist_markets.size() )
   {
      GRAPHENE_ASSERT( _sell_asset->options.whitelist_markets.find(_receive_asset->id)
                          != _sell_asset->options.whitelist_markets.end(),
                       limit_order_create_market_not_whitelisted,
                       "This market has not been whitelisted by the selling asset", );
   }
   if( _sell_asset->options.blacklist_markets.size() )
   {
      GRAPHENE_ASSERT( _sell_asset->options.blacklist_markets.find(_receive_asset->id)
                          == _sell_asset->options.blacklist_markets.end(),
                       limit_order_create_market_blacklisted,
                       "This market has been blacklisted by the selling asset", );
   }

   GRAPHENE_ASSERT( is_authorized_asset( d, *_seller, *_sell_asset ),
                    limit_order_create_selling_asset_unauthorized,
                    "The account is not allowed to transact the selling asset", );

   GRAPHENE_ASSERT( is_authorized_asset( d, *_seller, *_receive_asset ),
                    limit_order_create_receiving_asset_unauthorized,
                    "The account is not allowed to transact the receiving asset", );

   GRAPHENE_ASSERT( d.get_balance( *_seller, *_sell_asset ) >= op.amount_to_sell,
                    limit_order_create_insufficient_balance,
                    "insufficient balance",
                    ("balance",d.get_balance(*_seller,*_sell_asset))("amount_to_sell",op.amount_to_sell) );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void limit_order_create_evaluator::convert_fee()
{
   if( !trx_state->skip_fee )
   {
      if( fee_asset->get_id() != asset_id_type() )
      {
         db().modify(*fee_asset_dyn_data, [this](asset_dynamic_data_object& d) {
            d.fee_pool -= core_fee_paid;
         });
      }
   }
}

void limit_order_create_evaluator::pay_fee()
{
   _deferred_fee = core_fee_paid;
   if( fee_asset->get_id() != asset_id_type() )
      _deferred_paid_fee = fee_from_account;
}

object_id_type limit_order_create_evaluator::do_apply(const limit_order_create_operation& op)
{ try {
   if( op.amount_to_sell.asset_id == asset_id_type() )
   {
      db().modify( _seller->statistics(db()), [&op](account_statistics_object& bal) {
         bal.total_core_in_orders += op.amount_to_sell.amount;
      });
   }

   db().adjust_balance(op.seller, -op.amount_to_sell);

   const auto& new_order_object = db().create<limit_order_object>([&](limit_order_object& obj){
       obj.seller   = _seller->id;
       obj.for_sale = op.amount_to_sell.amount;
       obj.sell_price = op.get_price();
       obj.expiration = op.expiration;
       obj.deferred_fee = _deferred_fee;
       obj.deferred_paid_fee = _deferred_paid_fee;
   });
   limit_order_id_type order_id = new_order_object.id; // save this because we may remove the object by filling it
   bool filled;
   filled = db().apply_order( new_order_object );

   GRAPHENE_ASSERT( !op.fill_or_kill || filled,
                    limit_order_create_kill_unfilled,
                    "Killing limit order ${op} due to unable to fill",
                    ("op",op) );

   return order_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result limit_order_cancel_evaluator::do_evaluate(const limit_order_cancel_operation& o)
{ try {
   database& d = db();

   _order = d.find( o.order );

   GRAPHENE_ASSERT( _order != nullptr,
                    limit_order_cancel_nonexist_order,
                    "Limit order ${oid} does not exist",
                    ("oid", o.order) );

   GRAPHENE_ASSERT( _order->seller == o.fee_paying_account,
                    limit_order_cancel_owner_mismatch,
                    "Limit order ${oid} is owned by someone else",
                    ("oid", o.order) );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

asset limit_order_cancel_evaluator::do_apply(const limit_order_cancel_operation& o)
{ try {
   database& d = db();

   auto refunded = _order->amount_for_sale();

   d.cancel_limit_order(*_order, false /* don't create a virtual op*/);

   return refunded;
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result call_order_update_evaluator::do_evaluate(const call_order_update_operation& o)
{ try {
   database& d = db();

   _paying_account = &o.funding_account(d);
   _debt_asset     = &o.delta_debt.asset_id(d);
   FC_ASSERT( _debt_asset->is_market_issued(), "Unable to cover ${sym} as it is not a collateralized asset.",
              ("sym", _debt_asset->symbol) );

   FC_ASSERT( o.delta_debt.amount <= 0 || _debt_asset->can_create_new_supply(), "Can not create new supply" );

   _dynamic_data_obj = &_debt_asset->dynamic_asset_data_id(d);

   /***
    * There are instances of assets exceeding max_supply, therefore this code must remain.
    */
   FC_ASSERT( _dynamic_data_obj->current_supply + o.delta_debt.amount <= _debt_asset->options.max_supply,
         "Borrowing this quantity would exceed MAX_SUPPLY" );
   
   FC_ASSERT( _dynamic_data_obj->current_supply + o.delta_debt.amount >= 0,
         "This transaction would bring current supply below zero.");

   _bitasset_data  = &_debt_asset->bitasset_data(d);

   /// if there is a settlement for this asset, then no further margin positions may be taken and
   /// all existing margin positions should have been closed va database::globally_settle_asset
   FC_ASSERT( !_bitasset_data->has_settlement(), "Cannot update debt position when the asset has been globally settled" );

   FC_ASSERT( o.delta_collateral.asset_id == _bitasset_data->options.short_backing_asset,
              "Collateral asset type should be same as backing asset of debt asset" );

   if( _bitasset_data->is_prediction_market )
      FC_ASSERT( o.delta_collateral.amount == o.delta_debt.amount,
                 "Debt amount and collateral amount should be same when updating debt position in a prediction market" );
   else if( _bitasset_data->current_feed.settlement_price.is_null() )
      FC_THROW_EXCEPTION(insufficient_feeds, "Cannot borrow asset with no price feed.");

   // Note: there was code here checking whether the account has enough balance to increase delta collateral,
   //       which is now removed since the check is implicitly done later by `adjust_balance()` in `do_apply()`.

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }


object_id_type call_order_update_evaluator::do_apply(const call_order_update_operation& o)
{ try {
   database& d = db();

   if( o.delta_debt.amount != 0 )
   {
      d.adjust_balance( o.funding_account, o.delta_debt );

      // Deduct the debt paid from the total supply of the debt asset.
      d.modify(*_dynamic_data_obj, [&](asset_dynamic_data_object& dynamic_asset) {
         dynamic_asset.current_supply += o.delta_debt.amount;
      });
   }

   if( o.delta_collateral.amount != 0 )
   {
      d.adjust_balance( o.funding_account, -o.delta_collateral  );

      // Adjust the total core in orders accodingly
      if( o.delta_collateral.asset_id == asset_id_type() )
      {
         d.modify(_paying_account->statistics(d), [&](account_statistics_object& stats) {
               stats.total_core_in_orders += o.delta_collateral.amount;
         });
      }
   }

   auto& call_idx = d.get_index_type<call_order_index>().indices().get<by_account>();
   auto itr = call_idx.find( boost::make_tuple(o.funding_account, o.delta_debt.asset_id) );
   const call_order_object* call_obj = nullptr;
   call_order_id_type call_order_id;

   optional<price> old_collateralization;
   optional<share_type> old_debt;

   if( itr == call_idx.end() ) // creating new debt position
   {
      FC_ASSERT( o.delta_collateral.amount > 0, "Delta collateral amount of new debt position should be positive" );
      FC_ASSERT( o.delta_debt.amount > 0, "Delta debt amount of new debt position should be positive" );

      call_obj = &d.create<call_order_object>( [&o,this]( call_order_object& call ){
         call.borrower = o.funding_account;
         call.collateral = o.delta_collateral.amount;
         call.debt = o.delta_debt.amount;
         call.call_price = price( asset( 1, o.delta_collateral.asset_id ), asset( 1, o.delta_debt.asset_id ) );
         call.target_collateral_ratio = o.extensions.value.target_collateral_ratio;
      });
      call_order_id = call_obj->id;
   }
   else // updating existing debt position
   {
      call_obj = &*itr;
      auto new_collateral = call_obj->collateral + o.delta_collateral.amount;
      auto new_debt = call_obj->debt + o.delta_debt.amount;
      call_order_id = call_obj->id;

      if( new_debt == 0 )
      {
         FC_ASSERT( new_collateral == 0, "Should claim all collateral when closing debt position" );
         d.remove( *call_obj );
         return call_order_id;
      }

      FC_ASSERT( new_collateral > 0 && new_debt > 0,
                 "Both collateral and debt should be positive after updated a debt position if not to close it" );

      old_collateralization = call_obj->collateralization();
      old_debt = call_obj->debt;

      d.modify( *call_obj, [&o,new_debt,new_collateral,this]( call_order_object& call ){
         call.collateral = new_collateral;
         call.debt       = new_debt;
         call.target_collateral_ratio = o.extensions.value.target_collateral_ratio;
      });
   }

   // then we must check for margin calls and other issues
   if( !_bitasset_data->is_prediction_market )
   {
      // check to see if the order needs to be margin called now, but don't allow black swans and require there to be
      // limit orders available that could be used to fill the order.
      // Note: due to https://github.com/bitshares/bitshares-core/issues/649, before core-343 hard fork,
      //       the first call order may be unable to be updated if the second one is undercollateralized.
      if( d.check_call_orders( *_debt_asset, false, false, _bitasset_data ) ) // don't allow black swan, not for new limit order
      {
         call_obj = d.find(call_order_id);
         // before hard fork core-583: if we filled at least one call order, we are OK if we totally filled.
         // after hard fork core-583: we want to allow increasing collateral
         //   Note: increasing collateral won't get the call order itself matched (instantly margin called)
         //   if there is at least a call order get matched but didn't cause a black swan event,
         //   current order must have got matched. in this case, it's OK if it's totally filled.
         GRAPHENE_ASSERT(
            !call_obj,
            call_order_update_unfilled_margin_call,
            "Updating call order would trigger a margin call that cannot be fully filled"
            );
      }
      else
      {
         call_obj = d.find(call_order_id);
         // we know no black swan event has occurred
         FC_ASSERT( call_obj, "no margin call was executed and yet the call object was deleted" );

         // We didn't fill any call orders.  This may be because we
         // aren't in margin call territory, or it may be because there
         // were no matching orders. In the latter case,
         // if collateral ratio is not increased or debt is increased, we throw.
         // be here, we know no margin call was executed,
         // so call_obj's collateral ratio should be set only by op
         // ------
         // Before BSIP77, CR of the new/updated position is required to be above MCR;
         // after BSIP77, CR of the new/updated position is required to be above max(ICR,MCR).
         // The `current_initial_collateralization` variable has been initialized according to the logic,
         // so we directly use it here.
         bool check = ( call_obj->collateralization() > _bitasset_data->current_initial_collateralization );
         FC_ASSERT( check,
            "Can only increase collateral ratio without increasing debt when the debt position's "
            "collateral ratio is lower than required initial collateral ratio (ICR), "
            "if not to trigger a margin call that be fully filled immediately",
            ("old_debt", old_debt)
            ("new_debt", call_obj->debt)
            ("old_collateralization", old_collateralization)
            ("new_collateralization", call_obj->collateralization() )
            );
      }
   }

   return call_order_id;
} FC_CAPTURE_AND_RETHROW( (o) ) }

} } // graphene::chain

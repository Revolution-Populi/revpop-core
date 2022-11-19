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

#include <fc/uint128.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/fba_accumulator_id.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/budget_record_object.hpp>
#include <graphene/chain/buyback_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/vote_count.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/worker_object.hpp>
#include <graphene/chain/custom_authority_object.hpp>

namespace graphene { namespace chain {

template<class Index>
vector<std::reference_wrapper<const typename Index::object_type>> database::sort_votable_objects(size_t count) const
{
   using ObjectType = typename Index::object_type;
   const auto& all_objects = get_index_type<Index>().indices();
   count = std::min(count, all_objects.size());
   vector<std::reference_wrapper<const ObjectType>> refs;
   refs.reserve(all_objects.size());
   std::transform(all_objects.begin(), all_objects.end(),
                  std::back_inserter(refs),
                  [](const ObjectType& o) { return std::cref(o); });
   std::partial_sort(refs.begin(), refs.begin() + count, refs.end(),
                   [this](const ObjectType& a, const ObjectType& b)->bool {
      share_type oa_vote = _vote_tally_buffer[a.vote_id];
      share_type ob_vote = _vote_tally_buffer[b.vote_id];
      if( oa_vote != ob_vote )
         return oa_vote > ob_vote;
      return a.vote_id < b.vote_id;
   });

   refs.resize(count, refs.front());
   return refs;
}

template<class Type>
void database::perform_account_maintenance(Type tally_helper)
{
   const auto& bal_idx = get_index_type< account_balance_index >().indices().get< by_maintenance_flag >();
   if( bal_idx.begin() != bal_idx.end() )
   {
      auto bal_itr = bal_idx.rbegin();
      while( bal_itr->maintenance_flag )
      {
         const account_balance_object& bal_obj = *bal_itr;

         modify( get_account_stats_by_owner( bal_obj.owner ), [&bal_obj](account_statistics_object& aso) {
            aso.core_in_balance = bal_obj.balance;
         });

         modify( bal_obj, []( account_balance_object& abo ) {
            abo.maintenance_flag = false;
         });

         bal_itr = bal_idx.rbegin();
      }
   }

   const auto& stats_idx = get_index_type< account_stats_index >().indices().get< by_maintenance_seq >();
   auto stats_itr = stats_idx.lower_bound( true );

   while( stats_itr != stats_idx.end() )
   {
      const account_statistics_object& acc_stat = *stats_itr;
      const account_object& acc_obj = acc_stat.owner( *this );
      ++stats_itr;

      if( acc_stat.has_some_core_voting() )
         tally_helper( acc_obj, acc_stat );

      if( acc_stat.has_pending_fees() )
         acc_stat.process_fees( acc_obj, *this );
   }

}

/// @brief A visitor for @ref worker_type which calls pay_worker on the worker within
struct worker_pay_visitor
{
   private:
      share_type pay;
      database& db;

   public:
      worker_pay_visitor(share_type pay, database& db)
         : pay(pay), db(db) {}

      typedef void result_type;
      template<typename W>
      void operator()(W& worker)const
      {
         worker.pay_worker(pay, db);
      }
};

void database::update_worker_votes()
{
   const auto& idx = get_index_type<worker_index>().indices().get<by_account>();
   auto itr = idx.begin();
   auto itr_end = idx.end();
   while( itr != itr_end )
   {
      modify( *itr, [this]( worker_object& obj )
      {
         obj.total_votes_for = _vote_tally_buffer[obj.vote_for];
         obj.total_cm_votes_for = _cm_vote_for_worker_buffer[obj.vote_for];
         obj.cm_support.swap(_cm_support_worker_buffer[obj.vote_for]);
      });
      ++itr;
   }
}

void database::pay_workers( share_type& budget )
{
   const auto head_time = head_block_time();
//   ilog("Processing payroll! Available budget is ${b}", ("b", budget));
   vector<std::reference_wrapper<const worker_object>> active_workers;
   // TODO optimization: add by_expiration index to avoid iterating through all objects
   uint64_t cm_size = get_global_properties().active_committee_members.size();
   get_index_type<worker_index>().inspect_all_objects([head_time, &active_workers, cm_size](const object& o) {
      const worker_object& w = static_cast<const worker_object&>(o);
      if( w.is_active(head_time) && w.cm_support_size() * 2 >= cm_size )
         active_workers.emplace_back(w);
   });

   // worker with more votes is preferred
   // if two workers exactly tie for votes, worker with lower ID is preferred
   std::sort(active_workers.begin(), active_workers.end(), [](const worker_object& wa, const worker_object& wb) {
      share_type wa_vote = wa.cm_support_size();
      share_type wb_vote = wb.cm_support_size();
      if( wa_vote != wb_vote )
         return wa_vote > wb_vote;
      return wa.id < wb.id;
   });

   const auto last_budget_time = get_dynamic_global_properties().last_budget_time;
   const auto passed_time_ms = head_time - last_budget_time;
   const auto passed_time_count = passed_time_ms.count();
   const auto day_count = fc::days(1).count();
   for( uint32_t i = 0; i < active_workers.size() && budget > 0; ++i )
   {
      const worker_object& active_worker = active_workers[i];
      share_type requested_pay = active_worker.daily_pay;

      // Note: if there is a good chance that passed_time_count == day_count,
      //       for better performance, can avoid the 128 bit calculation by adding a check.
      //       Since it's not the case on RevPop mainnet, we're not using a check here.
      fc::uint128_t pay = requested_pay.value;
      pay *= passed_time_count;
      pay /= day_count;
      requested_pay = static_cast<uint64_t>(pay);

      share_type actual_pay = std::min(budget, requested_pay);
      //ilog(" ==> Paying ${a} to worker ${w}", ("w", active_worker.id)("a", actual_pay));
      modify(active_worker, [&](worker_object& w) {
         w.worker.visit(worker_pay_visitor(actual_pay, *this));
      });

      budget -= actual_pay;
   }
}

void database::update_active_witnesses()
{ try {
   assert( _witness_count_histogram_buffer.size() > 0 );
   share_type stake_target = (_total_voting_stake[1]-_witness_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 witness do not get to express an opinion on
   /// the number of witnesses to have (they abstain and are non-voting accounts)

   share_type stake_tally = 0; 

   size_t witness_count = 0;
   if( stake_target > 0 )
   {
      while( (witness_count < _witness_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target) )
      {
         stake_tally += _witness_count_histogram_buffer[++witness_count];
      }
   }

   const global_property_object& gpo = get_global_properties();
   const chain_property_object& cpo = get_chain_properties();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();

   witness_count = std::max( witness_count*2+1, (size_t)cpo.immutable_parameters.min_witness_count );
   // RevPop: limit witnesses top list to max 63
   witness_count = std::min( witness_count, static_cast<size_t>(gpo.parameters.revpop_witnesses_top_max));
   auto wits = sort_votable_objects<witness_index>( witness_count );

   // RevPop: Get accounts of top witnesses
   vector<account_id_type> wits_acc;
   wits_acc.reserve( wits.size() );
   for( const witness_object& wit : wits )
   {
      wits_acc.push_back( wit.witness_account );
   }

   // RevPop: seed maintenance PRNG from commit-reveal scheme or chain_id + head block number
   if (HARDFORK_REVPOP_11_PASSED(head_block_time()))
   {
      uint64_t prng_seed = get_commit_reveal_seed_v2(wits_acc);
      if (prng_seed == 0)
      {
         // Fallback: seed PRNG from chain_id + head block num
         prng_seed = ((*(const uint64_t *)get_chain_id().data()) + dpo.head_block_number);
      }
      _maintenance_prng.seed(prng_seed);
   }
   else
   {
      uint64_t prng_seed = wits_acc.empty() ?
               // Fallback: seed PRNG from chain_id + head block num
               ((*(const uint64_t *)get_chain_id().data()) + dpo.head_block_number)
               :
               // Normal: seed PRNG from commit-reveal scheme
               get_commit_reveal_seed(wits_acc);
      _maintenance_prng.seed(prng_seed);
   }

   // RevPop: remove from top list witnesses without reveals
   {
      auto wits_acc_w_reveals = HARDFORK_REVPOP_11_PASSED(head_block_time()) ?
                     filter_commit_reveal_participant_v2(wits_acc)
                     :
                     filter_commit_reveal_participant(wits_acc);
      decltype(wits) enabled_wits;
      enabled_wits.reserve( wits_acc_w_reveals.size() );
      std::copy_if( wits.begin(), wits.end(), std::back_inserter( enabled_wits ),
                    [&wits_acc_w_reveals]( const witness_object & wits )
      {
         return std::find( wits_acc_w_reveals.begin(),
                           wits_acc_w_reveals.end(),
                           wits.witness_account ) != wits_acc_w_reveals.end();
      });
      if( !enabled_wits.empty() )
      {
         wits.swap(enabled_wits);
      }
      else
      {
         wlog("No top witnesses with reveals found. No-reveal penalties are not applicable.");
      }
   }

   if (HARDFORK_REVPOP_14_PASSED(head_block_time()))
   {
      const uint16_t electoral_threshold = gpo.parameters.get_electoral_threshold();
      uint32_t wits_size = std::min(                                   //21 or less
                           // as much as we want
                           (uint32_t)gpo.parameters.revpop_witnesses_active_max,
                           // as much as we can
                           (uint32_t)wits.size());

      decltype(wits) enabled_wits;
      enabled_wits.reserve( wits_size );
      
      // Sort all
      std::sort(wits.begin(), wits.end(),
            [&](const witness_object& a, const witness_object& b){
               return _vote_tally_buffer[a.vote_id] > _vote_tally_buffer[b.vote_id];
            });

      // the first round
      for( uint32_t i = 0; i < wits_size; ++i )
      {
         uint32_t jmax = wits_size - i;
         uint32_t j = i + _maintenance_prng.rand() % jmax;
         std::swap( wits[i], wits[j] );
      }
      uint32_t from_r1 = std::min(
                           // as much as we want
                           (uint32_t)gpo.parameters.revpop_witnesses_active_max - electoral_threshold,
                           // as much as we can
                           wits_size);
      std::copy(wits.begin(), wits.begin() + from_r1, back_inserter(enabled_wits));

      // the second round
      for( uint32_t i = from_r1; i < wits.size(); ++i )
      {
         uint32_t jmax = wits.size() - i;
         uint32_t j = i + _maintenance_prng.rand() % jmax;
         std::swap( wits[i], wits[j] );
      }
      uint32_t from_r2 = std::min(
                           // as much as we want
                           (uint32_t)electoral_threshold,
                           // as much as we can
                           wits_size - from_r1);
      std::copy(wits.begin() + from_r1, wits.begin() + from_r1 + from_r2, back_inserter(enabled_wits));

      // swap
      if( !enabled_wits.empty() )
      {
         wits.swap(enabled_wits);
      }
      else
      {
         wlog("The rdPoS algorithm missed, we use dPoS instead");
      }
   }
   else
   {
   // RevPop: shuffle witnesses top list
   for( uint32_t i = 0; i < wits.size(); ++i )
   {
      uint32_t jmax = wits.size() - i;
      uint32_t j = i + _maintenance_prng.rand() % jmax;
      std::swap( wits[i], wits[j] );
   }

   // RevPop: leave max 21 active witnesses in witnesses top list
   if( wits.size() > gpo.parameters.revpop_witnesses_active_max)
   {
      wits.erase( wits.begin() + gpo.parameters.revpop_witnesses_active_max, wits.end() );
   }
   }
   std::sort(wits.begin(), wits.end(),
             [&](const witness_object& a, const witness_object& b){
                return _vote_tally_buffer[a.vote_id] > _vote_tally_buffer[b.vote_id];
             });

   auto update_witness_total_votes = [this]( const witness_object& wit ) {
      modify( wit, [this]( witness_object& obj )
      {
         obj.total_votes = _vote_tally_buffer[obj.vote_id];
      });
   };

   if( _track_standby_votes )
   {
      const auto& all_witnesses = get_index_type<witness_index>().indices();
      for( const witness_object& wit : all_witnesses )
      {
         update_witness_total_votes( wit );
      }
   }
   else
   {
      for( const witness_object& wit : wits )
      {
         update_witness_total_votes( wit );
      }
   }

   // Update witness authority
   modify( get(GRAPHENE_WITNESS_ACCOUNT), [this,&wits]( account_object& a )
   {
      vote_counter vc;
      for( const witness_object& wit : wits )
         vc.add( wit.witness_account, _vote_tally_buffer[wit.vote_id] );
      vc.finish( a.active );
   } );

   modify( gpo, [&wits]( global_property_object& gp )
   {
      gp.active_witnesses.clear();
      gp.active_witnesses.reserve(wits.size());
      std::transform(wits.begin(), wits.end(),
                     std::inserter(gp.active_witnesses, gp.active_witnesses.end()),
                     [](const witness_object& w) {
         return w.id;
      });
   });

} FC_CAPTURE_AND_RETHROW() }

void database::update_active_committee_members()
{ try {
   assert( _committee_count_histogram_buffer.size() > 0 );
   share_type stake_target = (_total_voting_stake[0]-_committee_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 committee member do not get to express an opinion on
   /// the number of committee members to have (they abstain and are non-voting accounts)
   share_type stake_tally = 0;
   size_t committee_member_count = 0;
   if( stake_target > 0 )
   {
      while( (committee_member_count < _committee_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target.value) )
      {
         stake_tally += _committee_count_histogram_buffer[++committee_member_count];
      }
   }

   const chain_property_object& cpo = get_chain_properties();

   committee_member_count = std::max( committee_member_count*2+1, (size_t)cpo.immutable_parameters.min_committee_member_count );
   auto committee_members = sort_votable_objects<committee_member_index>( committee_member_count );

   auto update_committee_member_total_votes = [this]( const committee_member_object& cm ) {
      modify( cm, [this]( committee_member_object& obj )
      {
         obj.total_votes = _vote_tally_buffer[obj.vote_id];
      });
   };

   if( _track_standby_votes )
   {
      const auto& all_committee_members = get_index_type<committee_member_index>().indices();
      for( const committee_member_object& cm : all_committee_members )
      {
         update_committee_member_total_votes( cm );
      }
   }
   else
   {
      for( const committee_member_object& cm : committee_members )
      {
         update_committee_member_total_votes( cm );
      }
   }

   // Update committee authorities
   if( !committee_members.empty() )
   {
      const account_object& committee_account = get(GRAPHENE_COMMITTEE_ACCOUNT);
      modify( committee_account, [this,&committee_members](account_object& a)
      {
         vote_counter vc;
         for( const committee_member_object& cm : committee_members )
            vc.add( cm.committee_member_account, _vote_tally_buffer[cm.vote_id] );
         vc.finish( a.active );
      });
      modify( get(GRAPHENE_RELAXED_COMMITTEE_ACCOUNT), [&committee_account](account_object& a)
      {
         a.active = committee_account.active;
      });
   }
   modify( get_global_properties(), [&committee_members](global_property_object& gp)
   {
      gp.active_committee_members.clear();
      std::transform(committee_members.begin(), committee_members.end(),
                     std::inserter(gp.active_committee_members, gp.active_committee_members.begin()),
                     [](const committee_member_object& d) { return d.id; });
   });
} FC_CAPTURE_AND_RETHROW() }

void database::initialize_budget_record( fc::time_point_sec now, budget_record& rec )const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   const asset_object& core = get_core_asset();
   const asset_dynamic_data_object& core_dd = get_core_dynamic_data();

   rec.from_initial_reserve = core.reserved(*this);
   rec.from_accumulated_fees = core_dd.accumulated_fees;
   rec.from_unused_witness_budget = dpo.witness_budget;

   if(    (dpo.last_budget_time == fc::time_point_sec())
       || (now <= dpo.last_budget_time) )
   {
      rec.time_since_last_budget = 0;
      return;
   }

   int64_t dt = (now - dpo.last_budget_time).to_seconds();
   rec.time_since_last_budget = uint64_t( dt );

   // We'll consider accumulated_fees to be reserved at the BEGINNING
   // of the maintenance interval.  However, for speed we only
   // call modify() on the asset_dynamic_data_object once at the
   // end of the maintenance interval.  Thus the accumulated_fees
   // are available for the budget at this point, but not included
   // in core.reserved().
   share_type reserve = rec.from_initial_reserve + core_dd.accumulated_fees;
   // Similarly, we consider leftover witness_budget to be burned
   // at the BEGINNING of the maintenance interval.
   reserve += dpo.witness_budget;

   fc::uint128_t budget_u128 = reserve.value;
   budget_u128 *= uint64_t(dt);
   budget_u128 *= GRAPHENE_CORE_ASSET_CYCLE_RATE;
   //round up to the nearest satoshi -- this is necessary to ensure
   //   there isn't an "untouchable" reserve, and we will eventually
   //   be able to use the entire reserve
   budget_u128 += ((uint64_t(1) << GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS) - 1);
   budget_u128 >>= GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS;
   if( budget_u128 < static_cast<fc::uint128_t>(reserve.value) )
      rec.total_budget = share_type(static_cast<uint64_t>(budget_u128));
   else
      rec.total_budget = reserve;

   return;
}

/**
 * Update the budget for witnesses and workers.
 */
void database::process_budget()
{
   try
   {
      const global_property_object& gpo = get_global_properties();
      const dynamic_global_property_object& dpo = get_dynamic_global_properties();
      const asset_dynamic_data_object& core = get_core_dynamic_data();
      fc::time_point_sec now = head_block_time();

      int64_t time_to_maint = (dpo.next_maintenance_time - now).to_seconds();
      //
      // The code that generates the next maintenance time should
      //    only produce a result in the future.  If this assert
      //    fails, then the next maintenance time algorithm is buggy.
      //
      assert( time_to_maint > 0 );
      //
      // Code for setting chain parameters should validate
      //    block_interval > 0 (as well as the humans proposing /
      //    voting on changes to block interval).
      //
      assert( gpo.parameters.block_interval > 0 );
      uint64_t blocks_to_maint = (uint64_t(time_to_maint) + gpo.parameters.block_interval - 1) / gpo.parameters.block_interval;

      // blocks_to_maint > 0 because time_to_maint > 0,
      // which means numerator is at least equal to block_interval

      budget_record rec;
      initialize_budget_record( now, rec );
      share_type available_funds = rec.total_budget;

      share_type witness_budget = gpo.parameters.witness_pay_per_block.value * blocks_to_maint;
      rec.requested_witness_budget = witness_budget;
      witness_budget = std::min(witness_budget, available_funds);
      rec.witness_budget = witness_budget;
      available_funds -= witness_budget;

      fc::uint128_t worker_budget_u128 = gpo.parameters.worker_budget_per_day.value;
      worker_budget_u128 *= uint64_t(time_to_maint);
      worker_budget_u128 /= 60*60*24;

      share_type worker_budget;
      if( worker_budget_u128 >= static_cast<fc::uint128_t>(available_funds.value) )
         worker_budget = available_funds;
      else
         worker_budget = static_cast<uint64_t>(worker_budget_u128);
      rec.worker_budget = worker_budget;
      available_funds -= worker_budget;

      share_type leftover_worker_funds = worker_budget;
      pay_workers(leftover_worker_funds);
      rec.leftover_worker_funds = leftover_worker_funds;
      available_funds += leftover_worker_funds;

      rec.supply_delta = rec.witness_budget
         + rec.worker_budget
         - rec.leftover_worker_funds
         - rec.from_accumulated_fees
         - rec.from_unused_witness_budget;

      modify(core, [&]( asset_dynamic_data_object& _core )
      {
         _core.current_supply = (_core.current_supply + rec.supply_delta );

         assert( rec.supply_delta ==
                                   witness_budget
                                 + worker_budget
                                 - leftover_worker_funds
                                 - _core.accumulated_fees
                                 - dpo.witness_budget
                                );
         _core.accumulated_fees = 0;
      });

      modify(dpo, [&]( dynamic_global_property_object& _dpo )
      {
         // Since initial witness_budget was rolled into
         // available_funds, we replace it with witness_budget
         // instead of adding it.
         _dpo.witness_budget = witness_budget;
         _dpo.last_budget_time = now;
      });

      create< budget_record_object >( [&]( budget_record_object& _rec )
      {
         _rec.time = head_block_time();
         _rec.record = rec;
      });

      // available_funds is money we could spend, but don't want to.
      // we simply let it evaporate back into the reserve.
   }
   FC_CAPTURE_AND_RETHROW()
}

template< typename Visitor >
void visit_special_authorities( const database& db, Visitor visit )
{
   const auto& sa_idx = db.get_index_type< special_authority_index >().indices().get<by_id>();

   for( const special_authority_object& sao : sa_idx )
   {
      const account_object& acct = sao.account(db);
      if( !acct.owner_special_authority.is_type< no_special_authority >() )
      {
         visit( acct, true, acct.owner_special_authority );
      }
      if( !acct.active_special_authority.is_type< no_special_authority >() )
      {
         visit( acct, false, acct.active_special_authority );
      }
   }
}

void update_top_n_authorities( database& db )
{
   visit_special_authorities( db,
   [&]( const account_object& acct, bool is_owner, const special_authority& auth )
   {
      if( auth.is_type< top_holders_special_authority >() )
      {
         // use index to grab the top N holders of the asset and vote_counter to obtain the weights

         const top_holders_special_authority& tha = auth.get< top_holders_special_authority >();
         vote_counter vc;
         const auto& bal_idx = db.get_index_type< account_balance_index >().indices().get< by_asset_balance >();
         uint8_t num_needed = tha.num_top_holders;
         if( num_needed == 0 )
            return;

         // find accounts
         const auto range = bal_idx.equal_range( boost::make_tuple( tha.asset ) );
         for( const account_balance_object& bal : boost::make_iterator_range( range.first, range.second ) )
         {
             assert( bal.asset_type == tha.asset );
             if( bal.owner == acct.id )
                continue;
             vc.add( bal.owner, bal.balance.value );
             --num_needed;
             if( num_needed == 0 )
                break;
         }

         db.modify( acct, [&]( account_object& a )
         {
            vc.finish( is_owner ? a.owner : a.active );
            if( !vc.is_empty() )
               a.top_n_control_flags |= (is_owner ? account_object::top_n_control_owner : account_object::top_n_control_active);
         } );
      }
   } );
}

void split_fba_balance(
   database& db,
   uint64_t fba_id,
   uint16_t network_pct,
   uint16_t designated_asset_buyback_pct,
   uint16_t designated_asset_issuer_pct
)
{
   FC_ASSERT( uint32_t(network_pct) + uint32_t(designated_asset_buyback_pct) + uint32_t(designated_asset_issuer_pct) == GRAPHENE_100_PERCENT );
   const fba_accumulator_object& fba = fba_accumulator_id_type( fba_id )(db);
   if( fba.accumulated_fba_fees == 0 )
      return;

   const asset_dynamic_data_object& core_dd = db.get_core_dynamic_data();

   if( !fba.is_configured(db) )
   {
      ilog( "${n} core given to network at block ${b} due to non-configured FBA", ("n", fba.accumulated_fba_fees)("b", db.head_block_time()) );
      db.modify( core_dd, [&]( asset_dynamic_data_object& _core_dd )
      {
         _core_dd.current_supply -= fba.accumulated_fba_fees;
      } );
      db.modify( fba, [&]( fba_accumulator_object& _fba )
      {
         _fba.accumulated_fba_fees = 0;
      } );
      return;
   }

   fc::uint128_t buyback_amount_128 = fba.accumulated_fba_fees.value;
   buyback_amount_128 *= designated_asset_buyback_pct;
   buyback_amount_128 /= GRAPHENE_100_PERCENT;
   share_type buyback_amount = static_cast<uint64_t>(buyback_amount_128);

   fc::uint128_t issuer_amount_128 = fba.accumulated_fba_fees.value;
   issuer_amount_128 *= designated_asset_issuer_pct;
   issuer_amount_128 /= GRAPHENE_100_PERCENT;
   share_type issuer_amount = static_cast<uint64_t>(issuer_amount_128);

   // this assert should never fail
   FC_ASSERT( buyback_amount + issuer_amount <= fba.accumulated_fba_fees );

   share_type network_amount = fba.accumulated_fba_fees - (buyback_amount + issuer_amount);

   const asset_object& designated_asset = (*fba.designated_asset)(db);

   if( network_amount != 0 )
   {
      db.modify( core_dd, [&]( asset_dynamic_data_object& _core_dd )
      {
         _core_dd.current_supply -= network_amount;
      } );
   }

   fba_distribute_operation vop;
   vop.account_id = *designated_asset.buyback_account;
   vop.fba_id = fba.id;
   vop.amount = buyback_amount;
   if( vop.amount != 0 )
   {
      db.adjust_balance( *designated_asset.buyback_account, asset(buyback_amount) );
      db.push_applied_operation(vop);
   }

   vop.account_id = designated_asset.issuer;
   vop.fba_id = fba.id;
   vop.amount = issuer_amount;
   if( vop.amount != 0 )
   {
      db.adjust_balance( designated_asset.issuer, asset(issuer_amount) );
      db.push_applied_operation(vop);
   }

   db.modify( fba, [&]( fba_accumulator_object& _fba )
   {
      _fba.accumulated_fba_fees = 0;
   } );
}

void distribute_fba_balances( database& db )
{
   split_fba_balance( db, fba_accumulator_id_transfer_to_blind  , 20*GRAPHENE_1_PERCENT, 60*GRAPHENE_1_PERCENT, 20*GRAPHENE_1_PERCENT );
   split_fba_balance( db, fba_accumulator_id_blind_transfer     , 20*GRAPHENE_1_PERCENT, 60*GRAPHENE_1_PERCENT, 20*GRAPHENE_1_PERCENT );
   split_fba_balance( db, fba_accumulator_id_transfer_from_blind, 20*GRAPHENE_1_PERCENT, 60*GRAPHENE_1_PERCENT, 20*GRAPHENE_1_PERCENT );
}

void create_buyback_orders( database& db )
{
   const auto& bbo_idx = db.get_index_type< buyback_index >().indices().get<by_id>();
   const auto& bal_idx = db.get_index_type< primary_index< account_balance_index > >().get_secondary_index< balances_by_account_index >();

   for( const buyback_object& bbo : bbo_idx )
   {
      const asset_object& asset_to_buy = bbo.asset_to_buy(db);
      assert( asset_to_buy.buyback_account.valid() );

      const account_object& buyback_account = (*(asset_to_buy.buyback_account))(db);

      if( !buyback_account.allowed_assets.valid() )
      {
         wlog( "skipping buyback account ${b} at block ${n} because allowed_assets does not exist", ("b", buyback_account)("n", db.head_block_num()) );
         continue;
      }

      for( const auto& entry : bal_idx.get_account_balances( buyback_account.id ) )
      {
         const auto* it = entry.second;
         asset_id_type asset_to_sell = it->asset_type;
         share_type amount_to_sell = it->balance;
         if( asset_to_sell == asset_to_buy.id )
            continue;
         if( amount_to_sell == 0 )
            continue;
         if( buyback_account.allowed_assets->find( asset_to_sell ) == buyback_account.allowed_assets->end() )
         {
            wlog( "buyback account ${b} not selling disallowed holdings of asset ${a} at block ${n}", ("b", buyback_account)("a", asset_to_sell)("n", db.head_block_num()) );
            continue;
         }
      }
   }
   return;
}

void deprecate_annual_members( database& db )
{
   const auto& account_idx = db.get_index_type<account_index>().indices().get<by_id>();
   fc::time_point_sec now = db.head_block_time();
   for( const account_object& acct : account_idx )
   {
      try
      {
         transaction_evaluation_state upgrade_context(&db);
         upgrade_context.skip_fee_schedule_check = true;

         if( acct.is_annual_member( now ) )
         {
            account_upgrade_operation upgrade_vop;
            upgrade_vop.fee = asset( 0, asset_id_type() );
            upgrade_vop.account_to_upgrade = acct.id;
            upgrade_vop.upgrade_to_lifetime_member = true;
            db.apply_operation( upgrade_context, upgrade_vop );
         }
      }
      catch( const fc::exception& e )
      {
         // we can in fact get here, e.g. if asset issuer of buy/sell asset blacklists/whitelists the buyback account
         wlog( "Skipping annual member deprecate processing for account ${a} (${an}) at block ${n}; exception was ${e}",
               ("a", acct.id)("an", acct.name)("n", db.head_block_num())("e", e.to_detail_string()) );
         continue;
      }
   }
   return;
}

void database::process_bids( const asset_bitasset_data_object& bad )
{
   if( bad.is_prediction_market ) return;
   if( bad.current_feed.settlement_price.is_null() ) return;

   asset_id_type to_revive_id = (asset( 0, bad.options.short_backing_asset ) * bad.settlement_price).asset_id;
   const asset_object& to_revive = to_revive_id( *this );
   const asset_dynamic_data_object& bdd = to_revive.dynamic_data( *this );


   share_type covered = 0;
   if( covered < bdd.current_supply ) return;

   share_type to_cover = bdd.current_supply;
   share_type remaining_fund = bad.settlement_fund;

   FC_ASSERT( remaining_fund == 0 );
   FC_ASSERT( to_cover == 0 );

   _cancel_bids_and_revive_mpa( to_revive, bad );
}

/// Match call orders for all bitAssets, including PMs.
void match_call_orders( database& db )
{
   // Match call orders
   wlog( "Matching call orders at block ${n}", ("n",db.head_block_num()) );
   const auto& asset_idx = db.get_index_type<asset_index>().indices().get<by_type>();
   auto itr = asset_idx.lower_bound( true /** market issued */ );
   while( itr != asset_idx.end() )
   {
      const asset_object& a = *itr;
      ++itr;
      // be here, next_maintenance_time should have been updated already
      db.check_call_orders( a, true, false ); // allow black swan, and call orders are taker
   }
   wlog( "Done matching call orders at block ${n}", ("n",db.head_block_num()) );
}

void database::process_bitassets()
{
   time_point_sec head_time = head_block_time();
   uint32_t head_epoch_seconds = head_time.sec_since_epoch();

   const auto update_bitasset = [this,head_time,head_epoch_seconds]( asset_bitasset_data_object &o )
   {
      o.force_settled_volume = 0; // Reset all BitAsset force settlement volumes to zero

      // clear expired feeds
      const auto &asset = get( o.asset_id );
      auto flags = asset.options.flags;
      if ( ( flags & ( witness_fed_asset | committee_fed_asset ) ) &&
            o.options.feed_lifetime_sec < head_epoch_seconds ) // if smartcoin && check overflow
      {
         fc::time_point_sec calculated = head_time - o.options.feed_lifetime_sec;
         for( auto itr = o.feeds.rbegin(); itr != o.feeds.rend(); ) // loop feeds
         {
            auto feed_time = itr->second.first;
            std::advance( itr, 1 );
            if( feed_time < calculated )
               o.feeds.erase( itr.base() ); // delete expired feed
         }
      }
   };

   for( const auto& d : get_index_type<asset_bitasset_data_index>().indices() )
   {
      modify( d, update_bitasset );
      if( d.has_settlement() )
         process_bids(d);
   }
}

void update_median_feeds(database& db)
{
   time_point_sec head_time = db.head_block_time();
   time_point_sec next_maint_time = db.get_dynamic_global_properties().next_maintenance_time;

   const auto update_bitasset = [head_time, next_maint_time]( asset_bitasset_data_object &o )
   {
      o.update_median_feeds( head_time, next_maint_time );
   };

   for( const auto& d : db.get_index_type<asset_bitasset_data_index>().indices() )
   {
      db.modify( d, update_bitasset );
   }
}

/**
 * @brief Remove any custom active authorities whose expiration dates are in the past
 * @param db A mutable database reference
 */
void delete_expired_custom_authorities( database& db )
{
   const auto& index = db.get_index_type<custom_authority_index>().indices().get<by_expiration>();
   while (!index.empty() && index.begin()->valid_to < db.head_block_time())
      db.remove(*index.begin());
}

namespace detail {

   struct vote_recalc_times
   {
      time_point_sec full_power_time;
      time_point_sec zero_power_time;
   };

   struct vote_recalc_options
   {
      vote_recalc_options( uint32_t f, uint32_t d, uint32_t s )
      : full_power_seconds(f), recalc_steps(d), seconds_per_step(s)
      {
         total_recalc_seconds = ( recalc_steps - 1 ) * seconds_per_step; // should not overflow
         power_percents_to_subtract.reserve( recalc_steps - 1 );
         for( uint32_t i = 1; i < recalc_steps; ++i )
            power_percents_to_subtract.push_back( GRAPHENE_100_PERCENT * i / recalc_steps ); // should not overflow
      }

      vote_recalc_times get_vote_recalc_times( const time_point_sec now ) const
      {
         return { now - full_power_seconds, now - full_power_seconds - total_recalc_seconds };
      }

      uint32_t full_power_seconds;
      uint32_t recalc_steps; // >= 1
      uint32_t seconds_per_step;
      uint32_t total_recalc_seconds;
      vector<uint16_t> power_percents_to_subtract;

      static const vote_recalc_options witness();
      static const vote_recalc_options committee();
      static const vote_recalc_options worker();
      static const vote_recalc_options delegator();

      // return the stake that is "recalced to X"
      uint64_t get_recalced_voting_stake( const uint64_t stake, const time_point_sec last_vote_time,
                                         const vote_recalc_times& recalc_times ) const
      {
         if( last_vote_time > recalc_times.full_power_time )
            return stake;
         if( last_vote_time <= recalc_times.zero_power_time )
            return 0;
         uint32_t diff = recalc_times.full_power_time.sec_since_epoch() - last_vote_time.sec_since_epoch();
         uint32_t steps_to_subtract_minus_1 = diff / seconds_per_step;
         fc::uint128_t stake_to_subtract( stake );
         stake_to_subtract *= power_percents_to_subtract[steps_to_subtract_minus_1];
         stake_to_subtract /= GRAPHENE_100_PERCENT;
         return stake - static_cast<uint64_t>(stake_to_subtract);
      }
   };

   const vote_recalc_options vote_recalc_options::witness()
   {
      static const vote_recalc_options o( 360*86400, 8, 45*86400 );
      return o;
   }
   const vote_recalc_options vote_recalc_options::committee()
   {
      static const vote_recalc_options o( 360*86400, 8, 45*86400 );
      return o;
   }
   const vote_recalc_options vote_recalc_options::worker()
   {
      static const vote_recalc_options o( 360*86400, 8, 45*86400 );
      return o;
   }
   const vote_recalc_options vote_recalc_options::delegator()
   {
      static const vote_recalc_options o( 360*86400, 8, 45*86400 );
      return o;
   }
}

void database::perform_chain_maintenance(const signed_block& next_block, const global_property_object& global_props)
{
   const auto& gpo = get_global_properties();
   const auto& dgpo = get_dynamic_global_properties();

   distribute_fba_balances(*this);
   create_buyback_orders(*this);

   struct vote_tally_helper {
      database& d;
      const global_property_object& props;
      const dynamic_global_property_object& dprops;
      const time_point_sec now;
      const bool pob_activated;

      optional<detail::vote_recalc_times> witness_recalc_times;
      optional<detail::vote_recalc_times> committee_recalc_times;
      optional<detail::vote_recalc_times> worker_recalc_times;
      optional<detail::vote_recalc_times> delegator_recalc_times;

      vector<account_id_type> committee_members;

      vote_tally_helper( database& db )
         : d(db), props( d.get_global_properties() ), dprops( d.get_dynamic_global_properties() ),
           now( d.head_block_time() ),
           pob_activated( dprops.total_pob > 0 || dprops.total_inactive > 0 )
      {
         d._vote_tally_buffer.resize( props.next_available_vote_id, 0 );
         d._cm_vote_for_worker_buffer.resize( props.next_available_vote_id, 0 );
         d._cm_support_worker_buffer.resize( props.next_available_vote_id, {} );
         d._witness_count_histogram_buffer.resize( props.parameters.maximum_witness_count / 2 + 1, 0 );
         d._committee_count_histogram_buffer.resize( props.parameters.maximum_committee_count / 2 + 1, 0 );
         d._total_voting_stake[0] = 0;
         d._total_voting_stake[1] = 0;
         witness_recalc_times   = detail::vote_recalc_options::witness().get_vote_recalc_times( now );
         committee_recalc_times = detail::vote_recalc_options::committee().get_vote_recalc_times( now );
         worker_recalc_times    = detail::vote_recalc_options::worker().get_vote_recalc_times( now );
         delegator_recalc_times = detail::vote_recalc_options::delegator().get_vote_recalc_times( now );

         committee_members.reserve(props.active_committee_members.size());
         std::transform(props.active_committee_members.cbegin(), props.active_committee_members.cend(),
                        std::back_inserter(committee_members),
                        [&](committee_member_id_type c)
                        { return c(d).committee_member_account; });
         std::sort(committee_members.begin(), committee_members.end());
         /*
         ilog( "committee_members:");
         for (auto c : committee_members)
         {
            ilog( "         - ${n}", ("n", c(d).name) );
         }
         */
      }

      void operator()( const account_object& stake_account, const account_statistics_object& stats )
      {
         // PoB activation
         if( pob_activated && stats.total_core_pob == 0 && stats.total_core_inactive == 0 )
            return;

         if( props.parameters.count_non_member_votes || stake_account.is_member( now ) )
         {
            // There may be a difference between the account whose stake is voting and the one specifying opinions.
            // Usually they're the same, but if the stake account has specified a voting_account, that account is the
            // one specifying the opinions.
            bool directly_voting = ( stake_account.options.voting_account == GRAPHENE_PROXY_TO_SELF_ACCOUNT );
            const account_object& opinion_account = ( directly_voting ? stake_account
                                                      : d.get(stake_account.options.voting_account) );

            uint64_t voting_stake[3]; // 0=committee, 1=witness, 2=worker, as in vote_id_type::vote_type
            uint64_t num_committee_voting_stake; // number of committee members
            voting_stake[2] = ( pob_activated ? 0 : stats.total_core_in_orders.value )
                  + (stake_account.cashback_vb.valid() ? (*stake_account.cashback_vb)(d).balance.amount.value: 0)
                  + stats.core_in_balance.value;

            //PoB
            const uint64_t pol_amount = stats.total_core_pol.value;
            const uint64_t pol_value = stats.total_pol_value.value;
            const uint64_t pob_amount = stats.total_core_pob.value;
            const uint64_t pob_value = stats.total_pob_value.value;
            if( pob_amount == 0 )
            {
               voting_stake[2] += pol_value;
            }
            else if( pol_amount == 0 ) // and pob_amount > 0
            {
               if( pob_amount <= voting_stake[2] )
               {
                  voting_stake[2] += ( pob_value - pob_amount );
               }
               else
               {
                  auto base_value = static_cast<fc::uint128_t>( voting_stake[2] ) * pob_value / pob_amount;
                  voting_stake[2] = static_cast<uint64_t>( base_value );
               }
            }
            else if( pob_amount <= pol_amount ) // pob_amount > 0 && pol_amount > 0
            {
               auto base_value = static_cast<fc::uint128_t>( pob_value ) * pol_value / pol_amount;
               auto diff_value = static_cast<fc::uint128_t>( pob_amount ) * pol_value / pol_amount;
               base_value += ( pol_value - diff_value );
               voting_stake[2] += static_cast<uint64_t>( base_value );
            }
            else // pob_amount > pol_amount > 0
            {
               auto base_value = static_cast<fc::uint128_t>( pol_value ) * pob_value / pob_amount;
               fc::uint128_t diff_amount = pob_amount - pol_amount;
               if( diff_amount <= voting_stake[2] )
               {
                  auto diff_value = static_cast<fc::uint128_t>( pol_amount ) * pob_value / pob_amount;
                  base_value += ( pob_value - diff_value );
                  voting_stake[2] += static_cast<uint64_t>( base_value - diff_amount );
               }
               else // diff_amount > voting_stake[2]
               {
                  base_value += static_cast<fc::uint128_t>( voting_stake[2] ) * pob_value / pob_amount;
                  voting_stake[2] = static_cast<uint64_t>( base_value );
               }
            }

            // Shortcut
            if( voting_stake[2] == 0 )
               return;

            // Recalculate votes
            if( !directly_voting )
            {
               voting_stake[2] = detail::vote_recalc_options::delegator().get_recalced_voting_stake(
                                       voting_stake[2], stats.last_vote_time, *delegator_recalc_times );
            }
            const account_statistics_object& opinion_account_stats = ( directly_voting ? stats
                                       : opinion_account.statistics( d ) );
            voting_stake[1] = detail::vote_recalc_options::witness().get_recalced_voting_stake(
                                 voting_stake[2], opinion_account_stats.last_vote_time, *witness_recalc_times );
            voting_stake[0] = detail::vote_recalc_options::committee().get_recalced_voting_stake(
                                 voting_stake[2], opinion_account_stats.last_vote_time, *committee_recalc_times );
            num_committee_voting_stake = voting_stake[0];
            if( opinion_account.num_committee_voted > 1 )
               voting_stake[0] /= opinion_account.num_committee_voted;
            voting_stake[2] = detail::vote_recalc_options::worker().get_recalced_voting_stake(
                                 voting_stake[2], opinion_account_stats.last_vote_time, *worker_recalc_times );

            bool is_committee_members = false;
            const account_id_type account = stake_account.id;
            auto itr = std::lower_bound(committee_members.begin(), committee_members.end(), account);
            if( itr != committee_members.end() && *itr == account ) is_committee_members = true;
            for( vote_id_type id : opinion_account.options.votes )
            {
               uint32_t offset = id.instance();
               uint32_t type = std::min( id.type(), vote_id_type::vote_type::worker ); // cap the data
               // if they somehow managed to specify an illegal offset, ignore it.
               if( offset >= d._vote_tally_buffer.size()
                  || offset >= d._cm_vote_for_worker_buffer.size()
                  || offset >= d._cm_support_worker_buffer.size() )
                  continue;

               if (is_committee_members && type == vote_id_type::vote_type::worker)
               {
                  // Add up only the committee members votes
                  d._cm_vote_for_worker_buffer[offset] += voting_stake[type];
                  d._cm_support_worker_buffer[offset].push_back(account);
               }

               d._vote_tally_buffer[offset] += voting_stake[type];
            }

            // votes for a number greater than maximum_witness_count are skipped here
            if( voting_stake[1] > 0
                  && opinion_account.options.num_witness <= props.parameters.maximum_witness_count )
            {
               uint16_t offset = opinion_account.options.num_witness / 2;
               d._witness_count_histogram_buffer[offset] += voting_stake[1];
            }
            // votes for a number greater than maximum_committee_count are skipped here
            if( num_committee_voting_stake > 0
                  && opinion_account.options.num_committee <= props.parameters.maximum_committee_count )
            {
               uint16_t offset = opinion_account.options.num_committee / 2;
               d._committee_count_histogram_buffer[offset] += num_committee_voting_stake;
            }

            d._total_voting_stake[0] += num_committee_voting_stake;
            d._total_voting_stake[1] += voting_stake[1];
         }
      }
   } tally_helper(*this);

   perform_account_maintenance( tally_helper );

   struct clear_canary {
      clear_canary(vector<uint64_t>& target): target(target){}
      ~clear_canary() { target.clear(); }
   private:
      vector<uint64_t>& target;
   };
   clear_canary a(_witness_count_histogram_buffer),
                b(_committee_count_histogram_buffer),
                c(_vote_tally_buffer),
                d(_cm_vote_for_worker_buffer);

   update_top_n_authorities(*this);
   update_active_witnesses();
   update_active_committee_members();
   update_worker_votes();

   modify(gpo, [&dgpo](global_property_object& p) {
      // Remove scaling of account registration fee
      p.parameters.get_mutable_fees().get<account_create_operation>().basic_fee >>= p.parameters.account_fee_scale_bitshifts *
            (dgpo.accounts_registered_this_interval / p.parameters.accounts_per_fee_scale);

      if( p.pending_parameters )
      {
         p.parameters = std::move(*p.pending_parameters);
         p.pending_parameters.reset();
      }
   });

   auto next_maintenance_time = dgpo.next_maintenance_time;
   auto maintenance_interval = gpo.parameters.maintenance_interval;

   if( next_maintenance_time <= next_block.timestamp )
   {
      if( next_block.block_num() == 1 )
         next_maintenance_time = time_point_sec() +
               (((next_block.timestamp.sec_since_epoch() / maintenance_interval) + 1) * maintenance_interval);
      else
      {
         // We want to find the smallest k such that next_maintenance_time + k * maintenance_interval > head_block_time()
         //  This implies k > ( head_block_time() - next_maintenance_time ) / maintenance_interval
         //
         // Let y be the right-hand side of this inequality, i.e.
         // y = ( head_block_time() - next_maintenance_time ) / maintenance_interval
         //
         // and let the fractional part f be y-floor(y).  Clearly 0 <= f < 1.
         // We can rewrite f = y-floor(y) as floor(y) = y-f.
         //
         // Clearly k = floor(y)+1 has k > y as desired.  Now we must
         // show that this is the least such k, i.e. k-1 <= y.
         //
         // But k-1 = floor(y)+1-1 = floor(y) = y-f <= y.
         // So this k suffices.
         //
         auto y = (head_block_time() - next_maintenance_time).to_seconds() / maintenance_interval;
         next_maintenance_time += (y+1) * maintenance_interval;
      }
   }

   modify(dgpo, [next_maintenance_time](dynamic_global_property_object& d) {
      d.next_maintenance_time = next_maintenance_time;
      d.accounts_registered_this_interval = 0;
   });

   process_bitassets();
   delete_expired_custom_authorities(*this);

   // process_budget needs to run at the bottom because
   //   it needs to know the next_maintenance_time
   process_budget();

   for (vector<account_id_type>& at: _cm_support_worker_buffer)
   {
      at.clear();
   }
   _cm_support_worker_buffer.clear();
}

void database::maintenance_prng::seed(uint64_t seed)
{
   _seed = seed;
   _counter = 0;
}

uint64_t database::maintenance_prng::rand()
{
   /// High performance random generator
   /// http://xorshift.di.unimi.it/
   uint64_t k = _seed + _counter * 2685821657736338717ULL;
   k ^= (k >> 12);
   k ^= (k << 25);
   k ^= (k >> 27);
   k *= 2685821657736338717ULL;

   _counter++;

   return k;
}

uint64_t database::maintenance_prng::get_seed() const
{
   return _seed;
}

uint64_t database::get_maintenance_seed() const
{
   return _maintenance_prng.get_seed();
}

} }

/**
 * The Revolution Populi Project
 * Copyright (C) 2022 Revolution Populi Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <boost/test/unit_test.hpp>

#include <graphene/app/database_api.hpp>
#include <graphene/content_cards/content_cards.hpp>

#include "../common/database_fixture.hpp"

#define BOOST_TEST_MODULE Content cards plugin tests

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;

namespace {
   const std::string content_url = "http://some.image.url/img.jpg";
   const std::string content_buffer = "some content";
   const std::string hash = fc::sha256::hash(content_buffer);
   const std::string content_key = fc::ecc::private_key::generate().get_public_key().to_base58();
   const std::string content_type = "image/png";
   const std::string content_description = "Some image";
   const std::string content_storage_data = "[\"GD\",\"1.0\",\"file_id_in_google_disk\"]";
}

BOOST_FIXTURE_TEST_SUITE( content_cards_tests, database_fixture )

BOOST_AUTO_TEST_CASE(content_cards_plugin_disabled_test)
{
try {
   ACTORS((nathan)(alice)(robert)(patty));

   content_card_v2_create_operation op;
   op.subject_account = alice_id;
   op.hash = hash;
   op.url = content_url;
   op.type = content_type;
   op.description = content_description;
   op.content_key = content_key;
   op.storage_data = content_storage_data;
   op.fee = db.get_global_properties().parameters.get_current_fees().calculate_fee(op);


   signed_transaction trx;
   set_expiration(db, trx);
   trx.operations.push_back(op);
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   content_card_v2_id_type content_card_id = ptx.operation_results[0].get<object_id_type>();
   
   graphene::app::database_api db_api(db);
   GRAPHENE_REQUIRE_THROW(db_api.get_content_card_v2_by_id(content_card_id), fc::exception);
   GRAPHENE_REQUIRE_THROW(db_api.get_content_cards_v2(alice_id, content_card_id, 100), fc::exception);
}
catch (fc::exception &e) {
   edump((e.to_detail_string()));
   throw;
} }

BOOST_AUTO_TEST_CASE(content_cards_plugin_enabled_test)
{
try {
   ACTORS((nathan)(alice)(robert)(patty));

   app.enable_plugin("content_cards");

   content_card_v2_create_operation op;
   op.subject_account = alice_id;
   op.hash = hash;
   op.url = content_url;
   op.type = content_type;
   op.description = content_description;
   op.content_key = content_key;
   op.storage_data = content_storage_data;
   op.fee = db.get_global_properties().parameters.get_current_fees().calculate_fee(op);


   signed_transaction trx;
   set_expiration(db, trx);
   trx.operations.push_back(op);
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   content_card_v2_id_type content_card_id = ptx.operation_results[0].get<object_id_type>();

   graphene::app::database_api db_api(db);

   const auto &cc = db_api.get_content_card_v2_by_id(content_card_id);
   BOOST_CHECK( cc.valid() );
   BOOST_CHECK_EQUAL( cc->hash, hash );
   BOOST_CHECK_EQUAL( cc->content_key, content_key );
   BOOST_CHECK_EQUAL( cc->url, content_url );
   BOOST_CHECK_EQUAL( cc->type, content_type );
   BOOST_CHECK_EQUAL( cc->storage_data, content_storage_data );

   const auto& ccs = db_api.get_content_cards_v2(alice_id, content_card_id, 100);
   BOOST_CHECK_EQUAL( ccs[0].hash, hash );
   BOOST_CHECK_EQUAL( ccs[0].content_key, content_key );
   BOOST_CHECK_EQUAL( ccs[0].url, content_url );
   BOOST_CHECK_EQUAL( ccs[0].type, content_type );
   BOOST_CHECK_EQUAL( ccs[0].storage_data, content_storage_data );
}
catch (fc::exception &e) {
   edump((e.to_detail_string()));
   throw;
} }

BOOST_AUTO_TEST_SUITE_END()

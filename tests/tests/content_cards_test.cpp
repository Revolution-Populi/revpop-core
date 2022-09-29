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

   app.register_plugin<graphene::content_cards::content_cards_plugin>(true);

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

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


#include <boost/test/unit_test.hpp>

#include <graphene/app/database_api.hpp>

#include "../common/database_fixture.hpp"

#define BOOST_TEST_MODULE Personal data tests

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;

BOOST_FIXTURE_TEST_SUITE( personal_data_tests, database_fixture )

BOOST_AUTO_TEST_CASE( get_personal_data )
{ try {
   const auto owner_private_key = generate_private_key("owner of the data");
   const auto owner_account = create_account("owner", owner_private_key.get_public_key());

   const auto op_private_key = generate_private_key("operator for specific data");
   const auto op_account = create_account("op", op_private_key.get_public_key());

   graphene::app::database_api db_api(db, &(this->app.get_options()));

   // Same owner and operator.
   try {
      personal_data_create_operation op;
      op.subject_account = owner_account.get_id();
      op.operator_account = owner_account.get_id();
      op.url = "url";
      op.hash = fc::sha256::hash("data");
      op.storage_data = "storage_data";

      signed_transaction trx;
      set_expiration( db, trx );
      trx.operations.push_back(op);
      sign(trx, owner_private_key);
      trx.validate();
      PUSH_TX(db, trx);

      const auto data = db_api.get_last_personal_data(owner_account.get_id(), owner_account.get_id());
      BOOST_CHECK(data);
      BOOST_CHECK(data->subject_account == owner_account.get_id());
      BOOST_CHECK(data->operator_account == owner_account.get_id());
      BOOST_CHECK(data->url == op.url);
      BOOST_CHECK(data->hash == op.hash);
      BOOST_CHECK(data->storage_data == op.storage_data);

      BOOST_CHECK(!db_api.get_last_personal_data(owner_account.get_id(), op_account.get_id()));
   } FC_LOG_AND_RETHROW()

   // Operator has access to the data.
   try {
      personal_data_create_operation op;
      op.subject_account = owner_account.get_id();
      op.operator_account = op_account.get_id();
      op.url = "new_url";
      op.hash = fc::sha256::hash("new_data");
      op.storage_data = "new_storage_data";

      signed_transaction trx;
      set_expiration( db, trx );
      trx.operations.push_back(op);
      sign(trx, owner_private_key);
      trx.validate();
      PUSH_TX(db, trx);

      const auto data = db_api.get_last_personal_data(owner_account.get_id(), op_account.get_id());
      BOOST_REQUIRE(data);
      BOOST_CHECK(data->subject_account == owner_account.get_id());
      BOOST_CHECK(data->operator_account == op_account.get_id());
      BOOST_CHECK(data->url == op.url);
      BOOST_CHECK(data->hash == op.hash);
      BOOST_CHECK(data->storage_data == op.storage_data);

      // Operator has access only to new personal data and nothing more.
      BOOST_CHECK(db_api.get_personal_data(owner_account.get_id(), op_account.get_id()).size() == 1);
   } FC_LOG_AND_RETHROW()

   // Operator can remove personal data and its not available anymore.
   try {
      personal_data_remove_operation op;
      op.subject_account = owner_account.get_id();
      op.operator_account = op_account.get_id();
      op.hash = fc::sha256::hash("new_data");

      signed_transaction trx;
      set_expiration( db, trx );
      trx.operations.push_back(op);
      sign(trx, owner_private_key);
      trx.validate();
      PUSH_TX(db, trx);

      BOOST_CHECK(!db_api.get_last_personal_data(owner_account.get_id(), op_account.get_id()));
      BOOST_CHECK(db_api.get_personal_data(owner_account.get_id(), op_account.get_id()).empty());

      // Other data is not affected.
      BOOST_CHECK(db_api.get_last_personal_data(owner_account.get_id(), owner_account.get_id()));
      BOOST_CHECK(!db_api.get_personal_data(owner_account.get_id(), owner_account.get_id()).empty());
   } FC_LOG_AND_RETHROW()
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

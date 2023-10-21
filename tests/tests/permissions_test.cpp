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

#define BOOST_TEST_MODULE Premission tests

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;

BOOST_FIXTURE_TEST_SUITE( permission_tests, database_fixture )

BOOST_AUTO_TEST_CASE( get_permissions )
{ try {
   const auto private_key = generate_private_key("private_key");
   const auto account = create_account("account", private_key.get_public_key());

   graphene::app::database_api db_api(db, &(this->app.get_options()));

   BOOST_CHECK(db_api.get_permissions(account.get_id(), permission_id_type(0u), 255u).empty());

   // Create new permission
   permission_create_operation perm_op;
   perm_op.subject_account = account.get_id();
   perm_op.operator_account = account.get_id();
   perm_op.permission_type = "type";
   perm_op.object_id = object_id_type(1, 2, 3);
   perm_op.content_key = "content";

   signed_transaction trx;
   set_expiration( db, trx );
   trx.operations.push_back(perm_op);
   sign(trx, private_key);
   trx.validate();
   
   const auto ptx = PUSH_TX(db, trx);
   auto permission_id = ptx.operation_results[0].get<object_id_type>();

   // The permission is accessable via get_permissions and get_permission_by_id APIs.
   try {
      const auto permissions = db_api.get_permissions(account.get_id(), permission_id_type(0u), 255u);
      BOOST_REQUIRE_EQUAL(permissions.size(), 1);
      BOOST_CHECK(permissions[0].subject_account == perm_op.subject_account);
      BOOST_CHECK(permissions[0].operator_account == perm_op.operator_account);
      BOOST_CHECK(permissions[0].permission_type == perm_op.permission_type);
      BOOST_CHECK(permissions[0].object_id == perm_op.object_id);
      BOOST_CHECK(permissions[0].content_key == perm_op.content_key);

      const auto permission_by_id = db_api.get_permission_by_id(permission_id);
      BOOST_CHECK(permission_by_id->subject_account == perm_op.subject_account);
      BOOST_CHECK(permission_by_id->operator_account == perm_op.operator_account);
      BOOST_CHECK(permission_by_id->permission_type == perm_op.permission_type);
      BOOST_CHECK(permission_by_id->object_id == perm_op.object_id);
      BOOST_CHECK(permission_by_id->content_key == perm_op.content_key);
   } FC_LOG_AND_RETHROW()

   // Invalid permission id.
   try {
      BOOST_CHECK(!db_api.get_permission_by_id(*perm_op.object_id));
      BOOST_CHECK(db_api.get_permissions(*perm_op.object_id, permission_id_type(0u), 255u).empty());
   } FC_LOG_AND_RETHROW()

   // Remove permission.
   try {
      permission_remove_operation perm_op;
      perm_op.subject_account = account.get_id();
      perm_op.permission_id = permission_id;

      signed_transaction trx;
      set_expiration( db, trx );
      trx.operations.push_back(perm_op);
      sign(trx, private_key);
      trx.validate();
      PUSH_TX(db, trx);

      const auto permissions = db_api.get_permissions(account.get_id(), permission_id_type(0u), 255u);
      BOOST_CHECK(permissions.empty());
      BOOST_CHECK(!db_api.get_permission_by_id(permission_id));
   } FC_LOG_AND_RETHROW()
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( multiple_permissions )
{ try {
   const auto private_key = generate_private_key("private_key");
   const auto account = create_account("account", private_key.get_public_key());

   graphene::app::database_api db_api(db, &(this->app.get_options()));

   BOOST_CHECK(db_api.get_permissions(account.get_id(), permission_id_type(0u), 255u).empty());

   // Create permissions
   permission_create_operation perm_op1;
   perm_op1.subject_account = account.get_id();
   perm_op1.operator_account = account.get_id();
   perm_op1.permission_type = "type";
   perm_op1.content_key = "content";
   
   permission_create_operation perm_op2;
   perm_op2.subject_account = account.get_id();
   perm_op2.operator_account = account.get_id();
   perm_op2.permission_type = "another_type";
   perm_op2.content_key = "another_content";

   signed_transaction trx;
   set_expiration( db, trx );
   trx.operations.push_back(perm_op1);
   trx.operations.push_back(perm_op2);
   sign(trx, private_key);
   trx.validate();
   
   const auto ptx = PUSH_TX(db, trx);
   auto last_permission_id = ptx.operation_results[1].get<object_id_type>();
   
   BOOST_CHECK(db_api.get_permissions(account.get_id(), permission_id_type(0u), 2u).size() == trx.operations.size());
   BOOST_CHECK(db_api.get_permissions(account.get_id(), permission_id_type(0u), 1u).size() == 1u);
   BOOST_CHECK(db_api.get_permissions(account.get_id(), permission_id_type(0u), 0u).size() == 0u);
   BOOST_CHECK(db_api.get_permissions(account.get_id(), last_permission_id, 2u).size() == 1u);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

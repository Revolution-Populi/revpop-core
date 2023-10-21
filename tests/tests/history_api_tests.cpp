/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
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

#include <graphene/app/api.hpp>

#include <graphene/utilities/tempdir.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;
BOOST_FIXTURE_TEST_SUITE( history_api_tests, database_fixture )

BOOST_AUTO_TEST_CASE(get_account_history) {
   try {
      graphene::app::history_api hist_api(app);

      //account_id_type() do 3 ops
      auto nathan = create_account("nathan");
      create_user_issued_asset("USD", nathan, 0);
      create_account("dan");
      create_account("bob");

      generate_block();
      fc::usleep(fc::milliseconds(2000));

      //int asset_create_op_id = operation::tag<asset_create_operation>::value;
      int account_create_op_id = operation::tag<account_create_operation>::value;

      //account_id_type() did 3 ops and includes id0
      vector<operation_history_object> histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 100, operation_history_id_type());

      BOOST_CHECK_EQUAL(histories.size(), 3u);
      BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);
      BOOST_CHECK_EQUAL(histories[2].op.which(), account_create_op_id);

      // 1 account_create op larger than id1
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 100, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK(histories[0].id.instance() != 0);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

      // Limit 2 returns 2 result
      histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 2, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 2u);
      BOOST_CHECK(histories[1].id.instance() != 0);
      BOOST_CHECK_EQUAL(histories[1].op.which(), account_create_op_id);
      // bob has 1 op
      histories = hist_api.get_account_history("bob", operation_history_id_type(), 100, operation_history_id_type());
      BOOST_CHECK_EQUAL(histories.size(), 1u);
      BOOST_CHECK_EQUAL(histories[0].op.which(), account_create_op_id);

   } catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()

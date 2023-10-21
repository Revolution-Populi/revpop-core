/**
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

#include <graphene/content_cards/content_cards.hpp>

namespace graphene { namespace content_cards {

content_cards_plugin::content_cards_plugin(graphene::app::application& app) :
   plugin(app)
{
   // Nothing else to do
}

content_cards_plugin::~content_cards_plugin() = default;

std::string content_cards_plugin::plugin_name()const
{
   return "content_cards";
}
std::string content_cards_plugin::plugin_description()const
{
   return "Stores full content of content_cards and allows access to them.";
}

void content_cards_plugin::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg
   )
{
}

void content_cards_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
}

void content_cards_plugin::plugin_startup()
{
   ilog("content_cards: plugin_startup() begin");
}

} }

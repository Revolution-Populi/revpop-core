/*
 * Copyright (c) 2018 Abit More, and contributors.
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

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/version.hpp>

#include <graphene/app/util.hpp>
#include <graphene/protocol/asset.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/utilities/git_revision.hpp>
#include <websocketpp/version.hpp>

#ifdef _WIN32
#include <windows.h>
#elif MACOS
#include <sys/param.h>
#include <sys/sysctl.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#include "sys/types.h"
#include "sys/sysinfo.h"
#endif

#include <string>

namespace graphene { namespace app {

using boost::multiprecision::uint256_t;

static fc::uint128_t to_capped_128( const uint256_t& t )
{
   if( t >= std::numeric_limits<fc::uint128_t>::max() )
      return std::numeric_limits<fc::uint128_t>::max();
   return static_cast<fc::uint128_t>(t);
}

std::string uint128_amount_to_string( const fc::uint128_t& amount, const uint8_t precision )
{ try {
   std::string s = fc::variant( amount, 2 ).as_string();
   if( precision == 0 || amount == fc::uint128_t() )
      return s;

   std::stringstream ss;
   uint8_t pos = s.find_last_not_of( '0' ); // should be >= 0
   uint8_t len = s.size();
   if( len > precision )
   {
      uint8_t left_len = len - precision;
      ss << s.substr( 0, left_len );
      if( pos >= left_len )
         ss << '.' << s.substr( left_len, pos - left_len + 1 );
   }
   else
   {
      ss << "0.";
      for( uint8_t i = precision - len; i > 0; --i )
         ss << '0';
      ss << s.substr( 0, pos + 1 );
   }
   return ss.str();
} FC_CAPTURE_AND_RETHROW( (amount)(precision) ) }

std::string price_to_string( const graphene::protocol::price& _price,
                             const uint8_t base_precision,
                             const uint8_t quote_precision )
{ try {
   if( _price.base.amount == 0 )
      return "0";
   FC_ASSERT( _price.base.amount >= 0 );
   FC_ASSERT( _price.quote.amount >= 0 );
   FC_ASSERT( base_precision <= 19 );
   FC_ASSERT( quote_precision <= 19 );
   graphene::protocol::price new_price = _price;
   if( new_price.quote.amount == 0 )
   {
      new_price.base.amount = std::numeric_limits<int64_t>::max();
      new_price.quote.amount = 1;
   }

   // times (10**19) so won't overflow but have good accuracy
   fc::uint128_t price128 = fc::uint128_t( new_price.base.amount.value ) * uint64_t(10000000000000000000ULL)
                                                                     / new_price.quote.amount.value;

   return uint128_amount_to_string( price128, 19 + base_precision - quote_precision );
} FC_CAPTURE_AND_RETHROW( (_price)(base_precision)(quote_precision) ) }

std::string price_to_string( const graphene::protocol::price& _price,
                             const graphene::chain::asset_object& _base,
                             const graphene::chain::asset_object& _quote )
{ try {
   if( _price.base.asset_id == _base.id && _price.quote.asset_id == _quote.id )
      return price_to_string( _price, _base.precision, _quote.precision );
   else if( _price.base.asset_id == _quote.id && _price.quote.asset_id == _base.id )
      return price_to_string( ~_price, _base.precision, _quote.precision );
   else
      FC_ASSERT( !"bad parameters" );
} FC_CAPTURE_AND_RETHROW( (_price)(_base)(_quote) ) }

std::string price_diff_percent_string( const graphene::protocol::price& old_price,
                                       const graphene::protocol::price& new_price )
{ try {
   FC_ASSERT( old_price.base.asset_id == new_price.base.asset_id );
   FC_ASSERT( old_price.quote.asset_id == new_price.quote.asset_id );
   FC_ASSERT( old_price.base.amount >= 0 );
   FC_ASSERT( old_price.quote.amount >= 0 );
   FC_ASSERT( new_price.base.amount >= 0 );
   FC_ASSERT( new_price.quote.amount >= 0 );
   graphene::protocol::price old_price1 = old_price;
   if( old_price.base.amount == 0 )
   {
      old_price1.base.amount = 1;
      old_price1.quote.amount = std::numeric_limits<int64_t>::max();
   }
   else if( old_price.quote.amount == 0 )
   {
      old_price1.base.amount = std::numeric_limits<int64_t>::max();
      old_price1.quote.amount = 1;
   }
   graphene::protocol::price new_price1 = new_price;
   if( new_price.base.amount == 0 )
   {
      new_price1.base.amount = 1;
      new_price1.quote.amount = std::numeric_limits<int64_t>::max();
   }
   else if( new_price.quote.amount == 0 )
   {
      new_price1.base.amount = std::numeric_limits<int64_t>::max();
      new_price1.quote.amount = 1;
   }

   // change = new/old - 1 = (new_base/new_quote)/(old_base/old_quote) - 1
   //        = (new_base * old_quote) / (new_quote * old_base) - 1
   //        = (new_base * old_quote - new_quote * old_base) / (new_quote * old_base)
   uint256_t new256 = uint256_t( new_price1.base.amount.value ) * old_price1.quote.amount.value;
   uint256_t old256 = uint256_t( old_price1.base.amount.value ) * new_price1.quote.amount.value;
   bool non_negative = (new256 >= old256);
   uint256_t diff256;
   if( non_negative )
      diff256 = new256 - old256;
   else
      diff256 = old256 - new256;
   diff256 = diff256 * 10000 / old256;
   std::string diff_str = uint128_amount_to_string( to_capped_128(diff256), 2 ); // at most 2 decimal digits
   if( non_negative || diff_str == "0" )
      return diff_str;
   else
      return "-" + diff_str;
} FC_CAPTURE_AND_RETHROW( (old_price)(new_price) ) }

int get_num_cores() {
#ifdef WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif MACOS
    int nm[2];
    size_t len = 4;
    uint32_t count;

    nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);

    if(count < 1) {
        nm[1] = HW_NCPU;
        sysctl(nm, 2, &count, &len, NULL, 0);
        if(count < 1) { count = 1; }
    }
    return count;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

// Represent memory size in Mb
struct memory_info {
   int phys_total = 0;
   int phys_avail = 0;
   int virt_total = 0;
};

memory_info get_system_memory_info()
{
   constexpr long megabyte_size = 1024 * 1024;
   memory_info mem_info;
#ifdef WIN32
   MEMORYSTATUSEX memInfo;
   memInfo.dwLength = sizeof(MEMORYSTATUSEX);
   GlobalMemoryStatusEx(&memInfo);

   mem_info.phys_total = memInfo.ullTotalPhys / megabyte_size;
   mem_info.phys_avail = memInfo.ullAvailPhys / megabyte_size;
   mem_info.virt_total = memInfo.ullTotalVirtual / megabyte_size;
#elif defined(__linux__)
   struct sysinfo memInfo;
   sysinfo(&memInfo);

   mem_info.phys_total = memInfo.totalram * memInfo.mem_unit / megabyte_size;
   mem_info.phys_avail = memInfo.freeram * memInfo.mem_unit / megabyte_size;
   mem_info.virt_total = (memInfo.totalram + memInfo.totalswap) * memInfo.mem_unit / megabyte_size;
#endif
   return mem_info;
}

std::string get_os_version() {
   std::string result = "N/A";
#if defined(__linux__)
   struct utsname un;
   if (uname(&un) == 0)
      result = std::string(un.sysname) + " " + un.version + " " + un.release + " " + un.machine;
#endif
   return result;
}

void log_system_info()
{
   ilog("Version: ${ver}",     ("ver",   graphene::utilities::git_revision_description) );
   ilog("SHA: ${sha}",         ("sha",   graphene::utilities::git_revision_sha) );
   ilog("Timestamp: ${tstmp}", ("tstmp", fc::get_approximate_relative_time_string(fc::time_point_sec(graphene::utilities::git_revision_unix_timestamp))) );
   ilog("SSL: ${ssl}",         ("ssl",   OPENSSL_VERSION_TEXT) );
   ilog("Boost: ${boost}",     ("boost", boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".")) );
   ilog("Websocket++: ${ws}",  ("ws",    std::to_string(websocketpp::major_version) + "." + std::to_string(websocketpp::minor_version) + "." + std::to_string(websocketpp::patch_version) ));
#if defined(__linux__)
   ilog("Platform: ${plfm}",   ("plfm",  get_os_version()) );
#else
   ilog("Platform: ${plfm}",   ("plfm",  boost::replace_all_copy(std::string(BOOST_PLATFORM), "_", ".")) );
#endif
   ilog("CPU count: ${cpus}",  ("cpus",  get_num_cores()) );
   auto mem_info = get_system_memory_info();
   ilog("RAM total size: ${tot_ram}Mb",       ("tot_ram", mem_info.phys_total) );
   ilog("RAM available size: ${avail_ram}Mb", ("avail_ram", mem_info.phys_avail) );
   ilog("Total virtual memory size: ${tot_virt}Mb", ("tot_virt", mem_info.virt_total) );
}

} } // graphene::app

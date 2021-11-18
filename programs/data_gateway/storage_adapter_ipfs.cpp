/**
 * The Revolution Populi Project
 * Copyright (C) 2021 Revolution Populi Limited
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

#include "storage_adapter_ipfs.hpp"

#include <fc/network/http/websocket.hpp>
#include <stdio.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>

// #define DATA_SIZE 10
// int Curr_index;

namespace graphene { namespace gateway {

   storage_adapter_ipfs::storage_adapter_ipfs(const std::string& url) 
      : storage_adapter()
      , ipfs_node_url(url)
   {
   }

   storage_adapter_ipfs::~storage_adapter_ipfs()
   {
   }

   static int getTransferStatus(CURLM *multiHandleInstance, CURL *currentHandle)
   {
      //check the status of transfer    CURLcode return_code=0;
      int msgs_left = 0;
      CURLMsg *msg = NULL;
      CURL *eh = NULL;
      CURLcode return_code;
      int http_status_code = 0;
      while ((msg = curl_multi_info_read(multiHandleInstance, &msgs_left)))
      {
         if (msg->msg == CURLMSG_DONE)
         {
            eh = msg->easy_handle;
            if (currentHandle == eh)
            {
               return_code = msg->data.result;
               if ((return_code != CURLE_OK) && (return_code != CURLE_RECV_ERROR))
               {
                  fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                  continue;
               }
               // Get HTTP status code
               http_status_code = 0;
               curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &http_status_code);
            }
         }
      }
      return http_status_code;
   }

   static size_t processResponse(void *contents, size_t size, size_t nmemb, void *userp)
   {
      //write callback
      ((std::string*)userp)->append((char*)contents, size * nmemb);
      return size * nmemb;
   }

   /** Receive content file and give it to storage adapter.
    *  @return IPFS CID of stored file
    *
    */
   std::string storage_adapter_ipfs::store_content(const file_upload& file)
   {
      ilog("Sending file to IPFS: ${name}, path: ${path}", ("name", file.name)("path", file.path));
      std::string res;

      struct curl_slist *header = NULL;
      header = curl_slist_append(header, "Content-Type: multipart/form-data");
      // header = curl_slist_append(header, "Transfer-Encoding: chunked");

      struct curl_httppost *formpost = NULL;
      struct curl_httppost *lastptr = NULL;

      curl_formadd(&formpost,
                   &lastptr,
                   CURLFORM_COPYNAME, file.name.c_str(),
                   CURLFORM_FILE, file.path.c_str(),
                   CURLFORM_END);

      // init the curl session
      CURL *eventHttp_handle = curl_easy_init();
      CURLM *multi_handle = curl_multi_init();
      std::string response_body;

      if (eventHttp_handle)
      {
         // struct data config;
         std::string url = ipfs_node_url + "/api/v0/add";
         curl_easy_setopt(eventHttp_handle, CURLOPT_URL, url.c_str());

         /* disable progress meter, set to 0L to enable and disable debug output */
         curl_easy_setopt(eventHttp_handle, CURLOPT_VERBOSE, 1L);
         curl_easy_setopt(eventHttp_handle, CURLOPT_NOPROGRESS, 0L);

         //write callback function
         curl_easy_setopt(eventHttp_handle, CURLOPT_WRITEFUNCTION, processResponse);
         curl_easy_setopt(eventHttp_handle, CURLOPT_WRITEDATA, (void *)&response_body);

         curl_easy_setopt(eventHttp_handle, CURLOPT_HTTPPOST, formpost);
         curl_easy_setopt(eventHttp_handle, CURLOPT_HTTPHEADER, header);

         //adding easy handle to Multihandle
         curl_multi_add_handle(multi_handle, eventHttp_handle);

      int still_running = 0;
         do
         {
            CURLMcode mc;
            int numfds;

            /* when CURLFORM_CONTENTSLENGTH is set to false value
               the "still_running" (active handles) never returns to 0*/
            mc = curl_multi_perform(multi_handle, &still_running);

            if (mc == CURLM_OK)
            {
               /* wait for activity, timeout or "nothing" */
               mc = curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
            }
            if (mc != CURLM_OK)
            {
               fprintf(stderr, "curl_multi failed, code %d.n", mc);
               break;
            }
            /* 'numfds' being zero means either a timeout or no file descriptors to
               wait for. Try timeout on first occurrence, then assume no file
               descriptors and no file descriptors to wait for means wait for 100
               milliseconds. */

            if (!numfds)
            {
               struct timeval wait = {0, 100 * 1000}; // 100ms
               select(0, NULL, NULL, NULL, &wait);
            }

         } while (still_running);

         if (header)
            curl_slist_free_all(header);

         if (formpost)
         {
            curl_formfree(formpost);
            formpost = NULL;
         }

         int responseCode = getTransferStatus(multi_handle, eventHttp_handle);
         if (responseCode == 200) //200 OK!
         {
            auto hash_start = response_body.find("Hash\":\"");
            if (hash_start != std::string::npos) {
               hash_start += 7;
               auto hash_end = response_body.find_first_of("\"", hash_start);
               if (hash_end != std::string::npos) {
                  res = response_body.substr(hash_start, hash_end-hash_start);
               }
            }
         }
      }
      return res;
   }

   /**
    *
    *
    */
   void storage_adapter_ipfs::remove_content()
   {
   }

   /**
    *
    *
    */
   void storage_adapter_ipfs::get_content_list()
   {
   }

}} // namespace graphene::gateway

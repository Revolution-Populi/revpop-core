file(GLOB GATEWAY_SRC "./*.cpp")
add_executable( data_gateway ${GATEWAY_SRC} )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling data_gateway with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

find_curl()

include_directories(${CURL_INCLUDE_DIRS})

if(CURL_STATICLIB)
  SET_TARGET_PROPERTIES(data_gateway PROPERTIES
  COMPILE_DEFINITIONS "CURL_STATICLIB")
endif(CURL_STATICLIB)

target_link_libraries( data_gateway
      PRIVATE graphene_app graphene_egenesis_brief graphene_wallet fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} ${CURL_LIBRARIES})

if(MSVC)
  set_source_files_properties( main.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   data_gateway

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

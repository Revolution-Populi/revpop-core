add_executable( etherium_keys main.cpp )

target_link_libraries( etherium_keys
                       PRIVATE fc tokendistribution ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   etherium_keys

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)

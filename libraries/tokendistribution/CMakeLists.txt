file(GLOB HEADERS "include/graphene/tokendistribution/*.hpp")

set( SOURCES
     Keccak256.cpp
     tokendistribution.cpp
   )

add_library( tokendistribution ${SOURCES} ${HEADERS} )
target_include_directories( tokendistribution
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
target_link_libraries( tokendistribution
                            PRIVATE fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )
install( TARGETS
   tokendistribution

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/graphene/tokendistribution" )

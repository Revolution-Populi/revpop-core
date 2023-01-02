file(GLOB HEADERS "include/graphene/tokendistribution/*.hpp")

set( SOURCES
     Keccak256.cpp
   )

add_library( tokendistribution ${SOURCES} ${HEADERS} )
target_include_directories( tokendistribution
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   tokendistribution

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
install( FILES ${HEADERS} DESTINATION "include/graphene/tokendistribution" )

file(GLOB HEADERS "include/graphene/content_cards/*.hpp")

add_library( graphene_content_cards
        content_cards.cpp
           )

target_link_libraries( graphene_content_cards graphene_chain graphene_app )
target_include_directories( graphene_content_cards
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties(content_cards.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_content_cards

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/content_cards" )

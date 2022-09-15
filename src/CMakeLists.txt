cmake_minimum_required( VERSION 3.0.0 )

project( emcc.index.html )

set( CMAKE_CXX_STANDARD 20 )

add_compile_definitions( ZLIB_CONST=1
                         BOOST_SML_CFG_DISABLE_MIN_SIZE # See https://github.com/boost-ext/sml/issues/249. Without this defined, UB detected.
                       )


# Note: With WASM=0, source-mappings are preserved; however, it is considerably slower.
# Note: -Wno-unused-command-line-argument used to cut down on the clutter. For some reason, compile args are being passed to linker.
add_compile_options( -Wall
                     -Werror=return-type
                     -Wno-invalid-partial-specialization
                     -Wno-unused-command-line-argument
                     --bind
                     -sFORCE_FILESYSTEM=1
                     -sALLOW_MEMORY_GROWTH=1
                     -sENVIRONMENT=\"node\"
                     -sTOTAL_MEMORY=1200MB
                     -fdiagnostics-show-template-tree )

add_link_options( --bind
                  -sNO_DISABLE_EXCEPTION_CATCHING ) # Flag must be present in both compile and link options

# Note: Resorted to using -02, rather than -03, due to experience of UB. Follow-up: UB persisted in -02.
if (CMAKE_BUILD_TYPE MATCHES "Release")
    add_compile_definitions( ZLIB_CONST=1
                            #  BOOST_CONTRACT_NO_PRECONDITIONS=1
                            #  BOOST_CONTRACT_NO_POSTCONDITIONS=1
                             KMAP_DEBUG=0
                             KMAP_PROFILE=0
                             KMAP_LOGGING_PATH=0
                             KMAP_LOGGING_DB=0
                             KMAP_LOG_EXCEPTION=0
                             KMAP_TEST_PRE_ENV=1 )
    add_compile_options( -Wall
                         -O2 
                         -sNO_DISABLE_EXCEPTION_CATCHING # Flag must be present in both compile and link options
                         -sWASM=1 )
    add_link_options()
else() # Warning: There is a known but in which a false-positive assert is triggered in DEBUG builds for file operations, so avoid using those in this mode.
    add_compile_definitions( KMAP_DEBUG=1 
                             KMAP_LOGGING_PATH=0 
                             KMAP_LOGGING_DB=0
                             KMAP_TEST_PRE_ENV=1 )
    add_compile_options( -g3 
                         -O1 
                         -sEXCEPTION_DEBUG=1 
                         -sNO_DISABLE_EXCEPTION_CATCHING # Flag must be present in both compile and link options
                         -sDEMANGLE_SUPPORT=1 
                         -sSAFE_HEAP=1 
                         -sASSERTIONS=2 
                         -sSTACK_OVERFLOW_CHECK=2 
                         -sWASM=1
                        #  --profiling 
                        #  --profiling-funcs 
                        #  -finstrument-functions
                        #  -s USE_OFFSET_CONVERTER # Needed by -finstrument-functions
                        #  -sALLOW_MEMORY_GROWTH=1
                        #  -fsanitize=address # Dramatically decreases runtime performance
                        #  -fsanitize=undefined # Even More Dramatically decreases runtime performance
                        #  -sINITIAL_MEMORY=400MB # Needed for -fsanitize=undefined
                         --emit-symbol-map )
add_link_options( -sALLOW_MEMORY_GROWTH=1 
                #   -s USE_OFFSET_CONVERTER # Needed by -finstrument-functions
                  # -fsanitize=address
                  # -fsanitize=undefined
                         -sSAFE_HEAP=1 # Incompatible with -fsanitize=address
                         -sASSERTIONS=2 
                         -sSTACK_OVERFLOW_CHECK=2 
                  -sINITIAL_MEMORY=400MB
                   ) # Flag must be present in both compile and link options
endif()

add_executable( emcc.index.html
                arg.cpp arg.hpp
                attribute.cpp attribute.hpp
                backend/basic_network.cpp backend/basic_network.hpp
                cli/parser.cpp cli/parser.hpp
                # cmd.cpp cmd.hpp
                # cmd/bookmark.cpp
                cmd/canvas.cpp
                cmd/cardinality.cpp
                cmd/command.cpp
                # cmd/conclusion.cpp
                # cmd/contact.cpp
                # cmd/definition.cpp
                # cmd/dotlang.cpp
                cmd/echo.cpp
                # cmd/next_fork.cpp
                cmd/node_manip.cpp
                cmd/parser.cpp
                # cmd/project.cpp
                # cmd/recipe.cpp
                cmd/repair.cpp
                # cmd/resource.cpp
                cmd/script.cpp
                # cmd/search.cpp
                cmd/select_node.cpp
                cmd/task.cpp 
                cmd/test.cpp
                cmd/text_area.cpp
                com/autosave/autosave.cpp com/autosave/autosave.hpp
                com/canvas/canvas.cpp com/canvas/canvas.hpp
                com/chrono/timer.cpp com/chrono/timer.hpp
                com/cli/cli.cpp com/cli/cli.hpp
                com/cmd/cclerk.cpp com/cmd/cclerk.hpp
                com/cmd/command.cpp com/cmd/command.hpp
                com/database/cache.cpp com/database/cache.hpp
                com/database/db.cpp com/database/db.hpp
                com/database/db_fs.cpp com/database/db_fs.hpp
                com/database/root_node.cpp com/database/root_node.hpp
                com/database/sm.cpp com/database/sm.hpp
                com/event/event.cpp com/event/event.hpp
                com/event/event_clerk.cpp com/event/event_clerk.hpp
                com/filesystem/filesystem.cpp com/filesystem/filesystem.hpp
                com/jump_stack/jump_stack.cpp com/jump_stack/jump_stack.hpp
                com/log/log.cpp com/log/log.hpp
                com/log/log_task.cpp
                com/network/alias.cpp com/network/alias.hpp
                com/network/command.cpp com/network/command.hpp
                com/network/network.cpp com/network/network.hpp
                com/network/visnetwork.cpp com/network/visnetwork.hpp
                com/option/option.cpp
                com/option/option_clerk.cpp
                com/tag/tag.cpp com/tag/tag.hpp
                com/task/task.cpp com/task/task.hpp
                com/text_area/text_area.cpp com/text_area/text_area.hpp
                common.cpp common.hpp
                component.cpp component.hpp
                component_store.cpp component_store.hpp
                emcc_bindings.cpp
                error/master.cpp error/master.hpp
                filesystem.cpp filesystem.hpp
                js_iface.cpp
                kmap.cpp kmap.hpp
                lineage.cpp
                path.cpp
                path/act/abs_path.cpp path/act/abs_path.hpp
                path/act/erase.cpp path/act/erase.hpp
                path/act/front.cpp path/act/front.hpp
                path/act/order.cpp path/act/order.hpp
                path/act/take.cpp path/act/take.hpp
                path/act/to_string.cpp path/act/to_string.hpp
                path/act/update_body.cpp path/act/update_body.hpp
                path/node_view.cpp path/node_view.hpp
                path/node_view2.cpp path/node_view2.hpp
                path/sm.cpp
                # stmt_prep.cpp
                test/autosave.cpp
                test/canvas/canvas.cpp
                test/cli/cli.cpp
                # test/cli/parse_raw/conclusion.cpp
                # test/cli/parse_raw/create_child.cpp
                # test/cli/parse_raw/node_manip.cpp
                test/cli/parse_raw/parse_raw.cpp
                # test/cli/parse_raw/project.cpp
                # test/cli/parse_raw/recipe.cpp
                # test/cli/parse_raw/tag.cpp
                test/cli/parser.cpp
                test/database/cache.cpp
                test/database/db.cpp
                test/database/sm.cpp
                test/event/event.cpp
                test/filesystem/filesystem.cpp
                # test/iface/create_alias.cpp
                # test/iface/create_node.cpp
                test/iface/iface.cpp
                # test/iface/jump_stack.cpp
                # test/iface/node_manip.cpp
                # test/iface/select.cpp
                test/iface/travel.cpp
                test/js/js_iface.cpp
                test/key_down/key_down.cpp
                test/master.cpp
                # test/network/visnetwork.cpp
                # test/path/node_view.cpp
                test/path/path.cpp
                test/sqlite/sqlite.cpp
                test/sqlpp/sqlpp.cpp
                test/util.cpp
                test/util/util.cpp
                util/profile.cpp
                util/window.cpp
                utility.cpp
                main.cpp )

set_target_properties( emcc.index.html
                       PROPERTIES
					       OUTPUT_NAME "emcc.index"
                           SUFFIX ".html" )

add_dependencies( emcc.index.html
                  boost
                  catch2
                  date
                  fmtlib
                  openssl 
                  range
                  # xed
				  zlib )

link_directories( ${CMAKE_BINARY_DIR}/lib )

target_link_libraries( emcc.index.html
                      ${CMAKE_BINARY_DIR}/lib/libboost_chrono.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_filesystem.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_system.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_timer.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_unit_test_framework.bc
                      ${CMAKE_BINARY_DIR}/lib/libcrypto.bc
                      ${CMAKE_BINARY_DIR}/lib/libfmt.bc
                      # ${CMAKE_BINARY_DIR}/lib/libxed-ild.bc
                      # ${CMAKE_BINARY_DIR}/lib/libxed.bc
                      ${CMAKE_BINARY_DIR}/lib/libz.bc
                      ${CMAKE_BINARY_DIR}/lib/libsqlite3.bc
                      ${CMAKE_BINARY_DIR}/lib/libCatch2.a
                      nodefs.js )

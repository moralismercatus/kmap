cmake_minimum_required( VERSION 3.0.0 )

project( emcc.index.html )

set( CMAKE_CXX_STANDARD 20 )

add_compile_definitions( ZLIB_CONST=1 )


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
                             BOOST_CONTRACT_NO_PRECONDITIONS=1
                             BOOST_CONTRACT_NO_POSTCONDITIONS=1
                             KMAP_DEBUG=0
                             KMAP_PROFILE=0
                             KMAP_LOGGING_PATH=0
                             KMAP_LOGGING_DB=0
                             KMAP_TEST_PRE_ENV=1 )
    add_compile_options( -Wall
                         -O2 
                         -sNO_DISABLE_EXCEPTION_CATCHING # Flag must be present in both compile and link options
                         -sWASM=1 )
else()
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
                         --profiling 
                         --profiling-funcs 
                         -sALLOW_MEMORY_GROWTH=1
                        #  -fsanitize=address # Dramatically decreases runtime performance
                        #  -fsanitize=undefined # Even More Dramatically decreases runtime performance
                        #  -sINITIAL_MEMORY=400MB # Needed for -fsanitize=undefined
                         --emit-symbol-map )
        # TODO: I was unable to make these work, but they provide additional checks not completely covered by -s checks.
        # -fsanitize=undefined \
        # -fsanitize=address \
add_link_options( -sALLOW_MEMORY_GROWTH=1 
                #   -fsanitize=address
                #   -fsanitize=undefined
                #   -sINITIAL_MEMORY=400MB
                   ) # Flag must be present in both compile and link options
endif()

add_executable( emcc.index.html
                alias.cpp alias.hpp
                arg.cpp arg.hpp
                attribute.cpp attribute.hpp
                backend/basic_network.cpp backend/basic_network.hpp
                canvas.cpp canvas.hpp
                chrono/timer.cpp chrono/timer.hpp
                cli.cpp cli.hpp
                cli/parser.cpp cli/parser.hpp
                cmd.cpp cmd.hpp
                cmd/bookmark.cpp
                cmd/canvas.cpp
                cmd/cardinality.cpp
                cmd/command.cpp
                cmd/conclusion.cpp
                cmd/contact.cpp
                cmd/definition.cpp
                cmd/dotlang.cpp
                cmd/echo.cpp
                cmd/log.cpp
                cmd/next_fork.cpp
                cmd/node_manip.cpp
                cmd/parser.cpp
                cmd/project.cpp
                cmd/recipe.cpp
                cmd/repair.cpp
                cmd/resource.cpp
                cmd/script.cpp
                cmd/search.cpp
                cmd/select_node.cpp
                cmd/state.cpp
                cmd/tag.cpp
                cmd/task.cpp 
                cmd/test.cpp
                cmd/text_area.cpp
                common.cpp common.hpp
                db.cpp db.hpp
                db/autosave.cpp db/autosave.hpp
                db/cache.cpp db/cache.hpp
                db/sm.cpp db/sm.hpp
                emcc_bindings.cpp
                environment.cpp environment.hpp
                event/event.cpp event/event.hpp
                event/event_clerk.cpp event/event_clerk.hpp
                filesystem.cpp filesystem.hpp
                js_iface.cpp
                jump_stack.cpp
                kmap.cpp kmap.hpp
                lineage.cpp
                network.cpp network.hpp
                node_fetcher.cpp
                option/option.cpp
                option/option_clerk.cpp
                path.cpp
                path/act/front.cpp path/act/front.hpp
                path/act/order.cpp path/act/order.hpp
                path/act/to_string.cpp path/act/to_string.hpp
                path/node_view.cpp path/node_view.hpp
                path/sm.cpp
                stmt_prep.cpp
                tag/tag.cpp tag/tag.hpp
                task/task.cpp task/task.hpp
                test/autosave.cpp
                test/canvas/canvas.cpp
                test/cli/cli.cpp
                test/cli/parse_raw/conclusion.cpp
                test/cli/parse_raw/create_child.cpp
                test/cli/parse_raw/node_manip.cpp
                test/cli/parse_raw/parse_raw.cpp
                test/cli/parse_raw/project.cpp
                test/cli/parse_raw/recipe.cpp
                test/cli/parse_raw/tag.cpp
                test/cli/parser.cpp
                test/database/cache.cpp
                test/database/db.cpp
                test/database/sm.cpp
                test/event/event.cpp
                test/filesystem/filesystem.cpp
                test/iface/create_alias.cpp
                test/iface/create_node.cpp
                test/iface/iface.cpp
                test/iface/jump_stack.cpp
                test/iface/node_manip.cpp
                test/iface/select.cpp
                test/iface/travel.cpp
                test/js/js_iface.cpp
                test/key_down/key_down.cpp
                test/master.cpp
                test/network/network.cpp
                test/path/node_view.cpp
                test/path/path.cpp
                test/sqlite/sqlite.cpp
                test/sqlpp/sqlpp.cpp
                test/util/util.cpp
                test/util.cpp
                text_area.cpp
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
                  xed
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
                      ${CMAKE_BINARY_DIR}/lib/libxed-ild.bc
                      ${CMAKE_BINARY_DIR}/lib/libxed.bc
                      ${CMAKE_BINARY_DIR}/lib/libz.bc
                      ${CMAKE_BINARY_DIR}/lib/libsqlite3.bc
                      ${CMAKE_BINARY_DIR}/lib/libCatch2.a
                      nodefs.js )

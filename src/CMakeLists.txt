cmake_minimum_required( VERSION 3.0.0 )

project( emcc.index.html )

set( CMAKE_CXX_STANDARD 20 )
# set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")

add_compile_definitions( ZLIB_CONST=1
                         BOOST_CONTRACT_STATIC_LINK=1
                         BOOST_SML_CFG_DISABLE_MIN_SIZE # See https://github.com/boost-ext/sml/issues/249. Without this defined, UB detected.
                         CATCH_CONFIG_ENABLE_BENCHMARKING=1
                         KMAP_NATIVE=0
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
                     -sTOTAL_MEMORY=2048MB
                     -fdiagnostics-show-template-tree
                   )

add_link_options( --bind
                )
                  # -sNO_DISABLE_EXCEPTION_CATCHING ) # Flag must be present in both compile and link options

# Note: Resorted to using -02, rather than -03, due to experience of UB. Follow-up: UB persisted in -02.
if (CMAKE_BUILD_TYPE MATCHES "Release")
    add_compile_definitions( ZLIB_CONST=1
                             BOOST_CONTRACT_NO_PRECONDITIONS=1
                             BOOST_CONTRACT_NO_POSTCONDITIONS=1
                             BOOST_CONTRACT_DISABLE_THREADS=1
                             NDEBUG=1
                             KMAP_DEBUG=0
                            #  KMAP_EXCEPTIONS_ENABLED=1
                             KMAP_LOGGING_DB=0
                             KMAP_LOGGING_PATH=0
                             KMAP_LOG_EXCEPTION=1
                             KMAP_LOG_KTRY=0
                             KMAP_LOG_KTRYE=1
                             KMAP_PROFILE=0
                             KMAP_TEST_PRE_ENV=1 )
    add_compile_options( -Wall
                         -O2 
                         -sWASM=1
                        #  -sNO_DISABLE_EXCEPTION_CATCHING # Flag must be present in both compile and link options
                        #  -sEVAL_CTORS
                        )
    add_link_options( -sALLOW_MEMORY_GROWTH=1
                      # -00 # Reduce link times
                      # -sERROR_ON_WASM_CHANGES_AFTER_LINK # Reduce link times
                      # -sWASM_BIGINT # Reduce link times 
                      # -sNO_DISABLE_EXCEPTION_CATCHING # Flag must be present in both compile and link options
                      # -sEVAL_CTORS
                      )
else() # Warning: There is a known but in which a false-positive assert is triggered in DEBUG builds for file operations, so avoid using those in this mode.
    add_compile_definitions( KMAP_DEBUG=1 
                             KMAP_EXCEPTIONS_ENABLED=1
                             KMAP_LOGGING_DB=0
                             KMAP_LOGGING_PATH=0 
                             KMAP_LOG_EXCEPTION=1
                             KMAP_LOG_KTRY=0
                             KMAP_LOG_KTRYE=1
                             KMAP_TEST_PRE_ENV=1 )
    add_compile_options( -g3 
                         -O1 
                         -sEXCEPTION_DEBUG=1 
                         -sNO_DISABLE_EXCEPTION_CATCHING # Flag must be present in both compile and link options
                         -sDEMANGLE_SUPPORT=1 
                        #  -sSAFE_HEAP=1 
                        #  -fsanitize=address
                         -fsanitize=undefined
                         -sASSERTIONS=2 
                         -sSTACK_OVERFLOW_CHECK=2 
                        #  -sWASM=1
                         -sWASM=1
                        #  --profiling 
                        #  --profiling-funcs 
                        #  -finstrument-functions
                        #  -s USE_OFFSET_CONVERTER # Needed by -finstrument-functions
                        #  -sALLOW_MEMORY_GROWTH=1
                         --emit-symbol-map )
add_link_options( -sALLOW_MEMORY_GROWTH=1 
                  -sNO_DISABLE_EXCEPTION_CATCHING # Flag must be present in both compile and link options
                #   -s USE_OFFSET_CONVERTER # Needed by -finstrument-functions
                  # -sSAFE_HEAP=1 # Incompatible with -fsanitize=address

                  # -fsanitize=address
                  -fsanitize=undefined 

                  -sSTACK_SIZE=100MB
                  -sASSERTIONS=2 
                  -sSTACK_OVERFLOW_CHECK=2 
                  -sINITIAL_MEMORY=400MB
                   ) # Flag must be present in both compile and link options
endif()

add_executable( emcc.index.html
                attribute.cpp attribute.hpp
                backend/basic_network.cpp backend/basic_network.hpp
                cli/parser.cpp cli/parser.hpp
                cmd/canvas.cpp
                cmd/cardinality.cpp
                cmd/command.cpp
                cmd/js_bind.cpp
                cmd/node_manip.cpp
                cmd/parser.cpp
                cmd/repair.cpp
                cmd/select_node.cpp
                cmd/task.cpp 
                cmd/text_area.cpp
                com/autosave/autosave.cpp com/autosave/autosave.hpp
                com/autosave/js_bind.cpp
                com/breadcrumb/breadcrumb.cpp com/breadcrumb/breadcrumb.hpp
                com/canvas/js_bind.cpp
                com/canvas/canvas.cpp com/canvas/canvas.hpp
                com/canvas/common.cpp com/canvas/common.hpp
                com/canvas/layout.cpp com/canvas/layout.hpp
                com/canvas/pane_clerk.cpp com/canvas/pane_clerk.hpp
                com/canvas/workspace.cpp com/canvas/workspace.hpp
                com/chrono/timer.cpp com/chrono/timer.hpp
                com/cli/cli.cpp com/cli/cli.hpp
                com/cli/js_bind.cpp
                com/cmd/cclerk.cpp com/cmd/cclerk.hpp
                com/cmd/command.cpp com/cmd/command.hpp
                com/cmd/standard_items.cpp
                com/database/cache.cpp com/database/cache.hpp
                com/database/db.cpp com/database/db.hpp
                com/database/filesystem/db_fs.cpp com/database/filesystem/db_fs.hpp
                com/database/filesystem/command.cpp com/database/filesystem/command.hpp
                com/database/js_bind.cpp
                com/database/query_cache.cpp com/database/query_cache.hpp
                com/database/root_node.cpp com/database/root_node.hpp
                com/database/sm.cpp com/database/sm.hpp
                com/database/util.cpp com/database/util.hpp
                com/event/event.cpp com/event/event.hpp
                com/event/event_clerk.cpp com/event/event_clerk.hpp
                com/event/js_bind.cpp
                com/filesystem/filesystem.cpp com/filesystem/filesystem.hpp
                com/filesystem/js_bind.cpp
                com/jump_stack/js_bind.cpp
                com/jump_stack/jump_stack.cpp com/jump_stack/jump_stack.hpp
                com/log/js_bind.cpp
                com/log/log.cpp com/log/log.hpp
                com/log_task/log_task.cpp com/log_task/log_task.hpp
                com/log_task/js_bind.cpp
                com/network/alias.cpp com/network/alias.hpp
                com/network/command.cpp com/network/command.hpp
                com/network/js_bind.cpp
                com/network/network.cpp com/network/network.hpp
                com/option/command.cpp
                com/option/js_bind.cpp
                com/option/option.cpp
                com/option/option_clerk.cpp
                com/tag/js_bind.cpp
                com/tag/tag.cpp com/tag/tag.hpp
                com/task/js_bind.cpp
                com/task/task.cpp com/task/task.hpp
                com/text_area/js_bind.cpp
                com/text_area/text_area.cpp com/text_area/text_area.hpp
                com/visnetwork/option.cpp com/visnetwork/option.hpp
                com/visnetwork/visnetwork.cpp com/visnetwork/visnetwork.hpp
                com/window/event.cpp
                com/window/option.cpp
                common.cpp common.hpp
                component.cpp component.hpp
                component_store.cpp component_store.hpp
                emcc_bindings.cpp
                error/master.cpp error/master.hpp
                filesystem.cpp filesystem.hpp
                js/iface.cpp js/iface.hpp
                js/scoped_code.cpp js/scoped_code.hpp
                kmap.cpp kmap.hpp
                kmap/binding/js/result.cpp kmap/binding/js/result.hpp
                lineage.cpp
                path.cpp
                path/act/abs_path.cpp path/act/abs_path.hpp
                path/act/erase.cpp path/act/erase.hpp
                path/act/fetch_body.cpp path/act/fetch_body.hpp
                path/act/fetch_heading.cpp path/act/fetch_heading.hpp
                path/act/front.cpp path/act/front.hpp
                path/act/order.cpp path/act/order.hpp
                path/act/select_node.cpp path/act/select_node.hpp
                path/act/take.cpp path/act/take.hpp
                path/act/to_string.cpp path/act/to_string.hpp
                path/act/update_body.cpp path/act/update_body.hpp
                path/disambiguate.cpp path/disambiguate.hpp
                path/node_view.cpp path/node_view.hpp
                path/node_view2.cpp path/node_view2.hpp
                path/parser/tokenizer.cpp path/parser/tokenizer.hpp
                path/sm.cpp
                path/view/act/abs_path.cpp path/view/act/abs_path.hpp
                path/view/act/count.cpp path/view/act/count.hpp
                path/view/act/create_node.cpp path/view/act/create_node.hpp
                path/view/act/erase_node.cpp path/view/act/erase_node.hpp
                path/view/act/exists.cpp path/view/act/exists.hpp
                path/view/act/fetch_body.cpp path/view/act/fetch_body.hpp
                path/view/act/fetch_heading.cpp path/view/act/fetch_heading.hpp
                path/view/act/fetch_node.cpp path/view/act/fetch_node.hpp
                path/view/act/fetch_or_create_node.cpp path/view/act/fetch_or_create_node.hpp
                path/view/act/to_fetch_set.cpp path/view/act/to_fetch_set.hpp
                path/view/act/to_heading_set.cpp path/view/act/to_heading_set.hpp
                path/view/act/to_node_set.cpp path/view/act/to_node_set.hpp
                path/view/act/to_node_vec.cpp path/view/act/to_node_vec.hpp
                path/view/act/to_string.cpp path/view/act/to_string.hpp
                path/view/act/update_body.cpp path/view/act/update_body.hpp
                path/view/alias.cpp path/view/alias.hpp
                path/view/all_of.cpp path/view/all_of.hpp
                path/view/ancestor.cpp path/view/ancestor.hpp
                path/view/anchor/abs_root.cpp path/view/anchor/abs_root.hpp
                path/view/anchor/node.cpp path/view/anchor/node.hpp
                path/view/any_of.cpp path/view/any_of.hpp
                path/view/attr.cpp path/view/attr.hpp
                path/view/child.cpp path/view/child.hpp
                path/view/derivation_link.cpp path/view/derivation_link.hpp
                path/view/desc.cpp path/view/desc.hpp
                path/view/direct_desc.cpp path/view/direct_desc.hpp
                path/view/left_lineal.cpp path/view/left_lineal.hpp
                path/view/link.cpp path/view/link.hpp
                path/view/none_of.cpp path/view/none_of.hpp
                path/view/order.cpp path/view/order.hpp
                path/view/parent.cpp path/view/parent.hpp
                path/view/resolve.cpp path/view/resolve.hpp
                path/view/right_lineal.cpp path/view/right_lineal.hpp
                path/view/root.cpp path/view/root.hpp
                path/view/sibling.cpp path/view/sibling.hpp
                path/view/tether.cpp path/view/tether.hpp
                test/autosave.cpp
                test/canvas/canvas.cpp
                test/misc/misc.cpp
                test/database/cache.cpp
                test/database/db.cpp
                test/database/sm.cpp
                test/event/event.cpp
                test/filesystem/filesystem.cpp
                test/iface/iface.cpp
                test/js/js_iface.cpp
                test/master.cpp
                test/path/path.cpp
                test/sqlite/sqlite.cpp
                test/util.cpp
                test/util/util.cpp
                util/clerk/clerk.cpp util/clerk/clerk.hpp
                util/json.cpp util/json.hpp
                util/log/log.cpp util/log/log.hpp
                util/log/xml.cpp util/log/xml.hpp
                util/markdown.cpp util/markdown.hpp
                util/profile.cpp util/profile.hpp
                util/result.cpp util/result.hpp
                util/script/script.cpp util/script/script.hpp
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
                      ${CMAKE_BINARY_DIR}/lib/libboost_contract.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_filesystem.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_json.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_system.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_timer.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_chrono.bc
                      ${CMAKE_BINARY_DIR}/lib/libboost_unit_test_framework.bc
                      ${CMAKE_BINARY_DIR}/lib/libcrypto.bc
                      ${CMAKE_BINARY_DIR}/lib/libfmt.bc
                      # ${CMAKE_BINARY_DIR}/lib/libxed-ild.bc
                      # ${CMAKE_BINARY_DIR}/lib/libxed.bc
                      ${CMAKE_BINARY_DIR}/lib/libz.bc
                      ${CMAKE_BINARY_DIR}/lib/libsqlite3.bc
                      ${CMAKE_BINARY_DIR}/lib/libCatch2.a
                      nodefs.js )

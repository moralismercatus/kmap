cmake_minimum_required( VERSION 3.0.0 )

project( emcc.index.html )

set( CMAKE_CXX_STANDARD 20 )

add_executable( emcc.index.html
                backend/basic_network.cpp
				cli/parser.cpp
				cmd/bookmark.cpp
				cmd/call_map.cpp
				cmd/cardinality.cpp
				cmd/contact.cpp
				cmd/command.cpp
				cmd/conclusion.cpp
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
                cmd/text.cpp
				node_fetcher.cpp
				node_view.cpp
                arg.cpp
                canvas.cpp
                cli.cpp
                cmd.cpp
                common.cpp
                db.cpp
                db_cache.cpp
                emcc_bindings.cpp
                environment.cpp
                filesystem.cpp
                js_iface.cpp
                jump_stack.cpp
                kmap.cpp
                lineage.cpp
                network.cpp
                path.cpp
                path/sm.cpp
                stmt_prep.cpp
                test/cli/cli.cpp
                test/cli/parse_raw/conclusion.cpp
                test/cli/parse_raw/create_child.cpp
                test/cli/parse_raw/node_manip.cpp
                test/cli/parse_raw/parse_raw.cpp
                test/cli/parse_raw/project.cpp
                test/cli/parse_raw/recipe.cpp
                test/cli/parse_raw/tag.cpp
                test/cli/parser.cpp
                test/database/database.cpp
                test/filesystem/filesystem.cpp
                test/iface/create_alias.cpp
                test/iface/create_node.cpp
                test/iface/iface.cpp
                test/iface/jump_stack.cpp
                test/iface/node_manip.cpp
                test/iface/select.cpp
                test/iface/travel.cpp
                test/key_down/key_down.cpp
                test/master.cpp
                test/network/network.cpp
                test/path/path.cpp
                test/sqlite/sqlite.cpp
                test/sqlpp/sqlpp.cpp
                text_view.cpp
                utility.cpp
                main.cpp )
set_target_properties( emcc.index.html
                       PROPERTIES
					       OUTPUT_NAME "emcc.index"
                           SUFFIX ".html" )

# MSYS workaround macro.
if(DEFINED ENV{KMAP_MSYS})
    message(STATUS "KMAP_MSYS environment variable defined")
    list( APPEND CMAKE_CXX_FLAGS "-DKMAP_MSYS=1")
endif()

# Note: With WASM=0, source-mappings are preserved; however, it is considerably slower.
SET( CMAKE_CXX_FLAGS "\
      -Wall \
      -Werror=return-type \
      -Wno-invalid-partial-specialization \
      --bind \
      -s FORCE_FILESYSTEM=1 \
      -s ALLOW_MEMORY_GROWTH=1 \
      -s ENVIRONMENT=\"node\" \
      -s TOTAL_MEMORY=1200MB \
      -DZLIB_CONST=1 \
       ${CMAKE_CXX_FLAGS} \
      "
)
# Note: Resorted to using -02, rather than -03, due to experience of UB. Follow-up: UB persisted in -02.
if (CMAKE_BUILD_TYPE MATCHES "Release")
    SET( CMAKE_CXX_FLAGS "\
        -O2 \
        -s WASM=1 \
        -s DISABLE_EXCEPTION_CATCHING=0 \
        -DBOOST_CONTRACT_NO_PRECONDITIONS=1 \
        -DBOOST_CONTRACT_NO_POSTCONDITIONS=1 \
        -DKMAP_PROFILE=0 \
        -DKMAP_LOGGING_PATH=0 \
        -DKMAP_LOGGING_DB=0 \
        -DKMAP_LOGGING_DB_CACHE=0 \
        ${CMAKE_CXX_FLAGS} \
        "
    )
else()
# Note: Disabling exceptions, as Emscripten doesn't seem to be able to handle them well. Better to terminate and, at least, have an idea of what failed.
    SET( CMAKE_CXX_FLAGS "\
        -g4 \
        -O0 \
        -s DISABLE_EXCEPTION_CATCHING=0 \
        -s DEMANGLE_SUPPORT=1 \
        -s SAFE_HEAP=1 \
        -s ASSERTIONS=2 \
        -s STACK_OVERFLOW_CHECK=2 \
        -s WASM=1 \
        --profiling \
        --profiling-funcs \
        --emit-symbol-map \
        -DKMAP_DEBUG=1 \
        -DKMAP_LOGGING_PATH=0 \
        -DKMAP_LOGGING_DB=0 \
        -DKMAP_LOGGING_DB_CACHE=0 \
        ${CMAKE_CXX_FLAGS} \
        "
    )
        # TODO: I was unable to make these work, but they provide additional checks not completely covered by -s checks.
        # -fsanitize=undefined \
        # -fsanitize=address \
endif()

add_dependencies( emcc.index.html
                  boost
                  date
                  xed
                  fmtlib
                  openssl 
                  range
                  sqlpp11-connector-sqlite3-proj
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
                      ${CMAKE_BINARY_DIR}/lib/libsqlpp11-connector-sqlite3.bc 
                      ${CMAKE_BINARY_DIR}/lib/libxed-ild.bc
                      ${CMAKE_BINARY_DIR}/lib/libxed.bc
                      ${CMAKE_BINARY_DIR}/lib/libz.bc
                      ${CMAKE_BINARY_DIR}/lib/libsqlite3.bc
                      nodefs.js )

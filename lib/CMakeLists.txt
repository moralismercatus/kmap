cmake_minimum_required( VERSION 2.8.7 )

project( kmap-lib )

include( ExternalProject )

message( "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} )

ExternalProject_Add(
    boost

    GIT_REPOSITORY https://github.com/boostorg/boost
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND
        ${CMAKE_BINARY_DIR}/lib/boost-prefix/src/boost/bootstrap.sh
    BUILD_IN_SOURCE 1
    BUILD_COMMAND 
        ${CMAKE_BINARY_DIR}/lib/boost-prefix/src/boost/b2 
            headers
     && ${CMAKE_BINARY_DIR}/lib/boost-prefix/src/boost/b2 
            toolset=emscripten
			target-os=linux # Even on Windows, use MSYS and mingw32-make.exe.
            link=static
            threading=single
             --stagedir=${CMAKE_BINARY_DIR}
            --with-chrono
            --with-filesystem
            --with-system
            --with-test
            --with-timer
            define=BOOST_TEST_ALTERNATIVE_INIT_API
            define=BOOST_TEST_NO_MAIN
            define=BOOST_TEST_DISABLE_ALT_STACK
    INSTALL_COMMAND ""
)

ExternalProject_Add(
    range

    GIT_REPOSITORY https://github.com/ericniebler/range-v3.git
    GIT_SUBMODULES ""
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

ExternalProject_Add(
    fmtlib

    GIT_REPOSITORY
        https://github.com/fmtlib/fmt.git
    CMAKE_ARGS
        -DBUILD_SHARED_LIBS=FALSE
        -DFMT_TEST=FALSE
        -DCMAKE_CXX_COMPILER=emcc
        -DCMAKE_CXX_COMPILER_WORKS=1
        -DCMAKE_CXX_FLAGS="-std=c++14"
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    UPDATE_COMMAND ""
    INSTALL_COMMAND emranlib libfmt.a
                 && ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/lib/fmtlib-prefix/src/fmtlib-build/libfmt.a ${CMAKE_BINARY_DIR}/lib/libfmt.bc
                 && ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/lib/fmtlib-prefix/src/fmtlib/include/fmt ${CMAKE_BINARY_DIR}/include/fmt
)

ExternalProject_Add(
    date

    GIT_REPOSITORY
        https://github.com/HowardHinnant/date.git
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
)

ExternalProject_Add(
    sqlite3

    GIT_REPOSITORY
        https://github.com/sqlite/sqlite.git
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND
        ./configure --enable-static --enable-releasemode
     && make keywordhash.h lemon mksourceid -j   
    BUILD_COMMAND 
        emconfigure ./configure --enable-static --enable-releasemode
     && make -j
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND 
        emranlib ${CMAKE_BINARY_DIR}/lib/sqlite3-prefix/src/sqlite3/.libs/libsqlite3.a
     && ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/lib/sqlite3-prefix/src/sqlite3/.libs/libsqlite3.a
                                           ${CMAKE_BINARY_DIR}/lib/libsqlite3.bc
)

ExternalProject_Add(
    sqlpp11

    GIT_REPOSITORY
        https://github.com/rbock/sqlpp11.git
    CMAKE_ARGS
        -DHinnantDate_ROOT_DIR=${CMAKE_BINARY_DIR}/lib/date-prefix/src/date/include
        -DENABLE_TESTS=FALSE
        -DCMAKE_CXX_COMPILER=emcc
        -DCMAKE_CXX_COMPILER_WORKS=1
        -DCMAKE_CXX_FLAGS="-std=c++14"
        -DCMAKE_C_COMPILER=emcc
        -DCMAKE_C_COMPILER_WORKS=1
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    GIT_TAG
        develop
    PATCH_COMMAND
        git apply ${CMAKE_SOURCE_DIR}/patch/sqlpp11.patch
    UPDATE_COMMAND ""
)

add_dependencies( sqlpp11
                  boost
                  date )

ExternalProject_Add(
    sqlpp11-connector-sqlite3-proj

    GIT_REPOSITORY
        https://github.com/rbock/sqlpp11-connector-sqlite3.git
    CMAKE_ARGS
        -DSQLPP11_INCLUDE_DIR=${CMAKE_BINARY_DIR}/lib/sqlpp11-prefix/src/sqlpp11/include
        -DHinnantDate_ROOT_DIR=${CMAKE_BINARY_DIR}/lib/date-prefix/src/date/include
        -DSQLite3_LIBRARY=/usr/lib/x86_64-linux-gnu/libsqlite3.so.0 # Only used to bypass config check. Real defs will be in LLVM BC.
        -DSQLite3_INCLUDE_DIR=${CMAKE_BINARY_DIR}/lib/sqlite3-prefix/src/sqlite3
        -DENABLE_TESTS=Off
        -DCMAKE_CXX_COMPILER=emcc
        -DCMAKE_CXX_FLAGS="-std=c++17"
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    GIT_TAG
        master
    UPDATE_COMMAND ""
    INSTALL_COMMAND emranlib src/libsqlpp11-connector-sqlite3.a
                 && ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/lib/sqlpp11-connector-sqlite3-proj-prefix/src/sqlpp11-connector-sqlite3-proj-build/src/libsqlpp11-connector-sqlite3.a ${CMAKE_BINARY_DIR}/lib/libsqlpp11-connector-sqlite3.bc
                 && ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/lib/sqlpp11-connector-sqlite3-proj-prefix/src/sqlpp11-connector-sqlite3-proj/include/sqlpp11/sqlite3 ${CMAKE_BINARY_DIR}/include/sqlpp11/sqlite3
)

add_dependencies( sqlpp11-connector-sqlite3-proj
                  sqlpp11
                  date
                  sqlite3 )

ExternalProject_Add(
    sml

    GIT_REPOSITORY
        https://github.com/boost-experimental/sml.git
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
)

ExternalProject_Add(
    openssl 

    GIT_REPOSITORY
        https://github.com/openssl/openssl.git
    GIT_SUBMODULES ""
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND
        CC=emcc CXX=emcc perl ./Configure -no-asm -no-shared -no-hw -no-pic no-threads -D__STDC_NO_ATOMICS__ cc 
    BUILD_IN_SOURCE 1
    BUILD_COMMAND 
        CC=emcc CXX=emcc make build_generated libcrypto.a -j16
    INSTALL_COMMAND
		${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/lib/openssl-prefix/src/openssl/libcrypto.a
            ${CMAKE_BINARY_DIR}/lib/libcrypto.bc
     && emranlib ${CMAKE_BINARY_DIR}/lib/libcrypto.bc
)

ExternalProject_Add(
    zlib

    GIT_REPOSITORY
        https://github.com/madler/zlib.git
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND
		CC=emcc CXX=emcc ./configure
    BUILD_COMMAND 
        make libz.a -j8
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND
		${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/lib/zlib-prefix/src/zlib/libz.a
            ${CMAKE_BINARY_DIR}/lib/libz.bc
     && emranlib ${CMAKE_BINARY_DIR}/lib/libz.bc
)

ExternalProject_Add(
    dry_comparisons 

    GIT_REPOSITORY https://github.com/rollbear/dry-comparisons.git
    GIT_TAG drier
    GIT_SUBMODULES ""
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

ExternalProject_Add(
    xed 

    BUILD_IN_SOURCE 1
    GIT_REPOSITORY
        https://github.com/intelxed/xed.git 
    CONFIGURE_COMMAND 
        git clone https://github.com/intelxed/mbuild.git ../mbuild
    BUILD_COMMAND 
        python3 mfile.py --compiler=clang --cc=emcc --cxx=emcc --static --host-cpu=ia32 # wasm doesn't support x64 yet
    UPDATE_COMMAND ""
    INSTALL_COMMAND 
		${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/lib/xed-prefix/src/xed/obj/libxed.a
            ${CMAKE_BINARY_DIR}/lib/libxed.bc
     && emranlib ${CMAKE_BINARY_DIR}/lib/libxed.bc
	 && ${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/lib/xed-prefix/src/xed/obj/libxed-ild.a
            ${CMAKE_BINARY_DIR}/lib/libxed-ild.bc
     && emranlib ${CMAKE_BINARY_DIR}/lib/libxed-ild.bc
)

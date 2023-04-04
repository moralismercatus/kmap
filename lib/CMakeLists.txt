cmake_minimum_required( VERSION 2.8.7 )

project( kmap-lib )

include( ExternalProject )

message( "CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER} )

ExternalProject_Add(
    boost

    GIT_REPOSITORY https://github.com/boostorg/boost
    UPDATE_COMMAND ""
    GIT_TAG
        boost-1.81.0
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
            cxxflags=-O2
             --stagedir=${CMAKE_BINARY_DIR}
            --with-chrono
            --with-contract
            --with-filesystem
            --with-json
            --with-system
            --with-test
            --with-timer
            define=BOOST_TEST_ALTERNATIVE_INIT_API
            define=BOOST_TEST_NO_MAIN
            define=BOOST_TEST_DISABLE_ALT_STACK
            define=BOOST_CONTRACT_DISABLE_THREADS
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
    GIT_TAG
        8.1.1
    CMAKE_ARGS
        -DBUILD_SHARED_LIBS=FALSE
        -DFMT_TEST=FALSE
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_COMPILER_WORKS=1
        -DCMAKE_CXX_FLAGS="-std=c++14 -O3"
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
    magic_enum

    GIT_REPOSITORY
        https://github.com/Neargye/magic_enum.git
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_COMPILER_WORKS=1
        -DMAGIC_ENUM_OPT_BUILD_TESTS=OFF
        -DMAGIC_ENUM_OPT_BUILD_EXAMPLES=OFF
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
)

ExternalProject_Add(
    sqlite3

    GIT_REPOSITORY
        https://github.com/sqlite/sqlite.git
    GIT_TAG
        961a72601bfb981f347d4860e65515e7ab5b7eba
    PATCH_COMMAND
        git apply ${CMAKE_SOURCE_DIR}/patch/sqlite3.patch
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND
        ./configure --enable-static --enable-releasemode
     && make keywordhash.h lemon mksourceid -j   
    BUILD_COMMAND 
        emconfigure ./configure --enable-static --enable-releasemode
        "CFLAGS=-s ERROR_ON_UNDEFINED_SYMBOLS=0"
     && make libsqlite3.la -j
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
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_COMPILER_WORKS=1
        -DCMAKE_CXX_FLAGS="-std=c++14"
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_C_COMPILER_WORKS=1
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    GIT_TAG
        main
    # PATCH_COMMAND
    #     git apply ${CMAKE_SOURCE_DIR}/patch/sqlpp11.patch
    UPDATE_COMMAND ""
)

add_dependencies( sqlpp11
                  boost
                  date )

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
    cryptopp

    GIT_REPOSITORY
        https://github.com/weidai11/cryptopp.git
    GIT_SUBMODULES ""
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE 1
    BUILD_COMMAND 
        CC=emcc CXX=emcc make libcryptopp.a -j16
    INSTALL_COMMAND
		${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/lib/cryptopp-prefix/src/cryptopp/libcryptopp.a
            ${CMAKE_BINARY_DIR}/lib/libcryptopp.bc
     && emranlib ${CMAKE_BINARY_DIR}/lib/libcryptopp.bc
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
    GIT_TAG main 
    GIT_SUBMODULES ""
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

# TODO: Remove. The decoding that XED provides should be part of a plugin, not core KMAP.
# ExternalProject_Add(
#     xed 

#     BUILD_IN_SOURCE 1
#     GIT_REPOSITORY
#         https://github.com/intelxed/xed.git 
#     GIT_TAG
#         main
#     CONFIGURE_COMMAND 
#         git clone https://github.com/intelxed/mbuild.git ../mbuild
#     BUILD_COMMAND 
#         python3 mfile.py --compiler=clang --cc=emcc --cxx=emcc --static --host-cpu=ia32 # wasm doesn't support x64 yet
#     UPDATE_COMMAND ""
#     INSTALL_COMMAND 
# 		${CMAKE_COMMAND} -E copy
#             ${CMAKE_BINARY_DIR}/lib/xed-prefix/src/xed/obj/libxed.a
#             ${CMAKE_BINARY_DIR}/lib/libxed.bc
#      && emranlib ${CMAKE_BINARY_DIR}/lib/libxed.bc
# 	 && ${CMAKE_COMMAND} -E copy
#             ${CMAKE_BINARY_DIR}/lib/xed-prefix/src/xed/obj/libxed-ild.a
#             ${CMAKE_BINARY_DIR}/lib/libxed-ild.bc
#      && emranlib ${CMAKE_BINARY_DIR}/lib/libxed-ild.bc
# )

ExternalProject_Add(
    catch2

    GIT_REPOSITORY
        https://github.com/catchorg/Catch2.git
    GIT_TAG
        v3.0.0-preview5
    UPDATE_COMMAND ""
    CMAKE_ARGS
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
)
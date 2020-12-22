cmake_minimum_required( VERSION 2.8.7 )

project( kmap NONE )

add_definitions( -D_BOOST_ENABLE_ASSERT_HANDLER )

set( default_build_type "Debug" )
set( CMAKE_SYSTEM_NAME Generic )
set( CMAKE_C_COMPILER_WORKS 1 )
set( CMAKE_CXX_COMPILER_WORKS 1 )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )

file( MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include )
file( MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/boost )
include_directories( ${CMAKE_BINARY_DIR}/include )
include_directories( ${CMAKE_BINARY_DIR}/lib/boost-prefix/src/boost )
include_directories( ${CMAKE_BINARY_DIR}/lib/date-prefix/src/date/include )
include_directories( ${CMAKE_BINARY_DIR}/lib/dry_comparisons-prefix/src/dry_comparisons )
include_directories( ${CMAKE_BINARY_DIR}/lib/openssl-prefix/src/openssl/include )
include_directories( ${CMAKE_BINARY_DIR}/lib/range-prefix/src/range/include )
include_directories( ${CMAKE_BINARY_DIR}/lib/sml-prefix/src/sml/include )
include_directories( ${CMAKE_BINARY_DIR}/lib/sqlite3-prefix/src/sqlite3 )
include_directories( ${CMAKE_BINARY_DIR}/lib/xed-prefix/src/xed/obj/wkit/include )
include_directories( ${CMAKE_BINARY_DIR}/lib/zlib-prefix/src/zlib )

add_subdirectory( lib )
add_subdirectory( src )

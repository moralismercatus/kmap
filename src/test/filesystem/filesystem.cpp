/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "../master.hpp"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include <sys/stat.h>

namespace fs = boost::filesystem;
namespace utf = boost::unit_test;

namespace kmap::test {

namespace {

auto const filesystem_file_path = "/kmap/fs_is_working.txt";

auto utf_filesystem_ctor()
    -> void
{
}
auto utf_filesystem_dtor()
    -> void
{
    if( file_exists( filesystem_file_path ) )
    {
        fs::remove( filesystem_file_path );
    }
}

} // namespace anon

/******************************************************************************/
BOOST_AUTO_TEST_SUITE( filesystem 
                     , 
                     * utf::label( "env" )
                     * utf::fixture( &utf_filesystem_ctor, &utf_filesystem_dtor ) )

BOOST_AUTO_TEST_CASE( write_file )
{
    if( file_exists( filesystem_file_path ) )
    {
        BOOST_TEST( fs::remove( filesystem_file_path ) );
    }

    std::ofstream ofs{ filesystem_file_path };

    BOOST_TEST( ofs.good() );

    ofs << "Yep, 'tis verking." << std::endl;
    
    BOOST_TEST( ofs.good() );
}

BOOST_AUTO_TEST_CASE( stat
                    ,
                    * utf::depends_on( "filesystem/write_file" ) )
{
    struct ::stat stat_buf;
    auto const rv = ::stat( filesystem_file_path
                          , &stat_buf );

    BOOST_TEST( rv == 0 );
}

BOOST_AUTO_TEST_SUITE_END( /* filesystem */ )

} // namespace kmap::test
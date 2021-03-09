/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TEST_MASTER_HPP
#define KMAP_TEST_MASTER_HPP

#include "../kmap.hpp"
#include "../cli.hpp"

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

namespace kmap {

class Kmap;

auto run_unit_tests( /*std::vector< std::string > const& args*/ )
    -> int;

namespace test {

// Use this fixture when completely resetting Kmap is required. For example, when Kmap fundamentals are being tested, or to ensure Kmap is fully reset.
struct ResetInstanceFixture
{
    ResetInstanceFixture();
    ~ResetInstanceFixture();
};

// Use this fixture when Kmap fundamentals are considered reliable.
// As each piece of data is tied to a node, it should be safe to assume that deleting all nodes (except root) is equivalent to
// reseting the map without having to decomission the DB. Speeds up testing, while also testing the delete function.
struct ClearMapFixture
{
    ClearMapFixture();
    ~ClearMapFixture();
};

template< typename... T >
[[ maybe_unused ]]
auto create_lineages( T&&... args )
    -> std::map< Heading, Uuid >
{
    auto rv = std::map< Heading, Uuid >{};
    auto const paths = std::vector< std::string >{ std::forward< T >( args )... };
    auto& kmap = Singleton::instance();

    for( auto const& path : paths )
    {
        for( auto const desc = kmap.create_descendants( path ).value()
           ; auto const& node : desc )
        {
            rv.emplace( kmap.absolute_path_flat( node ), node );
        }
    }

    return rv;
}

auto select_each_descendant_test( Kmap& kmap
                                , Uuid const& node )
    -> bool;

} // namespace test

} // namespace kmap

#endif // KMAP_TEST_MASTER_HPP

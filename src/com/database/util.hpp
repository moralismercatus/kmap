/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_UTIL_HPP
#define KMAP_DB_UTIL_HPP

#include "common.hpp"

#include <sqlpp11/sqlite3/connection.h>

#include <set>
#include <string>

namespace kmap::com::db {

auto make_connection_config( FsPath const& db_path
                           , int flags
                           , bool debug )
    -> sqlpp::sqlite3::connection_config;
auto open_connection( FsPath const& db_path
                    , int flags
                    , bool debug )
    -> sqlpp::sqlite3::connection;
auto select_aliases( sqlpp::sqlite3::connection& con
                   , std::string const& node )
    -> Result< std::set< std::string > >;
auto select_attribute_root( sqlpp::sqlite3::connection& con
                          , std::string const& node )
    -> Result< std::string >;
auto select_attributes( sqlpp::sqlite3::connection& con
                      , std::string const& node )
    -> Result< std::set< std::string > >;
auto select_attr_parent( sqlpp::sqlite3::connection& con
                       , std::string const& node )
    -> Result< std::string >;
auto select_body( sqlpp::sqlite3::connection& con
                , std::string const& node )
    -> Result< std::string >;
auto select_children( sqlpp::sqlite3::connection& con
                    , std::string const& node )
    -> Result< std::set< std::string > >;
auto select_heading( sqlpp::sqlite3::connection& con
                   , std::string const& node )
    -> Result< std::string >;
auto select_parent( sqlpp::sqlite3::connection& con
                  , std::string const& node )
    -> Result< std::string >;

} // kmap::com::db

#endif // KMAP_DB_UTIL_HPP

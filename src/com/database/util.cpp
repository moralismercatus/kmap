/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/database/util.hpp>

#include <com/database/table_decl.hpp>
#include <util/result.hpp>

#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/sqlite3/connection.h>
#include <sqlpp11/connection.h>

namespace kmap::com::db {

namespace {

auto dbErrorLogCallback( void* pArg
                       , int iErrCode
                       , char const* zMsg )
    -> void
{
    fmt::print( stderr
              , "{}: {}\n"
              , iErrCode
              , zMsg );
}

} // namespace anon

/**
 * Note:
 *   - Use of "unix-dotfile" as a locking mechanism is done because some platforms/VFSs aren't supporting the default locking mechanism for SQLITE.
 *     Requires file{}?vfs=unix-dotfile in the path and SQLITE_OPEN_URI in the flags.
 */
auto make_connection_config( FsPath const& db_path
                           , int flags
                           , bool debug )
    -> sqlpp::sqlite3::connection_config
{
    auto cfg = sqlpp::sqlite3::connection_config{};

    cfg.path_to_database = fmt::format( "file:{}?vfs=unix-dotfile", db_path.string() ); 
    cfg.flags = flags | SQLITE_OPEN_URI;

    if( debug )
    {
        cfg.debug = true;

        sqlite3_config( SQLITE_CONFIG_LOG
                      , dbErrorLogCallback
                      , NULL );
    }

    return cfg;
}

auto open_connection( FsPath const& db_path
                    , int flags
                    , bool debug )
    -> sqlpp::sqlite3::connection
{
    auto con = sqlpp::sqlite3::connection{ make_connection_config( db_path, flags, debug ) };

    // Sqlite3 disables extended error reporting by default. Enable it.
    // It seems to only not be provided by default b/c of legacy support. I don't foresee any negatives,
    // and the positives are better error reporting, so always enable - unless good reason not to.
    sqlite3_extended_result_codes( con.native_handle(), 1 );

    return con;
}

auto select_aliases( sqlpp::sqlite3::connection& con
                   , std::string const& node )
    -> Result< std::set< std::string > >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::set< std::string > >();
    auto rs = decltype( rv )::value_type{};
    auto tbl = aliases::aliases{};

    for( auto rows = con( select( all_of( tbl ) ).from( tbl ).where( tbl.dst_uuid == node ) )
       ; auto const& row : rows )
    {
        rs.emplace( row.src_uuid );
    }

    rv = rs;

    return rv;
}

auto select_attribute_root( sqlpp::sqlite3::connection& con
                          , std::string const& node )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::string >();
    auto rs = std::set< std::string >{};
    auto att = attributes::attributes{};

    for( auto rows = con( select( all_of( att ) ).from( att ).where( att.parent_uuid == node ) )
       ; auto const& row : rows )
    {
        rs.emplace( row.child_uuid );
    }

    if( rs.size() == 1 )
    {
        rv = *rs.begin();
    }

    return rv;
}

auto select_attributes( sqlpp::sqlite3::connection& con
                      , std::string const& node )
    -> Result< std::set< std::string > >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::set< std::string > >();
    auto rs = decltype( rv )::value_type{};
    auto ct = children::children{};
    auto const attr_root = KTRY( select_attribute_root( con, node ) );

    for( auto rows = con( select( all_of( ct ) ).from( ct ).where( ct.parent_uuid == attr_root ) )
       ; auto const& row : rows )
    {
        rs.emplace( row.child_uuid );
    }

    rv = rs;

    return rv;
}

auto select_body( sqlpp::sqlite3::connection& con
                , std::string const& node )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::string >();
    auto rs = std::set< decltype( rv )::value_type >{};
    auto tbl = bodies::bodies{};

    for( auto rows = con( select( all_of( tbl ) ).from( tbl ).where( tbl.uuid == node ) )
       ; auto const& row : rows )
    {
        rs.emplace( row.body );
    }

    if( rs.size() == 1 )
    {
        rv = *rs.begin();
    }

    return rv;
}

auto select_children( sqlpp::sqlite3::connection& con
                    , std::string const& node )
    -> Result< std::set< std::string > >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::set< std::string > >();
    auto rs = decltype( rv )::value_type{};
    auto ct = children::children{};

    for( auto rows = con( select( all_of( ct ) ).from( ct ).where( ct.parent_uuid == node ) )
       ; auto const& row : rows )
    {
        rs.emplace( row.child_uuid );
    }

    rv = rs;

    return rv;
}

auto select_heading( sqlpp::sqlite3::connection& con
                   , std::string const& node )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::string >();
    auto rs = std::set< decltype( rv )::value_type >{};
    auto tbl = headings::headings{};

    for( auto rows = con( select( all_of( tbl ) ).from( tbl ).where( tbl.uuid == node ) )
       ; auto const& row : rows )
    {
        rs.emplace( row.heading );
    }

    if( rs.size() == 1 )
    {
        rv = *rs.begin();
    }

    return rv;
}

auto select_parent( sqlpp::sqlite3::connection& con
                  , std::string const& node )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::string >();
    auto rs = std::set< decltype( rv )::value_type >{};
    auto ct = children::children{};

    for( auto rows = con( select( all_of( ct ) ).from( ct ).where( ct.child_uuid == node ) )
       ; auto const& row : rows )
    {
        rs.emplace( row.parent_uuid );
    }

    if( rs.size() == 1 )
    {
        rv = *rs.begin();
    }

    return rv;
}

auto select_attr_parent( sqlpp::sqlite3::connection& con
                       , std::string const& node )
    -> Result< std::string >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< std::string >();
    auto rs = std::set< decltype( rv )::value_type >{};
    auto tbl = attributes::attributes{};

    for( auto rows = con( select( all_of( tbl ) ).from( tbl ).where( tbl.child_uuid == node ) )
       ; auto const& row : rows )
    {
        rs.emplace( row.parent_uuid );
    }

    if( rs.size() == 1 )
    {
        rv = *rs.begin();
    }

    return rv;
}

} // namespace kmap::com::db

/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_DB_TABLE_DECL_HPP
#define KMAP_DB_TABLE_DECL_HPP

#include <sqlpp11/sqlpp11.h>
#include <sqlpp11/ppgen.h>

#include <map>
#include <string>

SQLPP_DECLARE_TABLE
(
    ( nodes )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( children )
    ,
    ( parent_uuid, text, SQLPP_NOT_NULL  )
    ( child_uuid, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( headings )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( heading, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( titles )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( title, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( bodies )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( body, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( aliases )
    ,
    ( src_uuid, text, SQLPP_NOT_NULL  )
    ( dst_uuid, text, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( resources )
    ,
    ( uuid, text, SQLPP_NOT_NULL  )
    ( resource, blob, SQLPP_NOT_NULL  )
)

SQLPP_DECLARE_TABLE
(
    ( attributes )
    ,
    ( parent_uuid, text, SQLPP_NOT_NULL  )
    ( child_uuid, text, SQLPP_NOT_NULL  )
)

namespace kmap::com::db {

inline
auto const table_map = std::map< std::string, std::string >
{
    { "nodes", 
R"(CREATE TABLE IF NOT EXISTS nodes
(   uuid
,   PRIMARY KEY( uuid )
);)" }
,   { "children",
R"(CREATE TABLE IF NOT EXISTS children
(   parent_uuid TEXT
,   child_uuid TEXT
,   PRIMARY KEY( parent_uuid
               , child_uuid )
);)" }
,   { "headings",
R"(CREATE TABLE IF NOT EXISTS headings
(   uuid TEXT
,   heading TEXT
,   PRIMARY KEY( uuid )
);)" }
,   { "titles",
R"(
CREATE TABLE IF NOT EXISTS titles
(   uuid TEXT
,   title TEXT
,   PRIMARY KEY( uuid )
);)" }
,   { "bodies", 
R"(
CREATE TABLE IF NOT EXISTS bodies
(   uuid TEXT
,   body TEXT
,   PRIMARY KEY( uuid )
);)" }
,   { "aliases",
R"(CREATE TABLE IF NOT EXISTS aliases
(   src_uuid TEXT
,   dst_uuid TEXT
,   PRIMARY KEY( src_uuid
               , dst_uuid )
);)" }
,   { "attributes",
R"(CREATE TABLE IF NOT EXISTS attributes
(   parent_uuid TEXT
,   child_uuid TEXT
,   PRIMARY KEY( parent_uuid
               , child_uuid )
);)" }
,   { "resources",
R"(CREATE TABLE IF NOT EXISTS resources
(   uuid TEXT
,   resource BLOB
,   PRIMARY KEY( uuid )
);)" }
};

} // kmap::com::db

#endif // KMAP_DB_TABLE_DECL_HPP

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

#endif // KMAP_DB_TABLE_DECL_HPP

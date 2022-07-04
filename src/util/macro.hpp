/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_UTIL_MACRO_HPP
#define KMAP_UTIL_MACRO_HPP

#define KMAP_CONCAT_( lhs, rhs ) lhs ## rhs
#define KMAP_CONCAT( lhs, rhs ) KMAP_CONCAT_( lhs, rhs )
#define KMAP_UNIQUE_NAME( prefix ) KMAP_CONCAT( prefix, __COUNTER__ )

#endif // KMAP_UTIL_MACRO_HPP
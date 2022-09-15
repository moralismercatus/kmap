/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#ifndef KMAP_CONTRACT_HPP
#define KMAP_CONTRACT_HPP

#include "io.hpp" // Must precede inclusion of dependents e.g., optional_io.hpp b/c of ADL.

#include <boost/contract.hpp>
#include <boost/contract_macro.hpp>
#include <boost/test/test_tools.hpp>

#define BC_CONTRACT( ... ) BOOST_CONTRACT_FUNCTION( __VA_ARGS__ )
#define BC_PRE( ... ) BOOST_CONTRACT_PRECONDITION( __VA_ARGS__ )
#define BC_POST( ... ) BOOST_CONTRACT_POSTCONDITION( __VA_ARGS__ )
#define BC_OLDOF( ... ) BOOST_CONTRACT_OLDOF( __VA_ARGS__ )
// #define BC_ASSERT( ... ) BOOST_CONTRACT_ASSERT( __VA_ARGS__ )
// TODO: Figure out why BOOST_CONTRACT_ASSERT is malfuctioning with emcc. Using regular assert for the time being.
// #define BC_ASSERT( ... ) assert( __VA_ARGS__ )
#define BC_ASSERT( x ) ((void)((x) || (__assert_fail(#x, __FILE__, __LINE__, __func__),0))) // Applicable for both debug and ndebug.
// Note: Using BOOST_TEST_REQUIRE, as this seems to be the only kind of assertion capable of printing details about the types involved,
//       at the expensive of having to provide ostream overloads for the respective types.
// Note: Enabling/disabling requires different macros than those associated with std assert.
// Note: This will also mean running unit tests will inflate the number of "tests", if contract tests and assertions are included in the count. 
// Note: Added extra () around args, as Boost.Test _intentionally disables logical operators in TEST() statements. For whatever reason, the recommendation is to surround with parentheses.
// #define BC_ASSERT( ... ) BOOST_TEST_REQUIRE( ( __VA_ARGS__ ) ) // This doesn't seem to work outside of a Boost test case.
#define KMAP_ASSERT_EQ( lhs, rhs ) { if( ( lhs ) != ( rhs ) ){ fmt::print( stderr, "{} != {}\n", ( lhs ), ( rhs ) ); assert( ( lhs ) == ( rhs ) ); } }
#define KMAP_ASSERT_P( p ) { if( ( p ) ){ fmt::print( stderr, "{}\n", ( p ) ); assert( ( p ) ); } }

namespace kmap {

// Assumes terminate handler is properly configured.
inline
auto configure_contract_failure_handlers()
    -> void
{
    using boost::contract::set_precondition_failure;
    using boost::contract::set_postcondition_failure;
    using boost::contract::set_invariant_failure;
    using boost::contract::set_old_failure;
    using boost::contract::from;
    using boost::contract::from_destructor;

    set_precondition_failure(
    set_postcondition_failure(
    set_invariant_failure(
    set_old_failure( []( from where )
    {
        if( where == from_destructor )
        {
            fmt::print( stderr
                      , "Ignoring destructor contract failure\n" );
        }
        else
        {
            // An exception is pending here.
            // The terminate handler must consume it.
            std::terminate();
        }
    } ) ) ) );
}

} // namespace kmap

#endif // KMAP_CONTRACT_HPP

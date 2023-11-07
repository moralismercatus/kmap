/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <js/scoped_code.hpp>

#include <contract.hpp>
#include <error/js_iface.hpp>
#include <test/util.hpp>
#include <util/result.hpp>
#include <utility.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#endif // !KMAP_NATIVE

#include <catch2/catch_test_macros.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/join.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace ranges;

namespace kmap::js {

ScopedCode::ScopedCode( std::string const& ctor
                      , std::string const& dtor )
    : ctor_code{ ctor }
    , dtor_code{ dtor }
{
    KM_RESULT_PROLOG();
        KM_RESULT_PUSH( "ctor", ctor );
        KM_RESULT_PUSH( "dtor", dtor );

    if( !ctor_code.empty() )
    {
        #if !KMAP_NATIVE
        auto const pp = KTRYE( js::preprocess( ctor_code ) );

        KTRYE( eval_void( pp ) );
        #endif // !KMAP_NATIVE
    }
}

ScopedCode::ScopedCode( ScopedCode&& other )
    : ctor_code{ std::move( other.ctor_code ) }
    , dtor_code{ std::move( other.dtor_code ) }
{
    other.ctor_code = {};
    other.dtor_code = {};
}

ScopedCode::~ScopedCode()
{
    KM_RESULT_PROLOG();

    try
    {
        if( !dtor_code.empty() )
        {
            #if !KMAP_NATIVE
            fmt::print( "scoped dtor, evaling: {}\n", dtor_code );
            auto const pp = KTRYE( js::preprocess( dtor_code ) );

            KTRYE( eval_void( pp ) );
            #endif // !KMAP_NATIVE
        }
    }
    catch( std::exception const& e )
    {
        std::cerr << e.what() << '\n';
        std::terminate();
    }
}

auto ScopedCode::operator=( ScopedCode&& other )
    -> ScopedCode&
{
    ctor_code = std::move( other.ctor_code );
    dtor_code = std::move( other.dtor_code );

    other.ctor_code = {};
    other.dtor_code = {};

    return *this;
}

} // namespace kmap::js

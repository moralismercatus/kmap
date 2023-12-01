/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <path/view/disambiguate.hpp>

#include <common.hpp>
#include <path/disambiguate.hpp>

#include <compare>

namespace rvs = ranges::views;

namespace kmap::view2 {

auto Disambiguate::create( CreateContext const& ctx
                  , Uuid const& root ) const
    -> Result< UuidSet >
{
    // This should never be called. Creating a disambiguation is incoherent.

    KM_RESULT_PROLOG();
    
    auto rv = result::make_result< UuidSet >();

    return rv;
}

auto Disambiguate::fetch( FetchContext const& ctx
                        , Uuid const& node ) const
    -> Result< FetchSet >
{
    KM_RESULT_PROLOG();

    auto rv = result::make_result< FetchSet >();
    auto const dis = 
    rv = KTRY( disambiguate_path3( ctx.km, node ) );

    return rv;
}

auto Disambiguate::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Disambiguate >();
}

auto Disambiguate::to_string() const
    -> std::string
{
    return "disambiguate";
}

auto Disambiguate::compare_less( Link const& other ) const
    -> bool
{ 
    auto const cmp = compare_links( *this, other );

    if( cmp == std::strong_ordering::equal )
    {
        return pred_ < dynamic_cast< decltype( *this )& >( other ).pred_;
    }
    else if( cmp == std::strong_ordering::less )
    {
        return true;
    }
    else
    {
        return false;
    }
}

} // namespace kmap::view2
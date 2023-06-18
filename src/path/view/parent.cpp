/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/view/parent.hpp"

#include "com/network/network.hpp"
#include "kmap.hpp"
#include "utility.hpp"

#include <catch2/catch_test_macros.hpp>

namespace kmap::view2 {

auto Parent::create( CreateContext const& ctx
                   , Uuid const& root ) const
    -> Result< UuidSet >
{
    KMAP_THROW_EXCEPTION_MSG( "This is not what you want" );
}

auto Parent::fetch( FetchContext const& ctx
                  , Uuid const& node ) const
    -> FetchSet
{
    KM_RESULT_PROLOG();

    if( pred_ )
    {
        auto dispatch = util::Dispatch
        {
            [ & ]( char const* pred )
            {
                return view2::parent( std::string{ pred } ).fetch( ctx, node );
            }
        ,   [ & ]( std::string const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto fs = FetchSet{};

                if( auto const parent = nw->fetch_parent( node )
                  ; parent )
                {
                    if( pred == KTRYE( nw->fetch_heading( parent.value() ) ) )
                    {
                        return FetchSet{ LinkNode{ .id = parent.value() } };
                    }
                }

                return fs;
            }
        ,   [ & ]( Uuid const& pred )
            {
                auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );
                auto fs = FetchSet{};

                if( auto const parent = nw->fetch_parent( node )
                  ; parent )
                {
                    if( pred == parent.value() )
                    {
                        fs = FetchSet{ LinkNode{ .id = pred } };
                    }
                }

                return fs;
            }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        auto const nw = KTRYE( ctx.km.fetch_component< com::Network >() );

        if( auto const parent = nw->fetch_parent( node )
          ; parent )
        {
            return FetchSet{ LinkNode{ .id = parent.value() } };
        }
        else
        {
            return FetchSet{};
        }
    }
}

auto Parent::new_link() const
    -> std::unique_ptr< Link >
{
    return std::make_unique< Parent >();
}

auto Parent::to_string() const
    -> std::string
{
    if( pred_ )
    {
        auto const dispatch = util::Dispatch
        {
            [ & ]( auto const& arg ){ return fmt::format( "parent( '{}' )", arg ); }
        };

        return std::visit( dispatch, pred_.value() );
    }
    else
    {
        return "parent";
    }
}

auto Parent::compare_less( Link const& other ) const
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

/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_PATH_PRED_COMMON_HPP
#define KMAP_PATH_PRED_COMMON_HPP

#include "../../common.hpp"

namespace kmap::view::pred {

using src = StrongType< Uuid, class Src >;
using dst = StrongType< Uuid, class Dst >;

// I'm looking to make a node_view


namespace pred
{
    namespace detail
    {
        struct Predicate
        {
            virtual auto match( Kmap const& kmap
                              , Uuid const& id ) const
                -> bool = 0;
        };
    }
    struct heading : detail::Predicate
    {
        std::string pred;

        auto match( Kmap const& kmap
                  , Uuid const& id ) const
            -> bool override
        {
            auto rv = false;

            if( auto r = nw->fetch_heading( id );
              ; r )
            {
                rv = ( h.value() == pred );
            }

            return rv;
        } 
        // What about... create? Let's give it a try.
        // Unfortunately, I don't think so. `create` is entirely dependent on the caller's type (child, sibling, etc.).
        // It's also dependent on pred, but, this is general whilst that is specific.
        // Actually, both pred and view are dependent on each other, but presumably the number of preds is limited.
        // This differs from match because match isn't dependent on view. `view` provides a Uuid consistently.
        // match( Uuid, <pred> )
        // create( <view>, <pred> ), that's the difficulty.
        // There is, however, a 
        // child::create( kmap, )
        auto create( auto& create_fn
                   , Uuid const& root )
        {
            create_fn( root, pred );
        }
    };
    struct src : detail::Predicate
    {
        Uuid pred;

        auto match( Kmap const& kmap
                  , Uuid const& id ) const
            -> bool override
        {
            auto rv = false;

            if( auto r = kmap.fetch_alias_src( id );
              ; r )
            {
                rv = ( r.value() == pred );
            }

            return rv;
        } 
    };
    struct any_of : detail::Predicate
    {
        std::set< std::string > pred; // TODO: std::set< detail::Predicate >?

        auto match( Kmap const& kmap
                  , Uuid const& id ) const
            -> bool override
        {
            return pred.contains( id );
        } 
    };
    struct none_of : detail::Predicate
    {
        std::set< std::string > pred; // TODO: std::set< detail::Predicate >?

        auto match( Kmap const& kmap
                  , Uuid const& id ) const
            -> bool override
        {
            return !pred.contains( id );
        } 
    };
    // OK, this one may be trouble. Basically, needs to match a UuidSet as input, rather than single Uuid.
    // E.g., view::child( pred::any_of( 'x', 'y' ) ) => fetch_children() | to_headings == any_of( 'x', 'y' )
    // I think every other predicate works nicely within this framework. PredFn may differ, again, because it may depend on a set.
    // view::all_of( view::child( "x" ), view::child( "y" ), view::desc( "x.y.z" ) )
    // Does the above make sense? If I make view::all_of( <view> ), it means I could insert any view, and they'd all need to jive.
    // I think so...
    struct all_of : detail::Predicate
    {
        std::set< std::string > pred; // TODO: std::set< detail::Predicate >?

        auto match( Kmap const& kmap
                  , Uuid const& id ) const
            -> bool override
        {
            return !pred.contains( id );
        } 
    };
}

namespace kmap
{
    namespace pred = kmap::view::pred;
}

#endif // KMAP_PATH_PRED_COMMON_HPP


/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "path/node_view2.hpp"

#include "com/database/root_node.hpp"
#include "contract.hpp"
#include "error/master.hpp"
#include "kmap.hpp"
#include "utility.hpp"


// Temporary...
#include "path/act/abs_path.hpp"

namespace kmap::view2 {

auto ForwardingLink::create( CreateContext const& ctx 
                           , Uuid const& root ) const
    -> Result< UuidSet >
{
    auto rv = KMAP_MAKE_RESULT( UuidSet );
    auto ns = UuidSet{};

    // Hmm... I understand that one can view predicated fetch as `lhs | rhs | filter( sel )`
    // But now what about 'create'? Hmm... Let's maybe examine some use cases?
    // --task--
    // view::task def:
    // auto const task_root = view::abs_root
    //                      | view::direct_desc( "task" );
    // auto const task = view::task_root
    //                 | view::child
    //                 | view::child( all_of( "problem", "solution" ) );
    // Usage:
    // view::task | view
    // Hmm.. Already I see a conundrum. 
    // How, then, do I use view::task( "my_task" )? If ForwardingLink forwards to the last link in the chain, which would be child( "problem", "solution" )
    // What about this somewhat round about:
    // auto const task = view::task_root
    //                 | view::child
    //                 | view::child( all_of( "problem", "solution" ) );
    //                 | view::parent
    // ???
    // This... would work two-fold. It would return the actual task node, rather than its children, and it would also be in line with fowarding to last link.
    // I THINK IT WORKS & Makes Sense!
    // Need to be able to handle:
    // view::child( "victor" )
    // view::attr // non-predicated create
    // view::child( "charlie" )
    // view::create_node
    // Maybe something like this could work for the problem related to how to specify a create and structured node in the same expression: 
    //     where `view::tail_predicate` is able to (somehow) access `chain.back().selection` and use it as its own.
    //     This works even when view::task is used without a predicate, such as selecting an arbitrary task.
    // auto const task = view::task_root
    //                 | view::child( view::tail_predicate )
    //                 | view::child( all_of( "problem", "solution" ) );
    //                 | view::parent
    auto const [ lhs, rhs ] = links;

    if( selection )
    {
        if( auto const prhs = dynamic_cast< PredLink const* >( rhs.get() )
          ; prhs )
        {
            if( auto const cprhs = dynamic_cast< CreatablePredLink const* >( &( ( *prhs )( selection.value() ) ) )
              ; cprhs )
            {
                for( auto const& ln : lhs->fetch( ctx, root ) )
                {
                    auto rs = KTRY( cprhs->create( ctx, ln ) );

                    ns.insert( rs.begin(), rs.end() );
                }
            }
            else
            {
                KMAP_THROW_EXCEPTION_MSG( "creatable used against non-creatable view link" );
            }

            rv = ns;
        }
        else
        {
            KMAP_THROW_EXCEPTION_MSG( "predicate used against non-predicable view link" );
        }
    }
    else if( auto const crhs = dynamic_cast< CreatablePredLink const* >( rhs.get() )
           ; crhs )
    {
        for( auto const& ln : lhs->fetch( ctx, root ) )
        {
            auto const rs = KTRY( crhs->create( ctx, ln ) );

            ns.insert( rs.begin(), rs.end() );
        }

        rv = ns;
    }
    else
    {
        KMAP_THROW_EXCEPTION_MSG( "creatable used against non-creatable view link" );
    }

    return rv;
}
    
auto ForwardingLink::fetch( FetchContext const& ctx 
                          , Uuid const& root ) const
    -> UuidSet
{
    // TODO: I don't think ForwardingLink is a predicable type. It's simply a placeholder. It doesn't involve itself in selection itself.
    //       Wait, what about auto kibble = bits | chits; root | kibble( "bacon" ) | to_node_set( km )?
    //       I think forward does need to be predicable...
    auto rv = UuidSet{};
    auto const [ lhs, rhs ] = links;

    if( selection )
    {
        if( auto const prhs = dynamic_cast< PredLink* >( rhs.get() )
          ; prhs )
        {
            for( auto const& ln : lhs->fetch( ctx, root ) )
            {
                auto rs = ( *prhs )( selection.value() ).fetch( ctx, ln );

                rv.insert( rs.begin(), rs.end() );
            }
        }
        else
        {
            KMAP_THROW_EXCEPTION_MSG( "predicate used against non-predicable view link" );
        }
    }
    else
    {
        for( auto const& ln : lhs->fetch( ctx, root ) )
        {
            auto rs = rhs->fetch( ctx, ln );

            rv.insert( rs.begin(), rs.end() );
        }
    }

    return rv;
}

auto ForwardingLink::operator()( view::SelectionVariant const& sel ) const
    -> ForwardingLink const&
{
    selection = sel;

    return *this;
}

/*----------------Derivatives----------------*/

auto Child::operator()( view::SelectionVariant const& sel ) const
    -> Child const&
{
    selection = sel;

    return *this;
}

auto Child::fetch( FetchContext const& ctx
                 , Uuid const& node ) const
    -> UuidSet
{
    if( selection )
    {
        return view::make( node )
             | view::child( selection.value() )
             | view::to_node_set( ctx.km );
    }
    else
    {
        return view::make( node )
             | view::child
             | view::to_node_set( ctx.km );
    }
}

auto Desc::operator()( view::SelectionVariant const& sel ) const
    -> Desc const&
{
    selection = sel;

    return *this;
}

auto Desc::fetch( FetchContext const& ctx
                , Uuid const& node ) const
    -> UuidSet
{
    if( selection )
    {
        return view::make( node )
             | view::desc( selection.value() )
             | view::to_node_set( ctx.km );
    }
    else
    {
        return view::make( node )
             | view::desc
             | view::to_node_set( ctx.km );
    }
}

auto DirectDesc::fetch( FetchContext const& ctx
                      , Uuid const& node ) const
    -> UuidSet
{
    if( selection )
    {
        return view::make( node )
             | view::direct_desc( selection.value() )
             | view::to_node_set( ctx.km );
    }
    else
    {
        return view::make( node )
             | view::direct_desc
             | view::to_node_set( ctx.km );
    }
}

auto DirectDesc::operator()( view::SelectionVariant const& sel ) const
    -> DirectDesc const&
{
    selection = sel;

    return *this;
}

auto Root::fetch( FetchContext const& ctx ) const
    -> UuidSet
{
    return nodes;
}

auto AbsRoot::fetch( FetchContext const& ctx ) const
    -> UuidSet
{
    auto const rn = KTRYE( ctx.km.fetch_component< com::RootNode >() );

    return UuidSet{ rn->root_node() };
}

auto root( Uuid const& node )
    -> Chain
{
    return Chain{ .anchor = std::make_shared< Root >( UuidSet{ node } ) };
}
auto root( UuidSet const& nodes )
    -> Chain
{
    return Chain{ .anchor = std::make_shared< Root >( nodes ) };
}

AbsPathFlat::AbsPathFlat( Kmap const& kmap )
    : km{ kmap }
{
}

ToNodeSet::ToNodeSet( Kmap const& kmap )
    : km{ kmap }
{
}

auto abs_path_flat( Kmap const& km )
    -> AbsPathFlat
{
    return AbsPathFlat{ km };
}

auto to_node_set( Kmap const& km )
    -> ToNodeSet 
{
    return ToNodeSet{ km };
}

// TODO: Temporary hack to test out functionality. Should use view::abs_path model directly, but that involves a lot more transitioning to view2, so hacking it for now.
// auto operator|( Anchor const& lhs
//               , AbsPathFlat const& rhs )
//     -> Result< std::string >
// {
//     auto rv = KMAP_MAKE_RESULT( std::string );

//     BC_CONTRACT()
//         BC_POST([ & ]
//         {
//             if( rv )
//             {
//                 BC_ASSERT( is_valid_heading_path( rv.value() ) );
//             }
//         })
//     ;

//     auto const av = lhs.fetch( FetchContext{ .km=rhs.km, .chain=Chain{ .anchor=lhs } );

//     KMAP_ENSURE( av.size() == 1, error_code::common::uncategorized );

//     rv = KTRY( view::make( *av.begin() )
//              | act::abs_path_flat( rhs.km ) );

//     return rv;
// }

auto operator|( Chain const& lhs
              , AbsPathFlat const& rhs )
    -> Result< std::string >
{
    auto rv = KMAP_MAKE_RESULT( std::string );

    BC_CONTRACT()
        BC_PRE([ & ]
        {
            BC_ASSERT( lhs.anchor );
        })
        BC_POST([ & ]
        {
            if( rv )
            {
                BC_ASSERT( is_valid_heading_path( rv.value() ) );
            }
        })
    ;

    auto const ctx = FetchContext{ rhs.km, lhs };
    auto const av = lhs.anchor->fetch( ctx );

    if( lhs.links.empty() )
    {
        KMAP_ENSURE( av.size() == 1, error_code::common::uncategorized );

        rv = KTRY( view::make( *av.begin() )
                 | act::abs_path_flat( rhs.km ) );
    }
    else
    {
        auto const ns = lhs | view2::to_node_set( rhs.km );

        KMAP_ENSURE( ns.size() == 1, error_code::common::uncategorized );

        rv = KTRY( view::make( av )
                 | view::desc( *ns.begin() )
                 | act::abs_path_flat( ctx.km ) );
    }

    return rv;
}

// auto operator|( Anchor const& lhs
//               , ToNodeSet const& rhs )
//     -> UuidSet
// {
//     auto rv = UuidSet{};

//     rv = lhs.fetch( rhs.km );

//     return rv;
// }

auto operator|( Chain const& lhs
              , ToNodeSet const& rhs )
    -> UuidSet
{
    auto rv = UuidSet{};
    auto const ctx = FetchContext{ rhs.km, lhs };
    auto ns = lhs.anchor->fetch( ctx );

    for( auto const& link : lhs.links )
    {
        auto next_ns = decltype( ns ){};

        for( auto const& node : ns )
        {
            auto const tns = link->fetch( ctx, node );

            next_ns.insert( tns.begin(), tns.end() );
        }

        ns = next_ns;
    }

    rv = ns;

    return rv;
}

} // namespace kmap::view2
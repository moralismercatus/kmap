/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "arg.hpp"

#include "cmd/command.hpp"
#include "com/cli/cli.hpp"
#include "com/database/db.hpp"
#include "com/filesystem/filesystem.hpp"
#include "com/network/network.hpp"
#include "contract.hpp"
#include "io.hpp"
#include "kmap.hpp"
#include "path.hpp"
#include "utility.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <emscripten.h>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/remove_if.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/split.hpp>
#include <range/v3/view/transform.hpp>

#include <map>

using namespace ranges;
namespace fs = boost::filesystem;

namespace kmap {

Argument::Argument( std::string const& arg_desc
                  , std::string const& cmd_ctx_desc )
    : desc_{ arg_desc }
    , cmd_ctx_desc_{ cmd_ctx_desc }
{
}

auto Argument::is_fmt_malformed( std::string const& raw ) const
    -> Optional< uint32_t >
{
    return nullopt;
}

auto Argument::complete( std::string const& raw ) const
    -> StringVec
{
    return { raw };
}

auto Argument::tip( std::string const& raw ) const
    -> std::string
{
    // Default is just to return argument description.
    return desc();
}

auto Argument::desc() const
    -> std::string
{
    return desc_;
}

auto Argument::cmd_ctx_desc() const
    -> std::string
{
    return cmd_ctx_desc_;
}

auto Argument::is_optional() const
    -> bool
{
    return optional_;
}

auto Argument::make_optional()
	-> Argument&
{
    optional_ = true;

    return *this;
}

CommandArg::CommandArg( std::string const& arg_desc
                      , std::string const& cmd_ctx_desc
                      , Kmap& kmap )
    : Argument{ arg_desc
              , cmd_ctx_desc}
    , kmap_{ kmap }
{
}

auto CommandArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t > 
{
    return nullopt; // TODO: Check for valid command chars.
}

auto CommandArg::complete( std::string const& raw ) const 
    -> StringVec
{
    auto const cli = KTRYE( kmap_.fetch_component< com::Cli >() );
    auto rm_pred = [ & ]( auto const& e )
    {
        return raw.size() != match_length( raw
                                         , e );
    };
    auto const cmds = cli->valid_commands();

    return cmds
         | views::transform( [ & ]( auto const& e ){ return e.command; } )
         | views::remove_if( rm_pred )
         | to< StringVec >();
}

FsPathArg::FsPathArg( std::string const& arg_desc
                    , std::string const& cmd_ctx_desc )
    : Argument{ arg_desc
              , cmd_ctx_desc}
{
}

auto FsPathArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t > 
{
    return nullopt; // TODO: Check for valid fs chars.
}

auto FsPathArg::complete( std::string const& raw ) const 
    -> StringVec
{
    // if( raw.empty() )
    // {
    //     return {};
    // }

    auto const parent_path = fs::path{ raw }.parent_path();
    fmt::print( "parent path: {}\n", parent_path.string() );
    auto to_paths = views::transform( [ & ]( auto const& e ){ return ( parent_path / e.path().filename() ).string(); } );
    auto const di = fs::directory_iterator{ com::kmap_root_dir / parent_path };
    auto const paths = di
                     | to_paths
                     | to_vector;

    return fetch_completions( raw 
                            , paths );
}

HeadingArg::HeadingArg( std::string const& arg_desc
                      , std::string const& cmd_ctx_desc )
    : Argument{ arg_desc
              , cmd_ctx_desc}
{
}

auto HeadingArg::is_fmt_malformed( std::string const& raw ) const
    -> Optional< uint32_t >
{
    return fetch_first_invalid( raw );
}

TitleArg::TitleArg( std::string const& arg_desc
                  , std::string const& cmd_ctx_desc )
    : Argument{ arg_desc
              , cmd_ctx_desc}
{
}

auto TitleArg::is_fmt_malformed( std::string const& raw ) const
    -> Optional< uint32_t >
{
    return nullopt; // TODO: Fetch first invalid title char.
}

UIntArg::UIntArg( std::string const& arg_desc
                , std::string const& cmd_ctx_desc )
    : Argument{ arg_desc
              , cmd_ctx_desc}
{
}

auto UIntArg::is_fmt_malformed( std::string const& raw ) const
    -> Optional< uint32_t >
{
    auto rv = Optional< uint32_t >{};

    try
    {
        auto const res = stoull( raw );
        (void)res;

        rv = nullopt;
    }
    catch(const std::exception& e)
    {
        rv = 1;
    }
    
    return rv;
}

HeadingPathArg::HeadingPathArg( std::string const& arg_desc
                              , std::string const& cmd_ctx_desc
                              , Kmap const& kmap )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

HeadingPathArg::HeadingPathArg( std::string const& arg_desc
                              , std::string const& cmd_ctx_desc
                              , Heading const& root 
                              , Kmap const& kmap )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
    , root_{ root }
{
}

auto HeadingPathArg::is_fmt_malformed( std::string const& raw ) const
    -> Optional< uint32_t >
{
    auto count = 0u;

    for( auto const e : raw
                      | views::split( '.' )
                      | views::transform( []( auto const& e ){ return to< std::string >( e ); } ) )
    {
        auto const inv = fetch_first_invalid( e );

        if( inv )
        {
            return count + *inv;
        }
        else
        {
            count += distance( e );
        }
    }

    return nullopt;
}

auto HeadingPathArg::complete( std::string const& raw ) const
    -> StringVec
{
    auto rv = StringVec{};
    auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );

    if( auto const root = view::abs_root
                        | view::desc( root_ )
                        | view::fetch_node( kmap_ )
      ; root )
    {
        if( raw.empty() )
        {
            auto const children = nw->fetch_children( root.value() );
            rv = children
               | views::transform( [ & ]( auto const& e ){ return nw->fetch_heading( e ).value(); } )
               | to< StringVec >();
        }
        else
        {
            auto const completed = complete_path( kmap_
                                                , root.value()
                                                , nw->selected_node()
                                                , raw );
            if( !completed )
            {
                return {};
            }
            else
            {
                rv = completed.value()
                   | views::transform( &CompletionNode::path )
                   | to< StringVec >();
            }
        }
    }

    return rv;
}

// InvertedPathArg::InvertedPathArg( std::string const& arg_desc
//                               , std::string const& cmd_ctx_desc
//                               , Kmap const& kmap )
//     : Argument{ arg_desc
//               , cmd_ctx_desc }
//     , kmap_{ kmap }
// {
// }

// InvertedPathArg::InvertedPathArg( std::string const& arg_desc
//                                 , std::string const& cmd_ctx_desc
//                                 , Heading const& root 
//                                 , Kmap const& kmap )
//     : Argument{ arg_desc
//               , cmd_ctx_desc }
//     , kmap_{ kmap }
//     , root_{ root }
// {
// }

// auto InvertedPathArg::is_fmt_malformed( std::string const& raw ) const
//     -> Optional< uint32_t >
// {
//     if( !raw.empty() 
//      && raw[ 0 ] == '.' )
//     {
//         return false;
//     }
    
//     auto count = 0u;

//     for( auto const e : raw
//                       | views::split( '.' )
//                       | views::transform( []( auto const& e ){ return to< std::string >( e ); } ) )
//     {
//         auto const inv = fetch_first_invalid( e );

//         if( inv )
//         {
//             return count + *inv;
//         }
//         else
//         {
//             count += distance( e );
//         }
//     }

//     return nullopt;
// }

// auto InvertedPathArg::complete( std::string const& raw ) const
//     -> StringVec
// {
//     auto const root = kmap_.fetch_leaf( root_ )
//                            .value_or( kmap_.root_node_id() );
//     auto const ipaths = kmap_.descendant_ipaths( root );
//     auto manip = views::transform( [ & ]( auto const& e )
//     {
//         return flatten( e
//                       | views::reverse
//                       | views::drop( distance( KTRYE( absolute_path_flat( kmap_, root ) ) ) )
//                       | views::reverse
//                       | to< StringVec >() ) ;
//     } );
//     auto rm_nonmatch = views::remove_if( [ & ]( auto const& e )
//     {
//         using boost::algorithm::starts_with;

//         return !starts_with( e
//                            , raw );
//     } );
//     auto const rv = ipaths
//                   | manip
//                   | rm_nonmatch
//                   | to< StringVec >();

//     return rv;
// }

JumpInArg::JumpInArg( std::string const& arg_desc
                    , std::string const& cmd_ctx_desc
                    , Kmap const& kmap )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto JumpInArg::is_fmt_malformed( std::string const& raw ) const
    -> Optional< uint32_t >
{
    return nullopt; // TODO: contains valid path chars
}

auto JumpInArg::complete( std::string const& raw ) const
    -> StringVec
{
    return { raw };
}

JumpOutArg::JumpOutArg( std::string const& arg_desc
                      , std::string const& cmd_ctx_desc
                      , Kmap const& kmap )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto JumpOutArg::is_fmt_malformed( std::string const& raw ) const
    -> Optional< uint32_t >
{
    return nullopt; // TODO: contains valid path chars
}

auto JumpOutArg::complete( std::string const& raw ) const
    -> StringVec
{
    return { raw };
}

DailyLogArg::DailyLogArg( std::string const& arg_desc
                        , std::string const& cmd_ctx_desc
                        , Kmap const& kmap  )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto DailyLogArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t >
{
    return nullopt; // TODO: regex match mm-dd-yyyy
}

auto DailyLogArg::complete( std::string const& raw ) const 
    -> StringVec 
{
    auto const log_root = view::abs_root
                        | view::direct_desc( "log.daily" )
                        | view::fetch_node( kmap_ );

    if( !log_root )
    {
        return {};
    }

    auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
    auto const parent = log_root.value();
    auto const cids = nw->fetch_children( parent );
    auto const db = KTRYE( kmap_.fetch_component< com::Database >() );
    auto to_headings = views::transform( [ & ]( auto const& e )
    {
        return KTRYE( db->fetch_heading( nw->resolve( e ) ) );
    } );
    auto rm_disparate = views::remove_if( [ & ]( auto const& e )
    {
        return raw.size() != match_length( raw, e );
    } );

    return cids
         | to_headings
         | rm_disparate
         | to< StringVec >();
}

TagPathArg::TagPathArg( std::string const& arg_desc
                      , std::string const& cmd_ctx_desc
                      , Kmap const& kmap  )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto TagPathArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t >
{
    return nullopt; // TODO: regex match valid tag syntax.
}

DefinitionPathArg::DefinitionPathArg( std::string const& arg_desc
                                    , std::string const& cmd_ctx_desc
                                    , Kmap const& kmap  )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto DefinitionPathArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t >
{
    return nullopt; // TODO: regex match valid tag syntax.
}

/**
 * root_path: Path to uppermost allowed node e.g., .root.tags or .root.log.daily
 * raw: Partial or full heading of node to complete.
 **/
auto complete( Kmap const& kmap
             , std::string const& root_path
             , std::string const& raw )
    -> StringVec
{
    auto const nw = KTRYE( kmap.fetch_component< com::Network >() );
    auto const root = fetch_descendant( kmap
                                      , kmap.root_node_id()
                                      , nw->selected_node()
                                      , root_path );

    if( !root )
    {
        return {};
    }

    auto const parent = root.value();
    auto const cids = nw->fetch_children( parent );
    auto to_headings = views::transform( [ & ]( auto const& e )
    {
        return nw->fetch_heading( e ).value();
    } );
    auto rm_disparate = views::remove_if( [ & ]( auto const& e )
    {
        return raw.size() != match_length( raw
                                         , e );
    } );

    return cids
         | to_headings
         | rm_disparate
         | to< StringVec >();
}

// TODO: Much of this functionality is redundant with DailyLogArg, (other), etc.
auto TagPathArg::complete( std::string const& raw ) const 
    -> StringVec 
{
    auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
    auto const tags_root = fetch_descendant( kmap_
                                           , kmap_.root_node_id()
                                           , nw->selected_node()
                                           , "tags" );

    if( !tags_root )
    {
        return {};
    }

    auto const parent = tags_root.value();
    auto const completed = complete_path( kmap_
                                        , parent
                                        , nw->selected_node()
                                        , raw );
    if( !completed )
    {
        return {};
    }
    else
    {
        return completed.value()
             | views::transform( &CompletionNode::path )
             | to< StringVec >();
    }
}

// TODO: Much of this functionality is redundant with DailyLogArg, (other), etc.
auto DefinitionPathArg::complete( std::string const& raw ) const 
    -> StringVec 
{
    auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
    auto const defs_root = view::abs_root
                         | view::direct_desc( "definitions" )
                         | view::fetch_node( kmap_ );

    if( !defs_root )
    {
        return {};
    }

    auto const parent = defs_root.value();
    auto const completed = complete_path( kmap_
                                        , parent
                                        , nw->selected_node()
                                        , raw );
    if( !completed )
    {
        return {};
    }
    else
    {
        return completed.value()
             | views::transform( &CompletionNode::path )
             | to< StringVec >();
    }
}

ResourcePathArg::ResourcePathArg( std::string const& arg_desc
                                , std::string const& cmd_ctx_desc
                                , Kmap const& kmap  )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto ResourcePathArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t >
{
    return nullopt; // TODO: regex match valid tag syntax.
}

// TODO: Much of this functionality is redundant with DailyLogArg, (other), etc.
auto ResourcePathArg::complete( std::string const& raw ) const 
    -> StringVec 
{
    auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
    auto const resources_root = view::abs_root
                              | view::direct_desc( "resources" )
                              | view::fetch_node( kmap_ );

    if( !resources_root )
    {
        return {};
    }

    auto const completed = complete_path( kmap_
                                        , resources_root.value()
                                        , nw->selected_node()
                                        , raw );
    if( !completed )
    {
        return {};
    }
    else
    {
        return completed.value()
             | views::transform( &CompletionNode::path )
             | to< StringVec >();
    }
}

AliasDestArg::AliasDestArg( std::string const& arg_desc
                          , std::string const& cmd_ctx_desc
                          , Kmap const& kmap  )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto AliasDestArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t >
{
    return nullopt; // TODO: regex match valid tag syntax.
}

auto AliasDestArg::complete( std::string const& raw ) const 
    -> StringVec 
{
    auto nw = KTRYE( kmap_.fetch_component< com::Network >() );
    auto const selected = nw->selected_node();
    auto const dsts = nw->alias_store().fetch_aliases_to( selected );
    auto const map = dsts
                   | views::transform( [ & ]( auto const& e ){ return nw->fetch_heading( e ).value(); } )
                   | to_vector;

    return map;
}

ProjectArg::ProjectArg( std::string const& arg_desc
                      , std::string const& cmd_ctx_desc
                      , Kmap const& kmap  )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto ProjectArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t >
{
    return nullopt; // TODO: regex match valid tag syntax.
}

auto ProjectArg::complete( std::string const& raw ) const 
    -> StringVec 
{
    auto complete = [ & ]
                    ( auto const& kmap
                    , auto const& rel_root
                    , auto const& raw ) -> StringVec
    {
        auto const nw = KTRYE( kmap.template fetch_component< com::Network >() );
        auto const root = view::abs_root
                        | view::direct_desc( rel_root )
                        | view::fetch_node( kmap );

        if( !root )
        {
            return {};
        }

        auto const parent = root.value();
        auto const completed = complete_path( kmap_
                                            , parent
                                            , nw->selected_node()
                                            , raw );
        if( !completed )
        {
            return {};
        }
        else
        {
            return completed.value()
                 | views::transform( &CompletionNode::path )
                 | to< StringVec >();
        }
    };

    auto const cs = { complete( kmap_
                              , "/projects.open.active"
                              , raw )
                    , complete( kmap_
                              , "/projects.open.inactive"
                              , raw )
                    , complete( kmap_
                              , "/projects.closed"
                              , raw ) };

    return cs
         | views::join
         | to< StringVec >();
}

ConclusionArg::ConclusionArg( std::string const& arg_desc
                            , std::string const& cmd_ctx_desc
                            , Kmap const& kmap  )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto ConclusionArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t >
{
    return nullopt; // TODO: regex match valid tag syntax.
}

auto ConclusionArg::complete( std::string const& raw ) const 
    -> StringVec 
{
    auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
    auto complete = [ & ]
                    ( auto const& kmap
                    , auto const& rel_root
                    , auto const& raw ) -> StringVec
    {
        auto const root = fetch_descendants( kmap_
                                           , kmap_.root_node_id()
                                           , nw->selected_node()
                                           , rel_root );

        if( !root )
        {
            return {};
        }

        auto const parent = root.value().back();
        auto const completed = complete_path( kmap_
                                            , parent
                                            , nw->selected_node()
                                            , raw );
        if( !completed )
        {
            return {};
        }
        else
        {
            return completed.value()
                 | views::transform( &CompletionNode::path )
                 | to< StringVec >();
        }
    };

    return complete( kmap_
                   , "/conclusions"
                   , raw );
}

RecipeArg::RecipeArg( std::string const& arg_desc
                            , std::string const& cmd_ctx_desc
                            , Kmap const& kmap  )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto RecipeArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t >
{
    return nullopt; // TODO: regex match valid tag syntax.
}

auto RecipeArg::complete( std::string const& raw ) const 
    -> StringVec 
{
    auto complete = [ & ]
                    ( auto const& kmap
                    , auto const& rel_root
                    , auto const& raw ) -> StringVec
    {
        auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );
        auto const root = fetch_descendants( kmap_
                                           , kmap_.root_node_id()
                                           , nw->selected_node()
                                           , rel_root );

        if( !root )
        {
            return {};
        }

        auto const parent = root.value().back();
        auto const completed = complete_path( kmap_
                                            , parent
                                            , nw->selected_node()
                                            , raw );
        if( !completed )
        {
            return {};
        }
        else
        {
            return completed.value()
                 | views::transform( &CompletionNode::path )
                 | to< StringVec >();
        }
    };

    return complete( kmap_
                   , "/recipes"
                   , raw );
}

CommandPathArg::CommandPathArg( std::string const& arg_desc
                              , std::string const& cmd_ctx_desc
                              , Kmap const& kmap  )
    : Argument{ arg_desc
              , cmd_ctx_desc }
    , kmap_{ kmap }
{
}

auto CommandPathArg::is_fmt_malformed( std::string const& raw ) const 
    -> Optional< uint32_t >
{
    return nullopt; // TODO: regex match valid tag syntax.
}

auto CommandPathArg::complete( std::string const& raw ) const 
    -> StringVec 
{
    auto rv = StringVec{};
    auto const cmd_abs_path = "/setting.command";
    auto const nw = KTRYE( kmap_.fetch_component< com::Network >() );

    if( auto const cmd_root = view::abs_root
                            | view::desc( cmd_abs_path )
                            | view::fetch_node( kmap_ )
      ; cmd_root )
    {
        auto const prospects = complete_path( kmap_
                                            , cmd_root.value()
                                            , cmd_root.value()
                                            , raw );
        auto const filter = views::filter( [ & ]( auto const& e )
        {
            return nw->exists( fmt::format( "{}.{}", cmd_abs_path, e.path ) ) // Ensure it's an absolute cmd path.
                && nw->is_lineal( e.target, "guard" )
                && nw->is_lineal( e.target, "action" )
                 ;
        } );

        if( prospects )
        {
            rv = prospects.value()
               | filter
               | views::transform( &CompletionNode::path )
               | to< StringVec >();
        }
        else
        {
            rv = {};
        }
    }

    return rv;
}

#if 0
namespace unconditional {
namespace {

auto const guard_code =
R"%%%(```javascript
return true;
```)%%%";
auto const completion_code =
R"%%%(```javascript
let rv = new kmap.VectorString();

rv.push_back( arg )

return rv;
```)%%%";

auto const description = "any valid text";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    unconditional
,   description 
,   guard
,   completion
);

} // namespace anon
} // namespace unconditional
#endif // 0

#if 0
namespace heading {
namespace {

auto const guard_code =
R"%%%(```javascript
return kmap.is_valid_heading( arg );
```)%%%";
auto const completion_code =
R"%%%(```javascript
let rv = new kmap.VectorString();

rv.push_back( arg )

return rv;
```)%%%";

auto const description = "node heading";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    heading 
,   description 
,   guard
,   completion
);

} // namespace anon
} // namespace heading 
#endif // 0

#if 0
namespace heading_path {
namespace {

auto const guard_code =
R"%%%(```javascript
return kmap.is_valid_heading_path( arg );
```)%%%";
auto const completion_code =
R"%%%(```javascript
return kmap.complete_heading_path( arg );
```)%%%";

auto const description = "node heading path";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    heading_path 
,   description 
,   guard
,   completion
);

} // namespace anon
} // namespace heading_path 
#endif // 0

#if 0
namespace filesystem_path_def {
namespace {

auto const guard_code =
R"%%%(```javascript
return kmap.success( "success" );
```)%%%";
auto const completion_code =
R"%%%(```javascript
return kmap.complete_filesystem_path( arg );
```)%%%";

auto const description = "filesystem path";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    filesystem_path 
,   description 
,   guard
,   completion
);

} // namespace anon
} // namespace filesystem_path_def
#endif // 0

#if 0
namespace numeric_decimal_def {
namespace {

auto const guard_code =
R"%%%(```javascript
const parsed = Number( arg );

if( Number.isNaN( parsed ) || parsed < 0 )
{
    return kmap.failure( 'not an decimal number' );
}
else if( parsed > Number.MAX_SAFE_INTEGER)
{
    return kmap.failure( 'number too large' );
}
else
{
    return kmap.success( 'decimal number' );
}
```)%%%";
auto const completion_code =
R"%%%(```javascript
return arg;
```)%%%";

auto const description = "decimal number";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    numeric.decimal
,   description 
,   guard
,   completion
);

} // namespace anon
} // namespace numeric_decimal_def 
#endif // 0

#if 0
namespace numeric_unsigned_integer_def {
namespace {

// TODO: As Number accepts decimals, too, how is this handled? We could reject floats/decimals, as this is an integer.
auto const guard_code =
R"%%%(```javascript
const parsed = Number( arg );

if( Number.isNaN( parsed ) || parsed < 0 )
{
    return kmap.failure( 'not an unsigned integer' );
}
else if( parsed > Number.MAX_SAFE_INTEGER)
{
    return kmap.failure( 'number too large' );
}
else
{
    return kmap.success( 'unsigned integer' );
}
```)%%%";
auto const completion_code =
R"%%%(```javascript
return arg;
```)%%%";

auto const description = "unsigned integer";
auto const guard = guard_code;
auto const completion = completion_code;

REGISTER_ARGUMENT
(
    numeric.unsigned_integer 
,   description 
,   guard
,   completion
);

} // namespace anon
} // namespace numeric_unsigned_integer_def 
#endif // 0

} // namespace kmap

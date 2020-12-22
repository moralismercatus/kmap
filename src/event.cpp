#include "event.hpp"
#include "command.hpp"

namespace kmap
{

auto default_network_key_down bindings = std::map< Heading
                                                 , Event >
{
    { Heading{ "network.key_down.h" }
    , Event{ { 72 }
           , "travel.left" } } 
,   { Heading{ "network.key_down.j" }
    , Event{ { 74 }
           , "travel.down" } } 
,   { Heading{ "network.key_down.k" }
    , Event{ { 75 }
           , "travel.up" } } 
,   { Heading{ "network.key_down.l" }
    , Event{ { 76 }
           , "travel.right" } } 
};

auto is_event( Kmap const& kmap
             , Uuid const& id )
    -> bool
{
    return has_geometry( kmap
                       , id
                       , std::regex{ "command" } );
}

auto fetch_network_events( Heading const& path )
    -> std::vector< Event >
{
    auto const event_root = kmap.fetch_or_create_descendant( 
                                                           , path );
    BC_ASSERT( event_root );
    auto const cids = kmap.fetch_children( event_root );
    auto const events = cids
                      | filter( []( auto const& e ){ return is_event( e ); } );
    
    return events;
}

struct CliSm 
{
    CliSm( Kmap& kmap )
        : kmap_{ kmap } 
    {
    }
    
    /* States */
    auto Prefix = state< class Prefix >;
    auto Command = state< class Prefix >;
    auto Arg = state< class Prefix >;

    /* Guards */
    auto is_cmd_prefix = [ & ]( auto const& e )
    {
        return e == ':';
    };

    auto is_space = [ & ]( auto const& e )
    {
        return e == ' ';
    };

    auto is_valid_cmd = [ & ]( auto const& e )
    {
        return exists( e );
    };

    /* Actions */
    auto complete_cmd = [ & ]( auto const& e )
    {
        // complete( e )    
    };

    auto push_char = [ & ]( std::string const& dst )
    {
        return [ &dst ]( auto const& e )
        {
            dst.push_back( e );
        };
    };

    auto pop_char = [ & ]( std::string const& dst )
    {
        return [ &dst ]( auto const& e )
        {
            dst.pop_back( e );
        };
    };

    // Table
    auto operator()()
    {
        using namespace sml;

        return make_transition_table
        (
        *   Prefix + event< char > [ is_cmd_prefix ]
                = Command
        ,   Prefix + event< Abort >
                / abort_cli
                = Prefix
        ,   Command + event< Abort >
                / abort_cli
                = Prefix
        ,   Command + event< Tab >
                / complete_cmd
                = Command
        ,   Command + event< Backspace > [ is_empty( cmd_ ) ]
                = Prefix
        ,   Command + event< Backspace > [ !is_empty( cmd_ ) ]
                / pop_char( cmd_ )
                = Command
        ,   Command + event< char > [ is_space
                                   && !is_valid_cmd ]
                / push_char( cmd_ )
                = Command
        ,   Command + event< char > [ is_space
                                   && is_valid_cmd ]
                / ( push_char( cmd_ )
                  , init_arg )
                = Arg
        );
    }

    Kmap& kmap_;
    std::string cmd_ = {}; // Stores present command input.
    std::string arg_ = {}; // Stores present argument input.
    CliCommand cli_cmd_ = {}; // To be assigned based on 'cmd_'
    CliCommand::Arguments::iterator cli_arg_ = {}; // To be assigned based on current arg being parsed.
};

struct EventSm 
{
    EventSm( Kmap& kmap )
        : kmap_{ kmap } 
    {
    }
    
    /* States */
    auto Window = state< class Window >;

    /* Guards */
    auto is_cmd_prefix = [ & ]( auto const& e )
    {
        return e == ':';
    };

    /* Actions */
    auto network_key_up = [ & ]( auto const& ev )
    {
        auto const events = fetch_events( network_key_up_path );

        // find ev.key_value in cmd bodies.
        // execute_command( found_cmd );
    };

    // Table
    auto operator()()
    {
        using namespace sml;

        return make_transition_table
        (
        *   Window + event< KeyUp > [ is_network_active ] / network_key_up
                = Window
        );
    }

    Kmap& kmap_;
    std::string const network_key_up_path = "network.key_up";
};

EventDispatch::EventDispatch( Kmap& kmap  )
    : sm_{ std::make_unique< EventSm >( kmap ) }
{
}

auto dispatch_key_up( uint64_t const& value )
    -> void
{
    sm_->process_event( KeyUp{ value } );
}

auto dispatch_key_down( uint64_t const& value )
    -> void
{
    sm_->process_event( KeyDown{ value } );
}

} // namespace kmap

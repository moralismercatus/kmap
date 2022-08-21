/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "timer.hpp"

#include "com/event/event.hpp"
#include "error/master.hpp"
#include "js_iface.hpp"
#include "kmap.hpp"
#include "path/node_view.hpp"

namespace kmap::com {

Timer::Timer( Kmap& kmap
            , std::set< std::string > const& requisites
            , std::string const& description )
    : Component{ kmap, requisites, description }
    , eclerk_{ kmap }
{
}

Timer::~Timer()
{
    for( auto const& id : timer_ids_ )
    {
        auto const script = fmt::format( R"%%%(clearInterval({});)%%%", id ); 

        if( !js::eval_void( script ) )
        {
            // TODO: How to properly handle error in dtor... not much can be done.
            fmt::print( stderr, "failed to clear interval for timer!" );
        }
    }
}

auto Timer::initialize()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( install_default_timers() );

    rv = outcome::success();

    return rv;
}

auto Timer::load()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

KMAP_LOG_LINE();
    KTRY( install_default_timers() );
KMAP_LOG_LINE();

    rv = outcome::success();

    return rv;
}

auto Timer::install_default_timers()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KTRY( eclerk_.install_subject( "chrono.timer" ) );
    KTRY( eclerk_.install_object( "chrono.second" ) );
    KTRY( eclerk_.install_object( "chrono.minute" ) );
    KTRY( eclerk_.install_object( "chrono.hour" ) );
    KTRY( eclerk_.install_verb( "intervaled" ) );

    KTRY( install_timer( "[ 'subject.chrono.timer', 'verb.intervaled', 'object.chrono.second' ]", 1000 ) ); // English: Timer [was] intervaled [for] second
    KTRY( install_timer( "[ 'subject.chrono.timer', 'verb.intervaled', 'object.chrono.minute' ]", 60000 ) ); // English: Timer [was] intervaled [for] minute
    KTRY( install_timer( "[ 'subject.chrono.timer', 'verb.intervaled', 'object.chrono.hour' ]", 360000 ) ); // English: Timer [was] intervaled [for] hour

    rv = outcome::success();

    return rv;
}

auto Timer::install_timer( std::string const& requisite_js_list
                         , uint32_t const& ms_frequency )
    -> Result< void >
{        
    auto rv = KMAP_MAKE_RESULT( void );
    auto const script = 
        fmt::format(
R"%%%( 
const requisites = to_VectorString( {} );
const interval = {};
return setInterval( function(){{ kmap.event_store().fire_event( requisites ); }}, interval ); 
)%%%"
            , requisite_js_list
            , ms_frequency );

    timer_ids_.emplace( KTRY( js::eval< uint32_t >( script ) ) );

    rv = outcome::success();

    return rv;
}

} // namespace kmap::com

namespace
{
    namespace timer_def
    {
        using namespace std::string_literals;

        REGISTER_COMPONENT
        (
            kmap::com::Timer
        ,   std::set({ "event_store"s })
        ,   "registers periodic timers that fire events"
        );
    } // namespace timer_def 
} // namespace anonymous

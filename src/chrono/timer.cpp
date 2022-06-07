/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "timer.hpp"

#include "error/master.hpp"
#include "event/event.hpp"
#include "js_iface.hpp"
#include "path/node_view.hpp"

namespace kmap::chrono {

Timer::Timer( Kmap& kmap )
    : kmap_{ kmap }
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

auto Timer::install_default_timers()
    -> Result< void >
{
    auto rv = KMAP_MAKE_RESULT( void );

    KMAP_TRY( eclerk_.install_subject( "chrono.timer" ) );
    KMAP_TRY( eclerk_.install_object( "chrono.second" ) );
    KMAP_TRY( eclerk_.install_object( "chrono.minute" ) );
    KMAP_TRY( eclerk_.install_object( "chrono.hour" ) );
    KMAP_TRY( eclerk_.install_verb( "intervaled" ) );

    KMAP_TRY( install_timer( "[ 'subject.chrono.timer', 'verb.intervaled', 'object.chrono.second' ]", 1000 ) ); // English: Timer [was] intervaled [for] second
    KMAP_TRY( install_timer( "[ 'subject.chrono.timer', 'verb.intervaled', 'object.chrono.minute' ]", 60000 ) ); // English: Timer [was] intervaled [for] minute
    KMAP_TRY( install_timer( "[ 'subject.chrono.timer', 'verb.intervaled', 'object.chrono.hour' ]", 360000 ) ); // English: Timer [was] intervaled [for] hour

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

    timer_ids_.emplace( KMAP_TRY( js::eval< uint32_t >( script ) ) );

    rv = outcome::success();

    return rv;
}

} // namespace kmap

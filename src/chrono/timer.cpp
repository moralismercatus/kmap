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
    auto& estore = kmap_.event_store();

    KMAP_TRY( estore.install_subject( "chrono.timer" ) );
    KMAP_TRY( estore.install_object( "chrono.second" ) );
    KMAP_TRY( estore.install_object( "chrono.minute" ) );
    KMAP_TRY( estore.install_object( "chrono.hour" ) );
    KMAP_TRY( estore.install_verb( "intervaled" ) );

    // TODO: More linguistic confusion between "source" and "object".
    //       I could use grammatical notions: subject (source), verb (action), object (thing being acted on)
    //       subject+object+verb for timer would be: timer.second+interval+at, or, "The seconds.timer [arrived] at the interval".
    //       I might actually like that way of decomposing the elemental parts.
    //       The kicker is that, I think, subject and object are swappable, in many cases, at least. Maybe not this case, but one could imagine:
    //       network woke keyboard, and keyboard woke network. ("woke" as in, maybe, made a light come on or some such in keyboard and focus for network, e.g.)
    //       It would therefore seem that the "things" can be either subject or object, so one ends up with some redundancy, potentially:
    //       subject.[network,keyboard] and object.[network,keyboard]. I suppose that's fine for now.
    KMAP_TRY( install_timer( "[ 'subject.chrono.timer', 'verb.intervaled', 'object.chrono.second' ]", 1000 ) ); // English: Timer [was] intervaled [for] second
    KMAP_TRY( install_timer( "[ 'subject.chrono.timer', 'verb.intervaled', 'object.chrono.minute' ]", 60000 ) ); // English: Timer [was] intervaled [for] minute
    KMAP_TRY( install_timer( "[ 'subject.chrono.timer', 'verb.intervaled', 'object.chrono.hour' ]", 360000 ) ); // English: Timer [was] intervaled [for] hour

    // Does this make sense: The ctrl key was depressed [for] network. It's a bit clunky, but it actually... works.
    // I suppose I should try this system. See if it works.
    // KMAP_TRY( install_event( "[ 'subject.keyboard.key.ctrl', 'verb.depressed', 'object.network' ]", 1000 ) ); // English: key.ctrl [was] depressed [for] network 
    // KMAP_TRY( install_event( "[ 'subject.keyboard.key.ctrl', 'verb.released', 'object.network' ]", 1000 ) );
    // KMAP_TRY( install_event( "[ 'subject.program', 'verb.exited' ]", 1000 ) ); // English: Program was exited

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

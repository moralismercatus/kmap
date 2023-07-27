/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/event/event_clerk.hpp>
#include <common.hpp>
#include <component.hpp>

namespace kmap::com::window {

/**
 * It should be noted that subdivisions, and their hierarchies, are represented as nodes, so all actions except for "update_*" change the node
 * description, and not the HTML itself. It is for "update" to apply these representations to the HTML.
 * 
 */
class Event : public Component
{
    EventClerk eclerk_;

public:
    static constexpr auto id = "window.event";
    constexpr auto name() const -> std::string_view override { return id; }

    Event( Kmap& km
          , std::set< std::string > const& requisites
          , std::string const& description )
        : Component{ km, requisites, description }
        , eclerk_{ km, { Event::id } }
    {
        KM_RESULT_PROLOG();

        KTRYE( register_standard_outlets() );
    }
    virtual ~Event() = default;

    auto initialize()
        -> Result< void > override
    {
        KM_RESULT_PROLOG();
        
        auto rv = result::make_result< void >();

        KTRY( eclerk_.install_registered() );

        rv = outcome::success();

        return rv;
    }
    auto load()
        -> Result< void > override
    {
        KM_RESULT_PROLOG();
        
        auto rv = result::make_result< void >();

        KTRY( eclerk_.check_registered() );

        rv = outcome::success();

        return rv;
    }

    auto register_standard_outlets()
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto rv = result::make_result< void >();
        
        // window.update_title_on_database_connection
        {
            auto const action = 
R"%%%(
const ostore = kmap.option_store();
const optn = ktry( kmap.fetch_node( '/meta.setting.option.window.title' ) );

ktry( ostore.apply( optn ) );
)%%%";
            eclerk_.register_outlet( Leaf{ .heading = "window.update_title_on_database_connection"
                                         , .requisites = { "subject.database", "verb.opened", "object.database_connection" }
                                         , .description = "includes file path on DB connection"
                                         , .action = action } );
        }

        rv = outcome::success();

        return rv;
    }
};

namespace {
namespace window_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::window::Event
,   std::set({ "event_store"s })
,   "event handling for window"
);

} // namespace window_def 
} // namespace anon

} // kmap::com::window

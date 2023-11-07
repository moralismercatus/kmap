/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/option/option_clerk.hpp>
#include <common.hpp>
#include <component.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#endif // !KMAP_NATIVE

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kmap::com::window {

/**
 * It should be noted that subdivisions, and their hierarchies, are represented as nodes, so all actions except for "update_*" change the node
 * description, and not the HTML itself. It is for "update" to apply these representations to the HTML.
 * 
 */
class Option : public Component
{
    OptionClerk oclerk_;

public:
    static constexpr auto id = "window.option";
    constexpr auto name() const -> std::string_view override { return id; }

    Option( Kmap& km
          , std::set< std::string > const& requisites
          , std::string const& description )
        : Component{ km, requisites, description }
        , oclerk_{ km }
    {
        KM_RESULT_PROLOG();

        KTRYE( register_standard_options() );
    }
    virtual ~Option()
    {
        // Undo title setting.
        auto const res = js::eval_void( "document.title = 'Knowledge Map [loading]';" );
        (void)res;
    }

    auto initialize()
        -> Result< void > override
    {
        KM_RESULT_PROLOG();
        
        auto rv = result::make_result< void >();

        KTRY( oclerk_.install_registered() );
        KTRY( oclerk_.apply_installed() );

        rv = outcome::success();

        return rv;
    }
    auto load()
        -> Result< void > override
    {
        KM_RESULT_PROLOG();
        
        auto rv = result::make_result< void >();

        KTRY( oclerk_.check_registered() );
        KTRY( oclerk_.apply_installed() );

        rv = outcome::success();

        return rv;
    }

    auto register_standard_options()
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto rv = result::make_result< void >();
        
        // window.title
        {
            auto const action = 
R"%%%(
const version = kmap.version();
const build = kmap.build_type();
const fpath = kmap.database().path();

if( fpath.has_value() )
{
    document.title = `Knowledge Map ${version} [${build}] - [${fpath.value()}]`;
}
else
{
    document.title = `Knowledge Map ${version} [${build}]`;
}
)%%%";
            KTRY( oclerk_.register_option( { .heading = "window.title"
                                           , .descr = "sets title for kmap window"
                                           , .value = "null"
                                           , .action = action } ) );
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
    kmap::com::window::Option
,   std::set({ "option_store"s })
,   "options for window"
);

} // namespace window_def 
} // namespace anon

} // kmap::com::window

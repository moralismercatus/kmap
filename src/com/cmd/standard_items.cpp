/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/cmd/cclerk.hpp>

namespace kmap::com {

/**
 * It should be noted that subdivisions, and their hierarchies, are represented as nodes, so all actions except for "update_*" change the node
 * description, and not the HTML itself. It is for "update" to apply these representations to the HTML.
 * 
 */
class StandardItems : public Component
{
    CommandClerk cclerk_;

public:
    static constexpr auto id = "command.standard_items";
    constexpr auto name() const -> std::string_view override { return id; }

    StandardItems( Kmap& km
                 , std::set< std::string > const& requisites
                 , std::string const& description )
        : Component{ km, requisites, description }
        , cclerk_{ km }
    {
        KM_RESULT_PROLOG();

        KTRYE( register_guards() );
        KTRYE( register_arguments() );
    }
    virtual ~StandardItems() = default;

    auto initialize()
        -> Result< void > override
    {
        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( void );

        KTRY( cclerk_.install_registered() );

        rv = outcome::success();

        return rv;
    }
    auto load()
        -> Result< void > override
    {
        KM_RESULT_PROLOG();

        auto rv = KMAP_MAKE_RESULT( void );

        KTRY( cclerk_.check_registered() );

        rv = outcome::success();

        return rv;
    }

    auto register_guards()
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto rv = result::make_result< void >();

        // unconditional
        {
            auto const guard_code =
            R"%%%(
                /* Nothing to do. */
            )%%%";

            auto const description = "without environmental constraints";

            KTRY( cclerk_.register_guard( Guard{ .path = "unconditional"
                                               , .description = description
                                               , .action = guard_code } ) );
        }

        rv = outcome::success();

        return rv;
    }

    auto register_arguments()
        -> Result< void >
    {
        KM_RESULT_PROLOG();

        auto rv = result::make_result< void >();

        // arg.unconditional
        {
            auto const guard_code =
            R"%%%(
                /* Nothing to do. */
            )%%%";
            auto const completion_code =
            R"%%%(
                let rv = new kmap.VectorString();

                rv.push_back( arg );

                return rv;
            )%%%";

            auto const description = "any valid text";

            KTRY( cclerk_.register_argument( Argument{ .path = "unconditional"
                                                     , .description = description
                                                     , .guard = guard_code
                                                     , .completion = completion_code } ) );
        }
        // arg.filesystem_path // TODO: I suspect that this belongs in a "filesystem" component.
        {
            auto const guard_code =
            R"%%%(
                /* Nothing to do. */
            )%%%";
            auto const completion_code =
            R"%%%(
                return kmap.complete_filesystem_path( arg );
            )%%%";

            auto const description = "filesystem path";

            KTRY( cclerk_.register_argument( Argument{ .path = "filesystem_path"
                                                     , .description = description
                                                     , .guard = guard_code
                                                     , .completion = completion_code } ) );
        }
        // arg.heading_path
        {
            auto const guard_code =
            R"%%%(
                /* Nothing to do. */
            )%%%";
            auto const completion_code =
            R"%%%(
                return kmap.complete_heading_path( arg );
            )%%%";

            auto const description = "heading path";

            KTRY( cclerk_.register_argument( Argument{ .path = "heading_path"
                                                     , .description = description
                                                     , .guard = guard_code
                                                     , .completion = completion_code } ) );
        }

        rv = outcome::success();

        return rv;
    }
};

} // kmap::com

namespace {
namespace cmd_std_items_def {

using namespace std::string_literals;

REGISTER_COMPONENT
(
    kmap::com::StandardItems
,   std::set({ "command.store"s })
,   "provides standard command items such as guards"
);

} // namespace cmd_std_items_def 
}

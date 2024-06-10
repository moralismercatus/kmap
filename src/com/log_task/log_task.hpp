/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/cmd/cclerk.hpp>
#include <com/event/event_clerk.hpp>
#include <component.hpp>
#include <kmap.hpp>

namespace kmap::com {

// TODO: So... the gist here is that there is Log and Task, and there's no interdependency between them, though they do publish events about their doings.
//       Rather, there's a 3rd party that performs the interplay, "bridges" them. That is what this is trying to achieve. It consumes the events and dispatches
//       to each. Maybe the "bridge" concept isn't even the right approach. Maybe individual outlets are the way to go, but that is what is ultimately installed.
//       The bridge is just a place to gather the functionality into a file.
//       Part of the gist is avoiding needing to alter Kmap.hpp/.cpp code to be aware of this component. An idea is to use something like the command registration.
//       The other requirement is for dependencies. Basically, this has other component dependencies. If that can somehow be communicated, then we won't hit the continual
//       uninitialized dep problem.
// TODO: A thought on how to achieve dependency resolution is through event system. Basically, when each component is initialized, it fires an event notifying its init-completion.
//       Other dependent components await requisite event notifications. For the possibility that a component isn't listening before a component fires init'd, some kind of
//       state would need to be maintained: if requisites already fired, init this component, otherwise, wait until all requisites fired.
//       This would allow for a dynamic means of handling component dependencies. It would also mean that _at least_ `EventStore` would need to be a common dependency.
//       I.e., EventStore is initialized before dynamic components.
//       A component might look something like: if req components fired, then init, else, create outlet for reqs that will call this component to be initialized.
//       It's a little complicated, because it's not like all req components are fired at the same time; however, it may be possible to make it work:
//       An outlet that listens for { "subject.component", "verb.commissioned" }, so any init'd fired will trigger. (following: { "verb.decommissioned" } on dtor)
//       Assuming I can integrate the payload mechanism, the payload can contain the ID of the component intialized, and store it somewhere, in a component store of some sort.
//       Then, each component can query this store on creation to see if it can be initialized at the outset, or listen for { "subject.component", "verb.commissioned" }
//       to inspect the component store for requisites. Outlet for ComponentStore must precede component outlets.
struct LogTask : public Component
{
    static constexpr auto id = "log_task";
    constexpr auto name() const -> std::string_view override { return id; }

    com::EventClerk eclerk_;
    com::CommandClerk cclerk_;

    LogTask( Kmap& kmap
           , std::set< std::string > const& requisites
           , std::string const& description );
    virtual ~LogTask() = default;

    auto initialize()
        -> Result< void > override;
    auto load()
        -> Result< void > override;
    auto register_standard_commands()
        -> Result< void >;
    auto register_standard_events()
        -> Result< void >;
    auto push_task_to_daily_log( Uuid const& task )
        -> Result< void >;
    auto push_active_tasks_to_log()
        -> Result< void >;
};

} // namespace kmap::com

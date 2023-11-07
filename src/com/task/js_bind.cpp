/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/task/task.hpp>

#include <emscripten/bind.h>

namespace kmap::com::binding {

struct TaskStore
{
    kmap::Kmap& km;

    TaskStore( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }

    auto cascade_tags( kmap::Uuid const& task )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->cascade_tags( task );
    }
    auto create_task( std::string const& title )
        -> kmap::Result< kmap::Uuid >
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->create_task( title );
    }
    auto create_subtask( kmap::Uuid const& supertask
                       , std::string const& title )
        -> kmap::Result< kmap::Uuid >
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->create_subtask( supertask, title );
    }
    auto close_task( kmap::Uuid const& task )
                   
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->close_task( task );
    }
    auto activate_task( kmap::Uuid const& task )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.component_store().fetch_component< kmap::com::TaskStore >() )->activate_task( task );
    }
    auto deactivate_task( kmap::Uuid const& task )
                   
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.component_store().fetch_component< kmap::com::TaskStore >() )->deactivate_task( task );
    }
    auto is_task( kmap::Uuid const& task )
        -> bool
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.component_store().fetch_component< kmap::com::TaskStore >() )->is_task( task );
    }
};

auto task_store()
    -> binding::TaskStore
{
    return binding::TaskStore{ kmap::Singleton::instance() };
}


using namespace emscripten;

EMSCRIPTEN_BINDINGS( kmap_task_store )
{
    function( "task_store", &binding::task_store );

    class_< binding::TaskStore >( "TaskStore" )
        .function( "cascade_tags", &binding::TaskStore::cascade_tags )
        .function( "create_task", &binding::TaskStore::create_task )
        .function( "create_subtask", &binding::TaskStore::create_subtask )
        .function( "close_task", &binding::TaskStore::close_task )
        .function( "activate_task", &binding::TaskStore::activate_task )
        .function( "deactivate_task", &binding::TaskStore::deactivate_task )
        .function( "is_task", &binding::TaskStore::is_task )
        ;
}

} // namespace kmap::com
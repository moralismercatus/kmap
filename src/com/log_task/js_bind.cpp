/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/log_task/log_task.hpp>
#include <emscripten/bind.h>

using namespace emscripten;

namespace kmap::com {

namespace {

struct LT
{ 
    kmap::Kmap& km;

    LT( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }

    auto push_task_to_log()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.fetch_component< kmap::com::LogTask >() )->push_task_to_log();
    }
    auto push_active_tasks_to_log()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRYE( km.fetch_component< kmap::com::LogTask >() )->push_active_tasks_to_log();
    }
};

auto log_task()
    -> LT
{
    return LT{ kmap::Singleton::instance() };
}

} // namespace anonymous

EMSCRIPTEN_BINDINGS( kmap_com_log_task )
{
    function( "log_task", &kmap::com::log_task );

    class_< LT >( "LogTask" )
        .function( "push_task_to_log", &kmap::com::LT::push_task_to_log )
        .function( "push_active_tasks_to_log", &kmap::com::LT::push_active_tasks_to_log )
        ;
}

} // kmap::com
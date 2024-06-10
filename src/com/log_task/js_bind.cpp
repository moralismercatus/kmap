/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/log/log.hpp>
#include <com/log_task/log_task.hpp>
#include <com/network/network.hpp>
#include <path/node_view2.hpp>

#include <emscripten/bind.h>
#include <range/v3/view/take.hpp>

#include <sstream>

using namespace emscripten;

namespace rvs = ranges::views;

namespace kmap::com {

namespace {

struct LT
{ 
    kmap::Kmap& km;

    LT( kmap::Kmap& kmap )
        : km{ kmap }
    {
    }

    auto print_tasks( Uuid const& from
                    , Uuid const& to )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        auto const is_daily_log                 // "node" day
                                = view2::parent // month
                                | view2::parent // year
                                | view2::parent( view2::log::daily_log_root );

        KMAP_ENSURE( anchor::node( from ) | is_daily_log | act2::exists( km ), error_code::common::uncategorized );
        KMAP_ENSURE( anchor::node( to )   | is_daily_log | act2::exists( km ), error_code::common::uncategorized );
        // TODO: This should be enforced, but is_ordered() only impl.'d for siblings.
        //       Perhaps not enforced, but start and end would need to be aware of the order.
        // KMAP_ENSURE( is_ordered( km, from, to ), error_code::common::uncategorized );

        auto printed_tasks = UuidSet{};
        auto const dlogs = view2::log::daily_log_root
                         | view2::child // year
                         | view2::child // month
                         | view2::child // day
                         | view2::order
                         | act2::to_node_vec( km );
        auto started = false;
        auto ss = std::stringstream{};

        for( auto const& dlog : dlogs )
        {
            if( dlog == from )
            {
                started = true;
            }

            if( started )
            {
                auto const tas = anchor::node( dlog )
                               | view2::child( "task" )
                               | view2::alias
                               | view2::resolve
                               | act2::to_node_set( km );
                
                for( auto const& ta : tas )
                {
                    if( !printed_tasks.contains( ta ) )
                    {
                        ss << "\n---\n"
                           << "# " << KTRY( anchor::node( ta ) | act2::fetch_title( km ) )
                           << "\n\n";

                        if( auto const rbody = anchor::node( ta )
                                             | view2::child( "problem" )
                                             | act2::fetch_body( km )
                          ; rbody )
                        {
                            auto const& body = rbody.value();

                                ss << body;
                        }

                        ss << "\n\n";
                           
                        printed_tasks.emplace( ta );
                    }
                }
            }

            if( dlog == to )
            {
                break;
            }
        }

        auto const& dlr = KTRY( view2::log::daily_log_root | act2::fetch_node( km ) );
        auto const from_path = format_heading( KTRY( anchor::node( dlr )
                                                   | view2::desc( from )
                                                   | act2::abs_path_flat( km ) ) );
        auto const to_path = format_heading( KTRY( anchor::node( dlr )
                                                 | view2::desc( to )
                                                 | act2::abs_path_flat( km ) ) );
        auto const voutput_node = anchor::abs_root 
                                | view2::child( "meta" )
                                | view2::child( "ephemeral" )
                                | view2::child( fmt::format( "logged_tasks_{}_to_{}", from_path, to_path ) );
        
        if( voutput_node | act2::exists( km ) )
        {
            KTRY( voutput_node | act2::erase_node( km ) );
        }

        auto const output_node = KTRY( voutput_node | act2::create_node( km ) );
        auto const nw = KTRY( km.fetch_component< com::Network >() );

        KTRY( voutput_node
            | act2::update_body( km, ss.str() ) );
        KTRY( voutput_node
            | view2::alias_src( printed_tasks )
            | act2::create( km ) );

        KTRY( nw->select_node( output_node ) );

        return outcome::success();
    }
    auto push_task_to_daily_log( Uuid const& task )
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::LogTask >() )->push_task_to_daily_log( task );
    }
    auto push_active_tasks_to_log()
        -> kmap::Result< void >
    {
        KM_RESULT_PROLOG();

        return KTRY( km.fetch_component< kmap::com::LogTask >() )->push_active_tasks_to_log();
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
        .function( "print_tasks", &kmap::com::LT::print_tasks )
        .function( "push_task_to_daily_log", &kmap::com::LT::push_task_to_daily_log )
        .function( "push_active_tasks_to_log", &kmap::com::LT::push_active_tasks_to_log )
        ;
}

} // kmap::com
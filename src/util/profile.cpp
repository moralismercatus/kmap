/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <util/profile.hpp>

#include <util/result.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#endif // !KMAP_NATIVE

#include <boost/timer/timer.hpp>
#include <fmt/format.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/range/conversion.hpp>

#include <map>
#include <optional>
#include <deque>
#include <string>

namespace
{
    auto const one_second_in_ns = boost::timer::nanosecond_type{ 1000000000LL };
    auto const profiling_duration = 120 * one_second_in_ns;
    auto const top_fn_display_count = 100;
    std::map< uint32_t, boost::timer::nanosecond_type > fn_runtime = {};
    std::deque< std::pair< uint64_t, boost::timer::cpu_timer > > call_stack;
    auto do_profiling = false;
    auto overall_timer = boost::timer::cpu_timer{};
} // namespace anonymous

namespace kmap {

auto enable_profiling()
    -> void
{
    fmt::print( "enabling profiling\n" );
    do_profiling = true;
}

} // namespace kmap

/**
 * ****Notes about profiling with -finstrument-functions****
 * 
 * Enabling: 
 *   - Configure `::profiling_duration` profile.cpp::profiling_duration to set how long the profiling should be done for.
 *   - Configure `::top_fn_display_count` profile::display_top_fn_count to set how many top function runtimes to display at profiling's end.
 *   - Profiling must be done in debug mode, else symbols will be missing.
 *   - Add `-finstrument-functions` to compile commands, and `-s USE_OFFSET_CONVERTER` to both compile and link commands.
 *   - Any time after `js::set_global_kmap();` is initialized, you will need to manually call `enable_profiling`
 * 
 * Overall Observations:
 *   - The general idea "works", as in it records times based on prolog/epilog elapsed times and prints them after a set duration.
 *     Whether the results are representative of the "real world" is less certain.
 *   - Mapping the function index to the function symbol proved a major challenge. The only way I successfully achieved it was to
 *     abort execution (`exit()`) and allow the console to "fill in" the symbol when printing the function index. Certainly a place for future improvement.
 *   - Enabling the prolog/epilog instrumentation seems to either cause such a severe slowdown or otherwise corrupts the flow that progress into the
 *     code is reduced, so how representative the result is, is also questionable.
 *   - There is a overhead bias for callers of deeper call depth. Here's how it occurs: the func_enter and func_exit processing itself takes time.
 *     At least some of this overhead is added to the runtime clocks for each caller which _shouldn't_ be counted towards runtime. I don't know a way around this problem.
 *     It seems no matter what, at least some level of meta-processing - that shouldn't be counted - is counted towards overhead for the callee ancestors.
 *     I have called halt() on all timers during func_exit processing, but I don't know if that helps or harms the problem, actually, because halting itself takes time.
 */
extern "C" 
{
    __attribute__((no_instrument_function))
    void __cyg_profile_func_enter(void *this_fn, void *call_site);
    __attribute__((no_instrument_function))
    void __cyg_profile_func_exit(void *this_fn, void *call_site);
}

__attribute__((no_instrument_function))
void __cyg_profile_func_enter( void *this_fn
                             , void *call_site ) 
{
    if( do_profiling )
    {
        do_profiling = false;

        call_stack.push_back( { reinterpret_cast< uint64_t >( this_fn ), boost::timer::cpu_timer{} } );

        do_profiling = true;
    }

    return;
}

auto stack_trace()
    -> std::vector< std::string >
{
    KM_RESULT_PROLOG();

#if !KMAP_NATIVE
    return { KTRYE( kmap::js::eval< std::string >( "return stackTrace();" ) ) };
#else
    return {};
#endif // !KMAP_NATIVE
}

__attribute__((no_instrument_function))
void __cyg_profile_func_exit( void *this_fn
                            , void *call_site ) 
{
    if( do_profiling )
    {
        do_profiling = false;

        if( !call_stack.empty() )
        {
            // Minimize overhead bias caused by func_exit processing.
            for( auto&& item : call_stack )
            {
                item.second.stop();
            }

            // Record fn time
            {
                auto const& [ cs_fid, cs_timer ] = call_stack.back(); assert( cs_fid == (uint64_t)this_fn );

                if( !fn_runtime.contains( cs_fid ) )
                {
                    fn_runtime[ cs_fid ] = cs_timer.elapsed().wall;
                }
                else
                {
                    fn_runtime[ cs_fid ] += cs_timer.elapsed().wall;
                }
            }

            // Check overall duration
            {
                auto const ote = overall_timer.elapsed();

                if( ote.wall > ::profiling_duration )
                {
                    fmt::print( "total elapsed time > interval\n" );
                    // stack_trace();
                    auto const sorted = fn_runtime
                                    | ranges::to< std::vector< std::pair< uint32_t, boost::timer::nanosecond_type > > >()
                                    | ranges::actions::sort( []( auto const& lhs, auto const& rhs ){ return lhs.second > rhs.second; } );
                    for( auto const& [ k, v ] : sorted
                                              | ranges::views::take( ::top_fn_display_count ) )
                    {
                        fmt::print( "fn_index: '{}', time: {{ '{}ns', {}s }}\n", k, v, ( v / double{ ::one_second_in_ns } ) );

#if !KMAP_NATIVE
                        EM_ASM(
                        {
                            console.log( getWasmTableEntry( $0 ) );
                        }
                        , k );
#endif // !KMAP_NATIVE
                    }
                    overall_timer = boost::timer::cpu_timer{};
                    // Note: For whatever reason, unless I exit, the console won't print the function names for `console.log( getWasmTableEntry( $0 ) )`. Very strange.
                    std::exit( 1 );
                }
            }

            for( auto&& item : call_stack )
            {
                item.second.resume();
            }

            call_stack.pop_back();
        }

        do_profiling = true;
    }

    return;
}
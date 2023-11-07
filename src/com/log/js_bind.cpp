/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <com/log/log.hpp>

#include <emscripten/bind.h>

namespace kmap::com {
namespace binding {

using namespace emscripten;

EMSCRIPTEN_BINDINGS( kmap_log )
{
    function( "present_date_string", &kmap::com::present_date_string );
    function( "present_daily_log_path", &kmap::com::present_daily_log_path );
}

} // namespace binding
} // namespace kmap::com

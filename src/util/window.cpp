/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include <util/window.hpp>

#include <common.hpp>
#include <io.hpp>
#include <util/result.hpp>

#if !KMAP_NATIVE
#include <js/iface.hpp>
#include <emscripten.h>
#include <emscripten/val.h>
#endif // !KMAP_NATIVE

namespace kmap::window {

auto inner_width()
    -> uint32_t
{
    KM_RESULT_PROLOG();

#if !KMAP_NATIVE
    return KTRYE( js::eval< uint32_t >( "return window.innerWidth;" ) ) * 0.995f; // Note: innerWidth results in value that's slightly too large.
#else
    return uint32_t{};
#endif
}

auto inner_height()
    -> uint32_t
{
    KM_RESULT_PROLOG();

#if !KMAP_NATIVE
    return KTRYE( js::eval< uint32_t >( "return window.innerHeight;" ) ) * 0.995f; // Note: innerWidth results in value that's slightly too large.
#else
    return uint32_t{};
#endif
}

} // namespace kmap::window
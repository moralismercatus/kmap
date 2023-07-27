/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "window.hpp"

#include "common.hpp"
#include "js_iface.hpp"
#include "io.hpp"

#include <emscripten.h>
#include <emscripten/val.h>

namespace kmap::window {

auto inner_width()
    -> uint32_t
{
    using emscripten::val;

    return val::global().call< val >( "eval", std::string{ "window.innerWidth" } ).as< uint32_t >() * 0.995f; // Note: innerWidth results in value that's slightly too large.
}

auto inner_height()
    -> uint32_t
{
    using emscripten::val;

    return val::global().call< val >( "eval", std::string{ "window.innerHeight" } ).as< uint32_t >() * 0.995f; // Note: innerHeight results in value that's slightly too large.
}

} // namespace kmap::window
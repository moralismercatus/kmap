/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "environment.hpp"

#include "contract.hpp"
#include "io.hpp"

#include <emscripten.h>

namespace kmap
{

Environment::Environment()
{
}

Environment::Environment( FsPath const& p )
    : root_node_id_{}
    , db_{ p }
{
}

auto Environment::database()
    -> Database&
{
    return db_;
}

auto Environment::database() const
    -> Database const&
{
    return db_;
}

auto Environment::root_node_id() const
    -> Uuid const&
{
    return root_node_id_;
}

auto Environment::set_root( Uuid const& id )
    -> void
{
    root_node_id_ = id;
}

} // namespace kmap



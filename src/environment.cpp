/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#include "environment.hpp"

#include "contract.hpp"
#include "db.hpp"
#include "io.hpp"

#include <emscripten.h>

#include <memory>

namespace kmap
{

Environment::Environment()
    : root_node_id_{ gen_uuid() }
    // , db_{ std::make_unique< Database >( /*kmap_root_dir / gen_temp_db_name()*/ ) }
{
}

Environment::Environment( FsPath const& p )
    : root_node_id_{}
    // , db_{ std::make_unique< Database >() }
{
}

// auto Environment::database()
//     -> Database&
// {
//     return *db_;
// }

// auto Environment::database() const
//     -> Database const&
// {
//     return *db_;
// }

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



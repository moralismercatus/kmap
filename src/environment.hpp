/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_ENVIRONMENT_HPP
#define KMAP_ENVIRONMENT_HPP

#include "common.hpp"
#include "utility.hpp"
#include "db.hpp"
#include "network.hpp"
#include <boost/circular_buffer.hpp>

#include <stack>
#include <vector>

namespace kmap
{

class Environment
{
public:
    Environment();
    Environment( FsPath const& db_path );

    auto database()
        -> Database&;
    auto database() const
        -> Database const&;
    auto root_node_id() const
        -> Uuid const&;
    auto set_root( Uuid const& id )
        -> void;

private:
    Uuid root_node_id_ = gen_uuid();
    Database db_{ kmap_root_dir / gen_temp_db_name() };
};

} // namespace kmap

#endif // KMAP_ENVIRONMENT_HPP

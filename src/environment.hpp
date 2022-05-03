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
#include "network.hpp"

#include <boost/circular_buffer.hpp>

#include <memory>
#include <stack>
#include <vector>

namespace kmap {

class Database;

class Environment
{
public:
    Environment();
    Environment( FsPath const& db_path );

    // auto database()
    //     -> Database&;
    // auto database() const
    //     -> Database const&;
    auto root_node_id() const
        -> Uuid const&;
    auto set_root( Uuid const& id )
        -> void;

private:
    Uuid root_node_id_;
    // std::unique_ptr< Database > db_; // Temporarily moving db to kmap, to have more fine-grained control. Will probably move back, along with other "environmental" features.
};

} // namespace kmap

#endif // KMAP_ENVIRONMENT_HPP

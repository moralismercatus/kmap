/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_PROJECT_HPP
#define KMAP_CMD_PROJECT_HPP

#include "com/cli/cli.hpp"
#include "../common.hpp"

#include <functional>

namespace kmap {

class Kmap;

}

namespace kmap::cmd {

enum class ProjectCategory
{
    inactive
,   active
,   closed
};

auto open_task( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto close_task( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto activate_project( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;
auto deactivate_project( Kmap& kmap )
    -> std::function< Result< std::string >( com::CliCommand::Args const& args ) >;

auto create_project( Kmap& kmap
                   , Title const& title )
    -> Result< Uuid >;
auto create_project( Kmap& kmap
                   , Uuid const& parent
                   , Title const& title )
    -> Result< Uuid >;
auto fetch_projects_root( Kmap const& kmap )
    -> Result< Uuid >;
auto fetch_project_root( Kmap const& kmap
                       , Lineal const& node )
    -> Result< Uuid >;
auto fetch_project_root( Kmap const& kmap
                       , Uuid const& node )
    -> Result< Uuid >;
auto fetch_appropriate_project_parent( Kmap& kmap
                                     , Uuid const& node )
    -> Uuid;
auto is_in_project( Kmap const& kmap
                  , Uuid const& node )
    -> bool;
auto is_project( Kmap const& kmap
               , Uuid const& node )
    -> bool;
auto update_project_status( Kmap& kmap
                          , Uuid const& project
                          , ProjectCategory const& cat )
    -> Result< Uuid >;

} // namespace kmap::cmd

#endif // KMAP_CMD_PROJECT_HPP

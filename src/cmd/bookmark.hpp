/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_CMD_BOOKMARK_HPP
#define KMAP_CMD_BOOKMARK_HPP

#include <string>

namespace kmap {
class Kmap;
}

namespace kmap::cmd {

inline auto const bookmarks_root = std::string{ "/meta.bookmarks" };

} // namespace kmap::cmd

#endif // KMAP_CMD_BOOKMARK_HPP

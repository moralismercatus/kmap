/******************************************************************************
 * Author(s): Christopher J. Havlicek
 *
 * See LICENSE and CONTACTS.
 ******************************************************************************/
#pragma once
#ifndef KMAP_TEST_UTIL_HPP
#define KMAP_TEST_UTIL_HPP

#include "canvas.hpp"
#include "js_iface.hpp"

#include <boost/filesystem/path.hpp>

namespace kmap::test {

struct BlankStateFixture
{
	std::string file;
	uint32_t line;

	BlankStateFixture( std::string const& curr_file 
	                 , uint32_t const curr_line );
	~BlankStateFixture();
};

struct SaveToDiskFixture
{
    boost::filesystem::path file_path;
	Database& db;
	std::string file;
	uint32_t line;

	SaveToDiskFixture( Database& d
	                 , std::string const& curr_file 
	                 , uint32_t const curr_line );
	~SaveToDiskFixture();
};

} // namespace kmap::test

#define KMAP_BLANK_STATE_FIXTURE_SCOPED() kmap::test::BlankStateFixture blank_state_fixture{ __FILE__, __LINE__}
#define KMAP_INIT_DISK_DB_FIXTURE_SCOPED( db ) kmap::test::SaveToDiskFixture save_to_disk_fixture{ ( db ), __FILE__, __LINE__ }

#endif // KMAP_TEST_UTIL_HPP
cmake_minimum_required(VERSION 3.5)
project(LeagueLib)

set(LEAGUELIB_SOURCES "" CACHE INTERNAL "LeagueLib sources")
add_subdirectory("ext/spekfile")
add_subdirectory("ext/zlib")
add_subdirectory("ext/zstd/build/cmake")

set_target_properties(SpekFile PROPERTIES FOLDER Libraries)

set_target_properties(zlib PROPERTIES FOLDER CMakePredefinedTargets/zlib)
set_target_properties(zlibstatic PROPERTIES FOLDER CMakePredefinedTargets/zlib)
set_target_properties(uninstall PROPERTIES FOLDER CMakePredefinedTargets/zlib)

set_target_properties("clean-all" PROPERTIES FOLDER CMakePredefinedTargets/zstd)
set_target_properties(zstd PROPERTIES FOLDER CMakePredefinedTargets/zstd)
set_target_properties(libzstd_shared PROPERTIES FOLDER CMakePredefinedTargets/zstd)
set_target_properties(libzstd_static PROPERTIES FOLDER CMakePredefinedTargets/zstd)

ADD_SRC(LEAGUELIB_SOURCES	"Util"						"inc/league_lib/util/enum_bitfield.hpp"				"")

ADD_SRC(LEAGUELIB_SOURCES	"Bin"						"inc/league_lib/bin/bin.hpp"						"src/bin/bin.cpp")
ADD_SRC(LEAGUELIB_SOURCES	"BinValueStorage"			"inc/league_lib/bin/bin_valuestorage.hpp"			"src/bin/bin_valuestorage.cpp")
# ADD_SRC(LEAGUELIB_SOURCES	"BinParser"					"inc/league_lib/bin/bin_parser.hpp"					"src/bin/bin_parser.cpp")

ADD_SRC(LEAGUELIB_SOURCES	"WAD"						"inc/league_lib/wad/wad.hpp"						"src/wad/wad.cpp")
ADD_SRC(LEAGUELIB_SOURCES	"WADFS"						"inc/league_lib/wad/wad_filesystem.hpp"				"src/wad/wad_filesystem.cpp")

ADD_SRC(LEAGUELIB_SOURCES	"NavGrid"					"inc/league_lib/navgrid/navgrid.hpp"				"src/navgrid/navgrid.cpp")

add_library(LeagueLib ${LEAGUELIB_SOURCES})
target_link_libraries(LeagueLib SpekFile zlibstatic libzstd_static)

target_include_directories(LeagueLib PUBLIC "inc")
target_include_directories(LeagueLib PUBLIC "ext/glm")
target_include_directories(LeagueLib PUBLIC "ext")

target_include_directories(LeagueLib PRIVATE "src")
target_include_directories(LeagueLib PRIVATE "ext/zlib")
target_include_directories(LeagueLib PRIVATE "ext/zstd/lib")
target_include_directories(LeagueLib PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/ext/zlib")

set_target_properties(LeagueLib PROPERTIES 
					VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
					CXX_STANDARD 17
					CXX_EXTENSIONS OFF)

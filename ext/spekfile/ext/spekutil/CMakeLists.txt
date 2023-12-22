cmake_minimum_required(VERSION 3.17)
project(SpekUtil)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
function(ADD_SRC var folder inc src)
	source_group(${folder} FILES ${inc} ${src})
	set(${var} ${${var}} ${inc} ${src} CACHE INTERNAL "")
endfunction(ADD_SRC)

# Source files
set(SPEKUTIL_SOURCES "" CACHE INTERNAL "SpekUtil source files")
ADD_SRC(SPEKUTIL_SOURCES	"Util\\CallbackContainer"			"inc/spek/util/callback_container.hpp"			"")
ADD_SRC(SPEKUTIL_SOURCES	"Util\\Types"						"inc/spek/util/types.hpp"						"")
ADD_SRC(SPEKUTIL_SOURCES	"Util\\Duration"					"inc/spek/util/duration.hpp"					"src/util/duration.cpp")
ADD_SRC(SPEKUTIL_SOURCES	"Util\\Debug"						"inc/spek/util/debug.hpp"						"src/util/debug.cpp")
ADD_SRC(SPEKUTIL_SOURCES	"Util\\Hash"						"inc/spek/util/hash.hpp"						"src/util/hash.cpp")

add_library(SpekUtil ${SPEKUTIL_SOURCES})
target_compile_definitions(SpekUtil PUBLIC SPEKUTIL_LIBRARY)

target_include_directories(SpekUtil PUBLIC "inc")
target_include_directories(SpekUtil PRIVATE "src")

set_target_properties(SpekUtil PROPERTIES 
						VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
						CXX_STANDARD 20
						CXX_EXTENSIONS OFF)

if (WIN32)
	target_compile_definitions(SpekUtil PUBLIC WIN32_LEAN_AND_MEAN)
	target_compile_definitions(SpekUtil PUBLIC NOMINMAX)
	target_compile_definitions(SpekUtil PUBLIC SPEK_WINDOWS)

else (EMSCRIPTEN)
	target_compile_definitions(SpekUtil PUBLIC SPEK_EMSCRIPTEN)

endif ()
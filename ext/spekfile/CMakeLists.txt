cmake_minimum_required(VERSION 3.17)
project(SpekFile)


set_property(GLOBAL PROPERTY USE_FOLDERS ON)
function(ADD_SRC var folder inc src)
	source_group(${folder} FILES ${inc} ${src})
	set(${var} ${${var}} ${inc} ${src} CACHE INTERNAL "")
endfunction(ADD_SRC)

# Source files
set(SPEKFILE_SOURCES "" CACHE INTERNAL "SpekFile source files")
ADD_SRC(SPEKFILE_SOURCES	"File\\System"				"inc/spek/file/base_filesystem.hpp"					"src/file/base_filesystem.cpp")
ADD_SRC(SPEKFILE_SOURCES	"File\\System\\Default"		"inc/spek/file/default_filesystem.hpp"				"src/file/default_filesystem.cpp")
ADD_SRC(SPEKFILE_SOURCES	"File\\System\\Default"		"src/file/default_filesystem_emscripten.cpp"		"src/file/default_filesystem_windows.cpp")
ADD_SRC(SPEKFILE_SOURCES	"File"						"inc/spek/file/file.hpp"							"src/file/file.cpp")

add_subdirectory("ext/spekutil")
set_target_properties(SpekUtil PROPERTIES FOLDER Libraries)

add_library(SpekFile ${SPEKFILE_SOURCES})
target_compile_definitions(SpekFile PUBLIC SPEKFILE_LIBRARY)
target_link_libraries(SpekFile SpekUtil)

target_include_directories(SpekFile PUBLIC "inc")
target_include_directories(SpekFile PRIVATE "src")

set_target_properties(SpekFile PROPERTIES 
						VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
						CXX_STANDARD 20
						CXX_EXTENSIONS OFF)

if (WIN32)
	target_compile_definitions(SpekFile PUBLIC WIN32_LEAN_AND_MEAN)
	target_compile_definitions(SpekFile PUBLIC NOMINMAX)
	target_compile_definitions(SpekFile PUBLIC SPEK_WINDOWS)

else (EMSCRIPTEN)
	target_compile_definitions(SpekFile PUBLIC SPEK_EMSCRIPTEN)

endif ()
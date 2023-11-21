file(GLOB_RECURSE NAM_SOURCES ../NAM/*.cpp ../NAM/*.c ../NAM*.h ./dsp/*.cpp ../dsp/*.c ../dsp*.h)

set(TOOLS benchmodel)

add_custom_target(tools ALL
	DEPENDS ${TOOLS})

find_package(Eigen3 REQUIRED)

include_directories(tools ..)
include_directories(tools ${NAM_DEPS_PATH}/eigen)
include_directories(tools ${NAM_DEPS_PATH}/nlohmann)

add_executable(loadmodel loadmodel.cpp ${NAM_SOURCES})
add_executable(benchmodel benchmodel.cpp ${NAM_SOURCES})
add_executable(reamp reamp.cpp ${NAM_SOURCES})

# needed for macOS
target_link_libraries(loadmodel Eigen3::Eigen)
target_link_libraries(benchmodel Eigen3::Eigen)
target_link_libraries(reamp Eigen3::Eigen)

source_group(NAM ${CMAKE_CURRENT_SOURCE_DIR} FILES ${NAM_SOURCES})

target_compile_features(${TOOLS} PUBLIC cxx_std_17)

set_target_properties(${TOOLS}
	PROPERTIES
	CXX_VISIBILITY_PRESET hidden
	INTERPROCEDURAL_OPTIMIZATION TRUE
	PREFIX ""
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	target_compile_definitions(${TOOLS} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (MSVC)
	target_compile_options(${TOOLS} PRIVATE
		"$<$<CONFIG:DEBUG>:/W4>"
		"$<$<CONFIG:RELEASE>:/O2>"
	)
else()
	target_compile_options(${TOOLS} PRIVATE
		-Wall -Wextra -Wpedantic -Wstrict-aliasing -Wunreachable-code -Weffc++ -Wno-unused-parameter
		"$<$<CONFIG:DEBUG>:-Og;-ggdb;-Werror>"
		"$<$<CONFIG:RELEASE>:-Ofast>"
	)
endif()
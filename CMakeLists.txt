project( synthything )
cmake_minimum_required( VERSION 3.1 )

set( CMAKE_MODULE_PATH 
	${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake-modules 
)

find_package( Jack )
find_package( Threads )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED TRUE )

include_directories( include )

add_executable( synthything 
	src/main.cpp

	src/engine.cpp
	src/jack.cpp

	include/audio_interface.hpp
	include/engine.hpp
	include/jack.hpp
)

target_link_libraries( synthything 
	fmt
	${JACK_LIBRARIES}
	Threads::Threads
)
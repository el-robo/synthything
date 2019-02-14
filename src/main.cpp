#include "engine.hpp"
#include "jack.hpp"

#include <iostream>
#include <exception>

#define FMT_STRING_ALIAS 1
#include <fmt/format.h>

constexpr auto project_name = "SynthyThing";

int main( int argc, char **argv )
{
	fmt::print( "Starting {}\n", project_name );

	try
	{
		audio::jack::engine audio( project_name );

	}
	catch( std::exception &e )
	{
		fmt::print( "Unhandled exception: {}\n", e.what() );
	}

	return 0;
}
#include "engine.hpp"
#include "jack.hpp"

#include <iostream>
#include <exception>
#include <stdio.h>

#define FMT_STRING_ALIAS 1
#include <fmt/format.h>

using namespace std::chrono_literals;

constexpr auto project_name = "SynthyThing";

int main( int argc, char **argv )
{
	fmt::print( "Starting {}\n", project_name );

	try
	{
		audio::jack::interface audio( project_name );
		synth::engine engine( audio );

		bool running = true;
		
		while( running )
		{
			std::this_thread::sleep_for( 10ms );
		}
	}
	catch( std::exception &e )
	{
		fmt::print( "Unhandled exception: {}\n", e.what() );
	}

	return 0;
}
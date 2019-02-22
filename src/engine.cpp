#include "engine.hpp"
#include "generator.hpp"

#include <future>
#include <algorithm>
#include <numeric>
#include <math.h>
#include <iostream>

using namespace synth;

void run_engine( engine::implementation & );

struct engine::implementation
{
	audio::interface &audio;
	std::atomic<bool> running;
	std::future<void> job;

	implementation( audio::interface &interface ) :
		audio( interface ),
		running( true ),
		job( std::async( std::launch::async, [ this ](){ run_engine( *this ); } ) )
	{
	}

	~implementation()
	{
		running = false;
		job.wait();
	}
};

engine::engine( audio::interface &interface ) :
	impl_( new implementation( interface ) )
{

}

engine::~engine()
{
}

void engine::process()
{
}

std::vector< audio::buffer > get_buffers( audio::frame &frame )
{
	std::vector< audio::buffer > result;

	if( frame.recycled_buffers.size() == frame.channel_count )
	{
		std::swap( frame.recycled_buffers, result );
	}
	else
	{
		result.resize( frame.channel_count );
	}
	
	std::for_each( result.begin(), result.end(), [ & ]( auto &buffer )
	{
		buffer.resize( frame.sample_count );
	} );

	return result;
}

constexpr auto pi = 3.1415926535897932384626433;
constexpr auto two_pi = pi * 2;

auto clip( double value )
{
	return std::max( -1.0, std::min( 1.0, value ) );
}

float sine( generator &g )
{
	const auto value = g.amplitude() * clip( sin( g.t ) );
	g.period = two_pi;
	return value;
}

float square( generator &g )
{
	return g.amplitude() * (sine( g ) > 0.0 ? 1.0 : -1.0);
}

float saw( generator &g )
{
	const auto value = (g.amplitude() * 2  * (g.t - std::floor( g.t )) ) - 1.0;
	// advance( g, 1.0 );
	return value;
}

using voice = std::vector< generator >;

template < typename Buffer >
void generate( Buffer &buffer, voice &v, double sample_rate )
{
	std::generate( buffer.begin(), buffer.end(), [ &v, sample_rate ]()
	{
		return std::accumulate( v.begin(), v.end(), 0.0, [ &sample_rate ]( double value, auto &generator )
		{
			value += generator();
			generator.advance( sample_rate );
			return value;
		} );
	} );
}

void run_engine( engine::implementation &impl )
{
	auto freq = 440.0;

	generator sw { saw, freq, 0.2 };
	generator sn { sine, freq, 0.2 };

	sn.mod.frequency.push_back( { std::plus<double>(), generator { sine, 2, 100 } } );
	sn.mod.frequency.push_back( { std::plus<double>(), generator { sine, 4, 100 } } );

	std::vector< voice > voices
	{
		{ sn },
		{ sn }
	};

	while( impl.running )
	{
		auto frame = impl.audio.next_frame();
		auto buffers = get_buffers( frame );

		auto buffer = buffers.begin();
		auto voice = voices.begin();

		while( buffer != buffers.end() && voice != voices.end() )
		{
			generate( *buffer++, *voice++, impl.audio.sample_rate() );
		}

		frame.promised_buffers.set_value( std::move( buffers ) );
	}
}

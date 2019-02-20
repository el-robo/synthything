#include "engine.hpp"
#include <future>
#include <algorithm>
#include <numeric>
#include <math.h>

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

constexpr auto pi = 3.1415926535897932384626433f;

auto clip( float value )
{
	return std::max( -1.0f, std::min( 1.0f, value ) );
}

struct generator
{
	using function = std::function< float( generator & ) >;

	const function generator;

	float frequency = 440.0f;
	float sample_rate = 48000.0;
	float t = 0.0f;

	const float period = (2*pi);
	const float amplitude = 0.2f;

	float operator()()
	{
		if( generator )
		{
			return generator( *this );
		}

		return 0.0f;
	}
};

float sine( generator &g )
{
	const auto value = g.amplitude * clip( sin( g.t ) );
	g.x += (g.period / g.sample_rate) * g.frequency;
	return value;
}

float square( generator &g )
{
	auto value = sine( g );
	return g.amplitude * (value > 0.0f ? 1.0f : -1.0f);
}

using voice = std::vector< generator >;

template < typename Buffer >
void generate( Buffer &buffer, voice &v )
{
	std::generate( buffer.begin(), buffer.end(), [ &v ]()
	{
		return std::accumulate( v.begin(), v.end(), 0.0f, []( auto &value, auto &generator )
		{
			return value + generator();
		} );
	} );
}

void run_engine( engine::implementation &impl )
{
	std::vector< voice > voices
	{
		{ { sine, 440.f }, { sine, 220.f } },
		{ { square } }
	};

	while( impl.running )
	{
		auto frame = impl.audio.next_frame();
		auto buffers = get_buffers( frame );

		auto buffer = buffers.begin();
		auto voice = voices.begin();

		while( buffer != buffers.end() && voice != voices.end() )
		{
			generate( *buffer++, *voice++ );
		}

		frame.promised_buffers.set_value( std::move( buffers ) );
	}
}

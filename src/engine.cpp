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
	float frequency = 440.0f;
	float sample_rate = 48000.0;
	float x = 0.0f;

	const float period = (2*pi);
	const float amplitude = 0.2f;

	float operator()()
	{
		const auto value = amplitude * clip( sin( x ) );
		x += (period / sample_rate) * frequency;
		return value;
	}
};

generator sine( float frequency = 440.0f, float sample_rate = 48000.0f )
{
	return
	{
		frequency,
		sample_rate
	};
}

template < typename Buffer, typename Voice >
void generate( Buffer &buffer, Voice &voice )
{
	std::generate( buffer.begin(), buffer.end(), std::ref( voice ) );
}

void run_engine( engine::implementation &impl )
{
	auto gen = sine();

	while( impl.running )
	{
		auto frame = impl.audio.next_frame();
		auto buffers = get_buffers( frame );
		
		if( !buffers.empty() )
		{
			generate( buffers.front(), gen );
		}

		frame.promised_buffers.set_value( std::move( buffers ) );
		frame.recycled_buffers.clear();
	}
}

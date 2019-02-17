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

using generator = std::function< float() >;

constexpr auto pi = 3.1415926535897932384626433f;

generator sine( float frequency = 440.0, float sample_rate = 48000.0 )
{
	return [ =, 
		sample = 0.0f, 
		period = (2*pi) / sample_rate,
		amplitude = 0.2
	]() mutable -> float
	{
		// auto value = sin( sample++ * (2*pi) * frequency * sample_rate) * 0.2;//0.0f;
		// return sin( sample++ / (sample_rate / (2*pi)) ) * 0.3;
		// sample += 0.01;
		// sample += period / frequency;
		return value;
	};
}

void run_engine( engine::implementation &impl )
{
	using voice = std::vector< generator >;
	// std::vector< voice > voices;

	// std::vector< generator> voices { sine() };//, sine() };

	auto gen = sine();

	while( impl.running )
	{
		auto frame = impl.audio.next_frame();
		auto buffers = get_buffers( frame );
		
		if( !buffers.empty() )
		{
			auto &buffer = buffers.front();
			for( auto &sample : buffer )
			{
				sample = gen();
			}

			for( auto i = 1; i < buffers.size(); ++i )
			{
				std::copy( buffer.begin(), buffer.end(), buffers[ i ].begin() );
			}

			// auto voice_it = voices.begin();
			// for( auto &buffer : buffers )
			// {
			// 	// if( voice_it == voices.end() )
			// 	// {
			// 	// 	voices.push_back( sine() );
			// 	// }

			// 	std::generate( buffer.begin(), buffer.end(), *voice_it );

			// 	// ++voice_it;
			// }
		}

		frame.promised_buffers.set_value( std::move( buffers ) );
	}
}

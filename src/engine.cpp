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

constexpr auto pi = 3.1415926535897932384626433;
constexpr auto two_pi = pi * 2;

auto clip( float value )
{
	return std::max( -1.0f, std::min( 1.0f, value ) );
}

struct generator
{
	using function = std::function< double( generator & ) >;
	const function wave_function;

	double frequency_ = 440.0;
	double amplitude_ = 0.1;
	double t = 0.0;
	double sample_rate_ = 480000.0;

	struct
	{
		std::function< double() > frequency;
		std::function< double() > amplitude;
	} mod;

	double frequency()
	{
		if( mod.frequency )
		{
			return frequency_ * mod.frequency();
		}

		return frequency_;
	}

	double amplitude()
	{
		if( mod.amplitude )
		{
			return amplitude_ * mod.amplitude();
		}

		return amplitude_;
	}

	double operator()()
	{
		if( wave_function )
		{
			return wave_function( *this );
		}

		return 0.0;
	}
};

void advance( generator &g, double period )
{
	g.t += (period / g.sample_rate_) * g.frequency();
}

float sine( generator &g )
{
	const auto value = g.amplitude() * clip( sin( g.t ) );
	advance( g, two_pi );
	return value;
}

float square( generator &g )
{
	return g.amplitude() * (sine( g ) > 0.0 ? 1.0 : -1.0);
}

float saw( generator &g )
{
	const auto value = g.amplitude() * (g.t - std::floor( g.t ));
	advance( g, 1.0 );
	return value;
}

using voice = std::vector< generator >;

template < typename Buffer >
void generate( Buffer &buffer, voice &v )
{
	std::generate( buffer.begin(), buffer.end(), [ &v ]()
	{
		return std::accumulate( v.begin(), v.end(), 0.0, []( double value, auto &generator )
		{
			return value + generator();
		} );
	} );
}

void set_sample_rate( std::vector< voice > &voices, double sample_rate )
{
	for( auto &voice : voices )
	{
		for( auto &gen : voice )
		{
			gen.sample_rate_ = sample_rate;
		}
	}
}

void run_engine( engine::implementation &impl )
{
	auto freq = 220.0;

	std::vector< voice > voices
	{
		{ { sine, freq } },
		{ { square, freq } }
		// { { square } }
	};

	set_sample_rate( voices, impl.audio.sample_rate() );

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

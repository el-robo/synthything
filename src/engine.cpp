#include "engine.hpp"
#include "generator.hpp"
#include "filter.hpp"

#include <future>
#include <algorithm>
#include <numeric>
#include <math.h>
#include <iostream>
#include <map>
#include <fmt/format.h>

using namespace synth;
using voice = std::vector< generator >;

void run_engine( engine::implementation & );

struct engine::implementation
{
	audio::interface &audio;
	std::atomic<bool> running;
	std::future<void> job;
	std::mutex mutex;
	std::map< uint8_t, voice > voices;
	double volume;

	implementation( audio::interface &interface ) :
		audio( interface ),
		running( true ),
		job( std::async( std::launch::async, [ this ](){ run_engine( *this ); } ) ),
		mutex(),
		voices(),
		volume( 0.3 )
	{
	}

	~implementation()
	{
		running = false;
		job.wait();
	}
};

auto frequency_for_note( int note )
{
	constexpr auto a4_frequency = 440.0;
	constexpr auto a4_note = 69.0;

	const auto note_delta = note - a4_note;
	return a4_frequency * pow( 2, note_delta/12.0 );
}

engine::engine( audio::interface &interface ) :
	impl_( new implementation( interface ) )
{
	interface.on_midi( [ this ]( const audio::midi &midi )
	{
		using type = audio::midi::message_type;

		switch( midi.message() )
		{
			case type::note_on:
			{
				const int note = midi.data[ 0 ];
				const auto velocity = static_cast< double >( midi.data[ 1 ] ) / 127.0;
				const auto frequency = frequency_for_note( note );

				std::lock_guard( impl_->mutex );
				auto &voice = impl_->voices[ note ];

				if( voice.empty() )
				{
					generator g { synth::saw, frequency, impl_->volume * velocity };
					g.mod.frequency.push_back
					( 
						{
							std::plus< double >(),
							generator { synth::sine, 1, 100 } 
						} 
					);
					g.mod.filters.push_back( 
						synth::low_pass( 0.3 )
					);
					voice.emplace_back( std::move( g ) );
				}

				break;
			}

			case type::note_off:
			{
				std::lock_guard( impl_->mutex );
				int note = midi.data[ 0 ];
				auto &voice = impl_->voices[ note ];
				voice.clear();
				break;
			}
		}
	} );
}

engine::~engine()
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
		std::fill( buffer.begin(), buffer.end(), 0.0f );
	} );

	return result;
}

template < typename Buffer >
void generate( Buffer &buffer, voice &v )
{
	std::transform(
		buffer.begin(), buffer.end(), buffer.begin(), 
		[ &v ]( float value )
		{
			// accumulate the output of each generator in this voice
			return std::accumulate( v.begin(), v.end(), value, 
				[]( double value, synth::generator &generator )
				{
					value += generator();
					generator.advance();
					
					return value;
				} 
			);
		} 
	);
}

void run_engine( engine::implementation &impl )
{
	while( impl.running )
	{
		auto frame = impl.audio.next_frame();
		auto buffers = get_buffers( frame );

		if( !buffers.empty() )
		{
			auto &buffer = buffers.front();

			std::fill( buffer.begin(), buffer.end(), 0.0f );

			std::lock_guard( impl.mutex );

			for( auto &voice : impl.voices )
			{
				if( !voice.second.empty() )
				{
					generate( buffer, voice.second );
				}
			}

			for( auto it = buffers.begin() + 1; it != buffers.end(); ++it )
			{
				std::copy( buffer.begin(), buffer.end(), it->begin() );
			}
		}

		frame.promised_buffers.set_value( std::move( buffers ) );
	}
}

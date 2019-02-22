#include "jack.hpp"

#include <jack/midiport.h>
#include <fmt/format.h>
#include <atomic>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <chrono>
#include <mutex>
#include <fstream>

using namespace std::chrono_literals;

namespace audio::jack 
{
	using buffers = std::vector< buffer >;
	using client = std::unique_ptr< jack_client_t, decltype( &jack_client_close ) >;

	client create_client( std::string_view name, std::atomic< bool > &server_running )
	{
		const char *server_name = nullptr;
		jack_options_t options = JackNoStartServer;
		jack_status_t status;

		jack::client client = 
		{
			jack_client_open( name.data(), options, &status, server_name ),
			&jack_client_close
		};

		if( !client )
		{
			throw std::runtime_error( "a jack client could not be created" );
		}

		if( status & JackServerStarted )
		{
			fmt::print( "JACK server started\n" );
			server_running = true;
		}

		if( status & JackNameNotUnique )
		{
			fmt::print( "JACK name not unique, instead got {}\n", jack_get_client_name( client.get() )  );
		}

		return client;
	}

	using port = std::unique_ptr< jack_port_t, std::function< void( jack_port_t * ) > >;

	port create_port( 
		jack_client_t *client, std::string_view port_name,
		int buffer_size,
		const char* type = JACK_DEFAULT_AUDIO_TYPE,
		JackPortFlags flags = JackPortIsOutput )
	{
		port result
		{
			jack_port_register( 
				client, 
				port_name.data(),
				type,
				flags,
				buffer_size
			),
			[ client ]( auto *port )
			{
				jack_port_unregister( client, port );
			}
		};

		if( !result )
		{
			throw std::runtime_error( fmt::format( "could not register port {}", port_name.data() ) );
		}

		return result;
	}
	
	auto create_audio_ports( jack_client_t *client, int count, int buffer_size, JackPortFlags flags = JackPortIsOutput )
	{
		std::vector< jack::port > ports;
		ports.reserve( count );

		std::generate_n(
			std::back_inserter( ports ),
			count,
			[ &, channel = 0 ]( ) mutable -> jack::port
			{
				const auto port_name = fmt::format( "out {}", channel++ );
				return create_port( client, port_name, buffer_size, JACK_DEFAULT_AUDIO_TYPE, flags );
			}
		);
		
		return ports;
	}

	void jack_on_shutdown( void *arg );
	int jack_on_process( jack_nframes_t nframes, void* arg );
	int jack_on_buffer_size_change( jack_nframes_t nframes, void* arg );

	void read_midi_events( 
		jack_port_t *port, 
		jack_nframes_t frames,
		const midi_handler &on_midi
	) {
		if( !on_midi )
		{
			return;
		}

		if( auto *data = jack_port_get_buffer( port, frames ) )
		{
			const auto count = jack_midi_get_event_count( data );
			
			jack_midi_event_t event;
			int index = 0;
			
			while( index < count )
			{
				jack_midi_event_get( &event, data, index++ );
				on_midi( midi { *event.buffer, event.buffer + 1 } );
			}
		}
	}

	struct interface::implementation
	{
		std::atomic< bool > jack_server_running;
		jack::client client;
		jack_nframes_t buffer_size;
		std::vector< jack::port > audio_ports;
		jack::port midi_port;

		std::mutex mutex;
		std::condition_variable wait_for_process;
		std::future< buffers > future_buffers;
		buffers recycled_buffers;
		midi_handler on_midi;

		implementation( std::string_view name ) :
			jack_server_running( false ),
			client( create_client( name, jack_server_running ) ),
			buffer_size( jack_get_buffer_size( client.get() ) ),
			audio_ports( create_audio_ports( client.get(), 2, buffer_size ) ),
			midi_port( create_port( client.get(), "midi in", buffer_size, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput ) ),
			mutex(),
			wait_for_process(),
			future_buffers(),
			recycled_buffers(),
			on_midi()
		{
			jack_set_process_callback( client.get(), &jack_on_process, this );
			jack_set_buffer_size_callback( client.get(), &jack_on_buffer_size_change, this );
			jack_on_shutdown( client.get(), &jack_on_shutdown, this );

			if( auto error = jack_activate( client.get() ); error != 0 )
			{
				throw std::runtime_error( fmt::format( "could not activate jack client, error {}", error ) );
			}
		}

		void shutdown()
		{
			jack_server_running = false;
		}

		buffers get_buffers()
		{
			std::lock_guard guard( mutex );
				
			if( future_buffers.valid() && future_buffers.wait_for( 10ms ) == std::future_status::ready )
			{
				return future_buffers.get();
			}

			return {};
		}

		int process( jack_nframes_t frames )
		{
			auto buffers = get_buffers();
			auto port = audio_ports.begin();

			for( auto &buffer : buffers )
			{
				auto *data = reinterpret_cast< jack_default_audio_sample_t* >
				( 
					jack_port_get_buffer( port->get(), frames )
				);

				if( data )
				{
					const auto samples = std::min< size_t >( frames, buffer.size() );
					std::copy_n( 
						buffer.begin(), 
						samples, 
						data
					);
				}
				++port;
			}

			read_midi_events( midi_port.get(), frames, on_midi );

			std::swap( buffers, recycled_buffers );
			wait_for_process.notify_all();

			return 0;
		}

		int buffer_size_changed( jack_nframes_t size )
		{
			buffer_size = size;
			return 0;
		}

	};

	interface::interface( std::string_view name ) :
		impl_( new implementation( name ) )
	{
	}

	interface::~interface()
	{
	}

	uint32_t interface::sample_rate() const
	{
		return jack_get_sample_rate( impl_->client.get() );
	}

	uint32_t interface::channel_count() const
	{
		return impl_->audio_ports.size();
	}

	auto acquire_lock( interface::implementation &impl )
	{
		std::unique_lock lock( impl.mutex );
		const auto future_done = [ & ]() { return !impl.future_buffers.valid(); };
		
		while( !future_done() )
		{
			impl.wait_for_process.wait( lock );
		}

		return lock;
	}

	frame interface::next_frame()
	{
		const auto lock = acquire_lock( *impl_ );		

		frame result
		{
			impl_->buffer_size, // sample count
			2, // channel count
			{}, // promise constructed by default
			{} // recycled buffers
		};

		std::swap( result.recycled_buffers, impl_->recycled_buffers );
		impl_->future_buffers = result.promised_buffers.get_future();

		return result;
	}

	void interface::on_midi( midi_handler handler ) 
	{
		impl_->on_midi = handler;
	}

	template< typename Result, typename ... Args >
	auto call_on_implementation( Result( interface::implementation::*func )( Args ... ),  void *ptr, Args ... args ) -> Result
	{
		if( auto impl = reinterpret_cast< interface::implementation* >( ptr ) )
		{
			auto call_member_on = std::mem_fn( func );
			return call_member_on( impl, args... );
		}

		if constexpr( std::is_void_v< Result > == false )
		{
			return Result();
		}
	}

	void jack_on_shutdown( void *arg )
	{
		call_on_implementation( &interface::implementation::shutdown, arg );
	}
	
	int jack_on_process( jack_nframes_t nframes, void* arg )
	{
		return call_on_implementation( &interface::implementation::process, arg, nframes );
	}

	int jack_on_buffer_size_change( jack_nframes_t nframes, void* arg )
	{
		return call_on_implementation( &interface::implementation::buffer_size_changed, arg, nframes );
	}

}
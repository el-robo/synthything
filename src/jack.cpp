#include "jack.hpp"

#include <fmt/format.h>
#include <atomic>
#include <algorithm>
#include <functional>

namespace audio::jack 
{
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

	port create_port( jack_client_t *client, std::string_view port_name, int buffer_size, JackPortFlags flags = JackPortIsOutput )
	{
		port result
		{
			jack_port_register( 
				client, 
				port_name.data(),
				JACK_DEFAULT_AUDIO_TYPE,
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

	auto create_ports( jack_client_t *client, int count, int buffer_size, JackPortFlags flags = JackPortIsOutput )
	{
		std::vector< port > ports;
		ports.reserve( count );

		std::generate_n(
			std::back_inserter( ports ),
			count,
			[ &, channel = 0 ]( ) mutable
			{
				const auto port_name = fmt::format( "out_{}", channel++ );
				return create_port( client, port_name, buffer_size, flags );
			}
		);
		
		return ports;
	}

	void jack_shutdown( void *arg );
	int jack_process( jack_nframes_t nframes, void* arg );

	struct engine::implementation
	{
		std::atomic< bool > jack_server_running;
		jack::client client;
		std::vector < port > ports;

		implementation( std::string_view name ) :
			jack_server_running( false ),
			client( create_client( name, jack_server_running ) ),
			ports( create_ports( client.get(), 2, 1920 ) )
		{
			jack_set_process_callback( client.get(), &jack_process, this );
			jack_on_shutdown( client.get(), jack_shutdown, this );

			// jack_activate()
		}

		void shutdown()
		{
			jack_server_running = false;
		}

		void process( jack_nframes_t frames )
		{
			// impl->jack_clock += nframes;

			// for( auto &channel : impl->channels )
			// {
			// 	if( auto *data = reinterpret_cast< jack_default_audio_sample_t* >( jack_port_get_buffer( channel.port, nframes ) ) )
			// 	{
			// 		switch( impl->mode )
			// 		{
			// 			case DaveDefines::Output:
			// 			{
			// 				mix_out( channel.buffer, data, nframes );
			// 				break;
			// 			}

			// 			default:
			// 			{
			// 				break;
			// 			}
			// 		}
			// 	}
			// }
		}
	};

	engine::engine( std::string_view name ) :
		impl_( new implementation( name ) )
	{
	}

	engine::~engine()
	{
	}

	void engine::write_audio( const std::vector< buffer > &audio )
	{
	}

	void jack_shutdown( void *arg )
	{
		if( auto impl = reinterpret_cast< engine::implementation* >( arg ) )
	   	{
			impl->jack_server_running = false;
		}
	}
	
	int jack_process( jack_nframes_t nframes, void* arg )
	{
		if( auto impl = reinterpret_cast< engine::implementation* >( arg ) )
		{
			impl->process( nframes );
			return 0;
		}

		return -1;
	}
}
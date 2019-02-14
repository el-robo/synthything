#include "jack.hpp"

#include <fmt/format.h>
#include <atomic>

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

	void jack_shutdown( void *arg );
	int jack_process( jack_nframes_t nframes, void* arg );

	struct engine::implementation
	{
		std::atomic< bool > jack_server_running;
		jack::client client;


		implementation( std::string_view name ) :
			jack_server_running( false ),
			client( create_client( name, jack_server_running ) )
		{
			jack_set_process_callback( client.get(), &jack_process, this );
			jack_on_shutdown( client.get(), jack_shutdown, this );
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
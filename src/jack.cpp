#include "jack.hpp"

#include <fmt/format.h>
#include <atomic>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <chrono>
#include <mutex>

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

	struct channel
	{
		jack::port port;
	};

	auto create_channels( jack_client_t *client, int count, int buffer_size, JackPortFlags flags = JackPortIsOutput )
	{
		std::vector< channel > channels;
		channels.reserve( count );

		std::generate_n(
			std::back_inserter( channels ),
			count,
			[ &, channel = 0 ]( ) mutable -> channel
			{
				const auto port_name = fmt::format( "out_{}", channel++ );
				return
				{
					create_port( client, port_name, buffer_size, flags )
				};
			}
		);
		
		return channels;
	}

	void jack_on_shutdown( void *arg );
	int jack_on_process( jack_nframes_t nframes, void* arg );
	int jack_on_buffer_size_change( jack_nframes_t nframes, void* arg );

	struct interface::implementation
	{
		std::atomic< bool > jack_server_running;
		jack::client client;
		jack_nframes_t buffer_size;
		std::vector< channel > channels;

		std::mutex mutex;
		std::future< buffers > future_buffers;
		buffers recycled_buffers;

		implementation( std::string_view name ) :
			jack_server_running( false ),
			client( create_client( name, jack_server_running ) ),
			buffer_size( jack_get_buffer_size( client.get() ) ),
			channels( create_channels( client.get(), 2, buffer_size ) ),
			mutex(),
			future_buffers(),
			recycled_buffers()
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

		int process( jack_nframes_t frames )
		{
			std::lock_guard guard( mutex );

			if( future_buffers.valid() && future_buffers.wait_for( 1ms ) == std::future_status::ready )
			{
				auto buffers = future_buffers.get();
				auto channel_it = channels.begin();
				auto buffer_it = buffers.begin();

				for( ; channel_it < channels.end() && buffer_it < buffers.end();
					++channel_it, ++buffer_it )
				{
					auto &channel = *channel_it;
					auto &buffer = *buffer_it;

					if( auto *data = reinterpret_cast< jack_default_audio_sample_t* >( jack_port_get_buffer( channel.port.get(), frames ) ) )
					{
						std::copy_n( 
							buffer.begin(), 
							std::min< size_t >( frames, buffer.size() ), 
							data
						);
					}
				}

				std::swap( buffers, recycled_buffers );
			}
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

	uint32_t interface::channels_count() const
	{
		return impl_->channels.size();
	}

	frame interface::next_frame()
	{
		std::lock_guard guard( impl_->mutex );

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
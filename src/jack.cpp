#include "jack.hpp"

namespace audio::jack 
{
	using client = std::unique_ptr< jack_client_t, decltype( &jack_client_close ) >;

	client create_client( std::string_view name )
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

		return client;
	}

	struct engine::implementation
	{
		client client_;

		implementation( std::string_view name ) :
			client_( create_client( name ) )
		{

		}
	};

	engine::engine( std::string_view name ) :
		impl_( new implementation( name ) )
	{
	}

	engine::~engine()
	{
	}

	void engine::write_audio()
	{
	}
}
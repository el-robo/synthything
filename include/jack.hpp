#include "audio_interface.hpp"

#include <jack/jack.h>

#include <memory>
#include <string_view>

namespace audio::jack
{
	struct engine : audio::interface
	{
		engine( std::string_view name );
		~engine();

		void write_audio( const std::vector< buffer >& ) override;

		struct implementation;
		
		private:
			std::unique_ptr< implementation > impl_;
	};
	
}
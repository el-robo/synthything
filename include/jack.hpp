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

		void write_audio() override;

		private:
			struct implementation;
			std::unique_ptr< implementation > impl_;
	};
	
}
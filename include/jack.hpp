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

		virtual uint32_t sample_rate() const override;
		virtual uint32_t channels_count() const override;
		
		virtual frame next_frame() override;

		struct implementation;
		
		private:
			std::unique_ptr< implementation > impl_;
	};
	
}
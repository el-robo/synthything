#pragma once

#include "audio.hpp"

#include <jack/jack.h>

#include <memory>
#include <string_view>

namespace audio::jack
{
	struct interface : audio::interface
	{
		interface( std::string_view name );
		~interface();

		virtual uint32_t channel_count() const override;
		
		virtual frame next_frame() override;

		virtual void on_midi( midi_handler ) override;

		struct implementation;
		
		private:
			std::unique_ptr< implementation > impl_;
	};
	
}
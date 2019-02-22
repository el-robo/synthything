#pragma once

#include "midi.hpp"

#include <vector>
#include <cstdint>
#include <future>

namespace audio
{
	struct frame;
	using midi_handler = std::function< void( midi ) >;

	struct interface
	{
		virtual uint32_t sample_rate() const = 0;
		virtual uint32_t channel_count() const = 0;
		
		virtual frame next_frame() = 0;

		virtual void on_midi( midi_handler ) = 0;
	};

	using buffer = std::vector< float >;

	struct frame
	{
		const uint32_t sample_count;
		const size_t channel_count;
		std::promise< std::vector< buffer > > promised_buffers;
		std::vector< buffer > recycled_buffers;
	};
}
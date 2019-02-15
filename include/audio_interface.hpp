#include <vector>
#include <cstdint>
#include <future>

namespace audio
{
	struct frame;

	struct interface
	{
		using buffer = std::vector< float >;

		virtual uint32_t sample_rate() const = 0;
		virtual uint32_t channels_count() const = 0;
		
		virtual frame next_frame() = 0;
	};

	struct frame
	{
		uint32_t sample_count;
		std::promise< std::vector< interface::buffer > > promised_buffers;
		std::vector< interface::buffer > recycled_buffers;
	};
}
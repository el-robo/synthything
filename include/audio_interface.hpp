#include <vector>

namespace audio
{
	struct interface
	{
		using buffer = std::vector< float >;

		virtual void write_audio( const std::vector< buffer > & ) = 0;
	};
}
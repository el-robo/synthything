#pragma once

#include "audio.hpp"

#include <map>

namespace analysis
{
	namespace frequency 
	{
		struct config
		{
			const double frequency_start;
			const double frequency_end;
			const double frequency_interval;
		};

		using frequencies = std::map< double, double >;

		frequencies process( const audio::frame &, const config & );
	}
}
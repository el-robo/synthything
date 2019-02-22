#pragma once

#include "audio.hpp"
#include "midi.hpp"

#include <memory>

namespace synth 
{
	class engine
	{
		public:
			engine( audio::interface & );
			~engine();
			
			struct implementation;

		private:
			std::unique_ptr< implementation > impl_;
	};
}
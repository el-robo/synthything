#pragma once

#include "audio.hpp"
#include <memory>

namespace synth 
{
	class engine
	{
		public:
			engine( audio::interface & );
			~engine();
			
			void process();

			struct implementation;

		private:
			std::unique_ptr< implementation > impl_;
	};
}
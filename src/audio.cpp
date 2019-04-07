#include "audio.hpp"

namespace audio {

	double& access_sample_rate()
	{
		static double sample_rate = 48000.0;
		return sample_rate;
	}

	double sample_rate()
	{
		return access_sample_rate();
	}

	void set_sample_rate( double sample_rate )
	{
		access_sample_rate() = sample_rate;
	}
}
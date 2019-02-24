#pragma once

#include <cstdint>

namespace audio
{
	struct midi
	{
		uint8_t header;
		const uint8_t* data;

		enum message_type
		{
			note_off = 0b1000,
			note_on = 0b1001,
			polyphonic_key_pressure = 0b1010,
			control_change = 0b1011,
			program_change = 0b1100,
			channel_pressure = 0b1101,
			pitch_bend = 0b1110,
			system_exclusive = 0b1111			
		};

		inline uint8_t message() const
		{
			return (header & 0xF0) >> 4;
		}

		inline uint8_t channel() const
		{
			return header & 0x0F;
		}
	};
}
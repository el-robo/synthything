#pragma once

#include <functional>

namespace synth {

struct generator;

template < typename T >
using mod_pair = std::pair< std::function< T( T, T ) >, generator >;

template < typename T >
using modulators = std::vector< mod_pair< T > >;

struct generator
{
	using function = std::function< double( generator & ) >;

	const function wave_function;
	double frequency_ = 440.0;
	double amplitude_ = 0.1;
	double t = 0.0;
	double period = 1.0; // this is set by the wave function

	struct
	{
		modulators< double > frequency;
		modulators< double > amplitude;
	} mod;

	double frequency();
	double amplitude();

	double operator()();
	void advance( double sample_rate );
};

}

#pragma once

#include "filter.hpp"

#include <functional>
#include <vector>

namespace synth {

struct generator;

/**
 * Modulators are a combination of a binary function and a generator.
 * For example: { std::plus< double >(), generator { sine, 2, 100 } };
 * The amplitude is meant to be way higher in order to have much effect.
 **/

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
		std::vector< synth::filter_function > filters;
	} mod;

	double frequency();
	double amplitude();

	double operator()();
	void advance();
};

double sine( generator &g );
double square( generator &g );
double saw( generator &g );

}

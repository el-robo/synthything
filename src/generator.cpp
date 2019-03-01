#include "generator.hpp"

#include <algorithm>
#include <numeric>
#include <math.h>

using namespace synth;

template < typename T >
auto process( T value, modulators< T > &mod )
{
	return std::accumulate( 
		mod.begin(), mod.end(), value, 
		[]( double value, auto &pair )
		{
			auto& [ mod, generator ] = pair;
			return mod( value, generator() );
		} 
	);
}

template < typename T >
void advance_modulators( modulators< T > &mod, T sample_rate )
{
	for( auto &pair : mod )
	{
		pair.second.advance( sample_rate );
	}
}

double generator::frequency()
{
	return process( frequency_, mod.frequency ); 
}

double generator::amplitude()
{
	return process( amplitude_, mod.amplitude );
}

double generator::operator()()
{
	if( wave_function )
	{
		return wave_function( *this );
	}

	return 0.0;
}

void generator::advance( double sample_rate )
{
	t += (period / sample_rate) * frequency();

	advance_modulators( mod.frequency, sample_rate );
	advance_modulators( mod.amplitude, sample_rate );
}

constexpr auto pi = 3.1415926535897932384626433;
constexpr auto two_pi = pi * 2;

auto clip( double value )
{
	return std::max( -1.0, std::min( 1.0, value ) );
}

double synth::sine( generator &g )
{
	const auto value = g.amplitude() * clip( sin( g.t ) );
	g.period = two_pi;
	return value;
}

double synth::square( generator &g )
{
	return g.amplitude() * (synth::sine( g ) > 0.0 ? 1.0 : -1.0);
}

double synth::saw( generator &g )
{
	return (g.amplitude() * 2  * (g.t - std::floor( g.t )) ) - 1.0;
}
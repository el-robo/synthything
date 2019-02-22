#include "generator.hpp"

#include <algorithm>
#include <numeric>

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
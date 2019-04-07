#include "filter.hpp"
#include <numeric>
#include <memory>

namespace synth {

double filter( double value, const std::vector< filter_function > &filters )
{
	return std::accumulate( 
		filters.begin(), filters.end(), value, 
		[]( double value, const synth::filter_function &filter )
		{
			return filter( value );
		} 
	);
}

// auto low_pass_buffer( double frequency )
// {
// 	auto buffer_size = sample_rate / frequency;
// 	return std::make_shared< std::vector< double > >( buffer_size );
// }

filter_function low_pass( double frequency )
{
	return [
		frequency,
		last_value = 0.0
	] ( double value ) mutable
	{
		//  y[i] := y[i-1] + α * (x[i] - y[i-1])
		const auto filtered = last_value + frequency * (value - last_value);
		last_value = filtered;
		return filtered;
	};
}

}
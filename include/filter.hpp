#pragma once

#include <functional>
#include <vector>

namespace synth {

	/**
	 * A filter function accepts a value
	 * and returns the filtered result.
	 **/
	using filter_function = std::function< double( double ) >;

	double filter( double value, const std::vector< filter_function > &filters );

	filter_function low_pass( double frequency );

}
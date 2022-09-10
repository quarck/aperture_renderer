#pragma once

#define _USE_MATH_DEFINES // for C++
#include <cmath>

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

template <typename TFloat>
struct lambda_profile
{
	float lambda; // wavelength 
	float velocity; // wave velocity through the medium
	TFloat two_pi_inverse_lambda;
	
	lambda_profile(float lambda)
		: lambda{ lambda }
		, velocity{  }
		, two_pi_inverse_lambda{ static_cast<TFloat>((2.0 * M_PI) / lambda) }
	{
	}

	lambda_profile() : lambda_profile{ 0 } {}
};
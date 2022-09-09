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
	TFloat inverse_velocity;
	//float omega; // we set such a value of v, so omega == 1.0 all the time. All we care is the wavelenght 

	lambda_profile(float lambda)
		: lambda{ lambda }
		, velocity{  }
		, inverse_velocity{ static_cast<TFloat>((2.0 * M_PI) / lambda) }
	{
	}

	lambda_profile() : lambda_profile{ 0 } {}
};
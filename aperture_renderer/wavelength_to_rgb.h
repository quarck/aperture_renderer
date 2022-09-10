#pragma once

#include <tuple>

// Credits to https://stackoverflow.com/questions/1472514/convert-light-frequency-to-rgb 

std::tuple<float, float, float> wavelength_to_rgb(float wavelength, float range_low, float range_high)
{
	float gamma = .8;
	float red, green, blue;

	if (range_low == range_high)
		return { 1.0f,  1.0f, 1.0f };

	// original formula got a bit of a over-stretch into red, '1.3' fixes it a bit
	wavelength = (wavelength - range_low) / (range_high - range_low) / 1.3f; 

	if ((wavelength >= 0.0f) && (wavelength < 0.15f))
	{
		red = -(wavelength - 0.15f) / (0.15f - 0.0f);
		green = 0.0f;
		blue = 1.0f;
	}
	else if ((wavelength >= 0.15f) && (wavelength < 0.275f))
	{
		red = 0.0f;
		green = (wavelength - 0.15f) / (0.275f - 0.15f);
		blue = 1.0f;
	}
	else if ((wavelength >= 0.275f) && (wavelength < 0.325f))
	{
		red = 0.0f;
		green = 1.0f;
		blue = -(wavelength - 0.325f) / (0.325f - 0.275f);
	}
	else if ((wavelength >= 0.325f) && (wavelength < 0.5f))
	{
		red = (wavelength - 0.325f) / (0.5f - 0.325f);
		green = 1.0f;
		blue = 0.0f;
	}
	else if ((wavelength >= 0.5f) && (wavelength < 0.6625f))
	{
		red = 1.0f;
		green = -(wavelength - 0.6625f) / (0.6625f - 0.5f);
		blue = 0.0f;
	}
	else if ((wavelength >= 0.6625f) && (wavelength <= 1.0f))
	{
		red = 1.0f;
		green = 0.0f;
		blue = 0.0f;
	}
	else
	{
		red = 0.0f;
		green = 0.0f;
		blue = 0.0f;
	}

	float factor = 1.0f;

	if ((wavelength >= 0.0f) && (wavelength < 0.1f))
	{
		factor = 0.3f + 0.7f * wavelength / (0.1f - 0.0f);
	}
	else if ((wavelength >= 0.1f) && (wavelength < 0.7f))
	{
		factor = 1.0f;
	}
	else if ((wavelength >= 0.7f) && (wavelength <= 1.0f))
	{
		factor = 0.3f + 0.7f * (1.f / 1.3f - wavelength) / (1.f / 1.3f - .7f);
	}
	else
	{
		factor = 0.0f;
	}


	if (red != 0.0f)
		red = std::powf(red * factor, gamma);

	if (green != 0.0f)
		green = std::powf(green * factor, gamma);

	if (blue != 0.0f)
		blue = std::powf(blue * factor, gamma);

	return { red, green, blue };
}

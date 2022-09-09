#pragma once

#include <tuple>

// Credits to https://stackoverflow.com/questions/1472514/convert-light-frequency-to-rgb 

std::tuple<float, float, float> wavelength_to_rgb(float wavelength, float range_low, float range_high)
{
	double gamma = 0.80;
	double red, green, blue;

	if (range_low == range_high)
		return { 1.0f,  1.0f, 1.0f };

	// original formula got a bit of a over-stretch into red, '1.3' fixes it a bit
	wavelength = (wavelength - range_low) / (range_high - range_low) / 1.3f; 

	if ((wavelength >= 0.0) && (wavelength < 0.15))
	{
		red = -(wavelength - 0.15) / (0.15 - 0.0);
		green = 0.0;
		blue = 1.0;
	}
	else if ((wavelength >= 0.15) && (wavelength < 0.275))
	{
		red = 0.0;
		green = (wavelength - 0.15) / (0.275 - 0.15);
		blue = 1.0;
	}
	else if ((wavelength >= 0.275) && (wavelength < 0.325))
	{
		red = 0.0;
		green = 1.0;
		blue = -(wavelength - 0.325) / (0.325 - 0.275);
	}
	else if ((wavelength >= 0.325) && (wavelength < 0.5))
	{
		red = (wavelength - 0.325) / (0.5 - 0.325);
		green = 1.0;
		blue = 0.0;
	}
	else if ((wavelength >= 0.5) && (wavelength < 0.6625))
	{
		red = 1.0;
		green = -(wavelength - 0.6625) / (0.6625 - 0.5);
		blue = 0.0;
	}
	else if ((wavelength >= 0.6625) && (wavelength <= 1.0))
	{
		red = 1.0;
		green = 0.0;
		blue = 0.0;
	}
	else
	{
		red = 0.0;
		green = 0.0;
		blue = 0.0;
	}

	double factor = 1.0;

	if ((wavelength >= 0.0) && (wavelength < 0.1))
	{
		factor = 0.3 + 0.7 * wavelength / (0.1 - 0.0);
	}
	else if ((wavelength >= 0.1) && (wavelength < 0.7))
	{
		factor = 1.0;
	}
	else if ((wavelength >= 0.7) && (wavelength <= 1.0))
	{
		factor = 0.3 + 0.7 * (1 / 1.3 - wavelength) / (1 / 1.3 - .7);
	}
	else
	{
		factor = 0.0;
	}


	if (red != 0.0)
		red = std::powf(red * factor, gamma);

	if (green != 0.0)
		green = std::powf(green * factor, gamma);

	if (blue != 0.0)
		blue = std::powf(blue * factor, gamma);

	return { (float)red, (float)green, (float)blue };
}

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

#include <immintrin.h> 

#include <iostream>
#include <vector>

#include "lodepng.h"
#include "ThreadGrid.h"

constexpr float PI = 3.14159265359f;

constexpr float GENERAL_MULTIPLIER = 10000.0;
constexpr float BRIGHT_RATIO = 90000.0; // basically defines how much "light" we want to see in the final render

struct aperture
{
	// i = y * Width + x
	std::vector<bool> intensity_mask;
	std::vector<float> z_values;

	int width;
	int height;

	//float omega; // we set such a value of v, so omega == 1.0 all the time. All we care is the wavelenght 
	float lambda; // wavelength 
	float velocity; // wave velocity through the medium
	float d; // distance between z==0 and the 'screen'
	
	// R is the radius of the 'lense', with the centre at (Width/2.0, Height/2.0, R), it affects the 
	// curvature of 'ZValues'

	aperture(std::vector<unsigned char> img, 
		int width, int height, float d, float R, float lambda)
		: width { width }
		, height { height }
		, d { d }
		, lambda { lambda }
		, velocity { static_cast<float>(lambda / (2.0 * PI)) } // thus f == 1/(2*pi), omega == 1.0
	{
		intensity_mask.resize(width * height);
		z_values.resize(width * height);

		float cx = width / 2.0f;
		float cy = height / 2.0f;

		// Loop through the images pixels to reset color.
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				auto img_offs = 4 * (y * width + x);
				
				float r = img[img_offs];
				float g = img[img_offs + 1];
				float b = img[img_offs + 2];

				float v = (r + g + b) / 3.0f / 255.0f;

				intensity_mask[y * width + x] = v > 0.5f;
				z_values[y * width + x] = 
					-std::sqrt(
							R * R - std::powf(x - cx, 2.0) - std::powf(y - cy, 2.0)
					) + d;
			}
		}
	}

	float diff_value(int x, int y)
	{
		float accum_a = 0;
		float accum_b = 0;

		for (int ay = 0; ay < height; ++ay)
		{
			for (int ax = 0; ax < width; ++ax)
			{
				int offs = ay * width + ax;

				if (!intensity_mask[offs])
					continue;

				float z = z_values[offs];

				auto l_sqr = std::pow(ax - x, 2.0) + std::pow(ay - y, 2.0) + std::pow(z - d, 2.0);

				float ivq = (float)(GENERAL_MULTIPLIER /* * intensity*/ / l_sqr);
				auto d_tv = std::sqrt(l_sqr) / velocity;

				accum_a += (float)(ivq * std::cos(d_tv));
				accum_b += (float)(ivq * std::sin(d_tv));
			}
		}

		return (float)(PI * (std::pow(accum_a, 2.0) + std::pow(accum_b, 2.0)));
	}
};

void report_progress(int value, int total)
{
	std::cout << "progress: [";

	for (int i = 0; i < 60; i++)
	{
		if (i * total < 60 * value)
			std::cout << "=";
		else
			std::cout << "-";
	}
	std::cout << "]" << std::endl;
}


int main(int argc, char* argv[])
{
	//_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	//_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

#ifdef _DEBUG
	int numWorkerThreads = 1;
#else 
	SYSTEM_INFO sysinfo;
	::GetSystemInfo(&sysinfo);
	int numWorkerThreads = sysinfo.dwNumberOfProcessors;
#endif

	if (argc < 3)
	{
		std::cerr << "Wrong usage, try:" << std::endl;
		std::cerr << "aperture_renderer <input.png> <output.png> [<d>] [<R>] [<lambda>]" << std::endl;
		std::cerr << "default values are: d = 200 (px), R = 1000 (px), lambda = 0.55" << std::endl;
		std::cerr << "Note: lambda defines the wavelength for the mid-spectrum, rendering will be done using three values:" << std::endl;
		std::cerr << "lambda_red = lambda * 1.2" << std::endl;
		std::cerr << "lambda_green = lambda * 1.0" << std::endl;
		std::cerr << "lambda_blue = lambda / 1.2" << std::endl;
		std::cerr << "(go to the source code to change those multipliers)";
		return -1;
	}

	std::string input = argv[1];
	std::string output = argv[2];

	float d = argc >= 4 ? std::atof(argv[3]) : 200.0;
	float R = argc >= 5 ? std::atof(argv[4]) : 1000.0;
	float lambda_g = argc >= 6 ? std::atof(argv[5]) : .55;
	float lambda_r = lambda_g * 1.2f;
	float lambda_b = lambda_r / 1.2f;

	std::vector<unsigned char> data;
	unsigned width;
	unsigned height;
	if (lodepng::decode(data, width, height, input) != 0)
	{
		std::cerr << "Failed to open " << input << std::endl;
		return -1;
	}

	ThreadGrid _grid{ numWorkerThreads };

	aperture ap_r{ data, static_cast<int>(width), static_cast<int>(height), d, R, lambda_r };
	aperture ap_g{ data, static_cast<int>(width), static_cast<int>(height), d, R, lambda_g };
	aperture ap_b{ data, static_cast<int>(width), static_cast<int>(height), d, R, lambda_b };

	std::vector<float> out_raw_r(width * height);
	std::vector<float> out_raw_g(width * height);
	std::vector<float> out_raw_b(width * height);

	std::mutex m;
	int progress = 0;

	_grid.GridRun(
		[&](int thread_idx, int num_threads)
		{
			int slice = static_cast<int>(1.0 * height / num_threads + 0.999);
			int from = thread_idx * slice;
			int to = std::min<int>(height, (thread_idx + 1) * slice);

			for (int y = from; y < to; ++y)
			{
				for (int x = 0; x < width; x++)
				{
					out_raw_r[y * width + x] = ap_r.diff_value(x, y) ;
				}
				std::lock_guard l{ m };
				report_progress(++progress, height * 3);
			}

			for (int y = from; y < to; ++y)
			{
				for (int x = 0; x < width; x++)
				{
					out_raw_g[y * width + x] = ap_g.diff_value(x, y);
				}
				std::lock_guard l{ m };
				report_progress(++progress, height * 3);
			}

			for (int y = from; y < to; ++y)
			{
				for (int x = 0; x < width; x++)
				{
					out_raw_b[y * width + x] = ap_b.diff_value(x, y);
				}
				std::lock_guard l{ m };
				report_progress(++progress, height * 3);
			}

		});

	float sum{ 0 };

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; x++)
		{
			float v = out_raw_r[y * width + x] + out_raw_g[y * width + x] + out_raw_b[y * width + x];
			sum += v / 3.0;
		}
	}

	float max{ sum / BRIGHT_RATIO }; 

	std::vector<unsigned char> out(width * height * 4);

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; x++)
		{
			int i_offs = y * width + x;
			int o_offs = 4 * i_offs;
			out[o_offs + 0] = std::min(255u, static_cast<unsigned>(255.0 * out_raw_r[i_offs] / max));
			out[o_offs + 1] = std::min(255u, static_cast<unsigned>(255.0 * out_raw_g[i_offs] / max));
			out[o_offs + 2] = std::min(255u, static_cast<unsigned>(255.0 * out_raw_b[i_offs] / max));
			out[o_offs + 3] = 255;
		}
	}

	lodepng::encode(output, out, width, height);

	return 0;
}

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

#include <immintrin.h> 

#include <iostream>
#include <vector>

#include "lodepng.h"
#include "ThreadGrid.h"

constexpr float PI = 3.14159265359f;

constexpr float GENERAL_MULTIPLIER = 1000000000;

struct aperture
{
	// i = y * Width + x
	std::vector<float> intensity_mask;
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

				//float v = (r + g + b) / 3.0f / 255.0f;
				float v = ((r > 127) ? 0.3333 : 0.0) + ((g > 127) ? 0.3333 : 0.0) + ((b > 127) ? 0.3334 : 0.0);

				intensity_mask[y * width + x] = v;
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

				float intensity = intensity_mask[offs];
				if (intensity < 0.00001)
					continue;

				float z = z_values[offs];

				auto l_sqr = std::pow(ax - x, 2.0) + std::pow(ay - y, 2.0) + std::pow(z - d, 2.0);

				float ivq = (float)(GENERAL_MULTIPLIER * intensity / l_sqr);
				auto d_tv = std::sqrt(l_sqr) / velocity;

				accum_a += (float)(ivq * std::cos(d_tv));
				accum_b += (float)(ivq * std::sin(d_tv));
			}
		}

		return (float)(PI * (std::pow(accum_a, 2.0) + std::pow(accum_b, 2.0)));
	}
};


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
		std::cerr << "Wrong usage -- see src.. joking" << std::endl;
		std::cerr << "aperture_renderer <input.png> <output.png> [<d>] [<R>] [<lambda>]" << std::endl;
		return -1;
	}

	std::string input = argv[1];
	std::string output = argv[2];

	float d = argc >= 4 ? std::atof(argv[3]) : 200.0;
	float R = argc >= 5 ? std::atof(argv[4]) : 1000.0;
	float lambda = argc >= 6 ? std::atof(argv[5]) : 0.5;

	std::vector<unsigned char> data;
	unsigned width;
	unsigned height;
	if (lodepng::decode(data, width, height, input) != 0)
	{
		std::cerr << "Failed to open " << input << std::endl;
		return -1;
	}

	ThreadGrid _grid{ numWorkerThreads };

	aperture ap{ data, static_cast<int>(width), static_cast<int>(height), d, R, lambda};

	std::vector<float> out_raw(width * height);

	auto max = ap.diff_value(width / 2 - 4, height / 2 - 4);

	std::mutex m;

	_grid.GridRun(
		[&](int thread_idx, int num_threads)
		{
			int slice = static_cast<int>(1.0 * height / num_threads + 0.999);
			int from = thread_idx * slice;
			int to = (thread_idx + 1) * slice;
			if (to > height)
				to = height;

			for (int y = from; y < to; ++y)
			{
				{
					std::lock_guard l{ m };
					std::cout << thread_idx << ": " << (y - from) << " of " << (to-from) << std::endl;
				}

				for (int x = 0; x < width; x++)
				{
					out_raw[y * width + x] = ap.diff_value(x, y) / max;
				}
			}
		});

	//for (int y = 0; y < height; ++y)
	//{
	//	for (int x = 0; x < width; x++)
	//	{
	//		out_raw[y * width + x] = ap.diff_value(x, y);
	//	}
	//}

	float max_value{ 0 };

	//for (int y = 0; y < height; ++y)
	//{
	//	for (int x = 0; x < width; x++)
	//	{
	//		float v = out_raw[y * width + x];
	//		max_value = v > max_value ? v : max_value;
	//	}
	//}

	//max_value *= 0.4;

	std::vector<unsigned char> out(width * height * 4);

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; x++)
		{
			int i_offs = y * width + x;
			int o_offs = 4 * i_offs;
			unsigned int pv = (unsigned)(255 * out_raw[i_offs]);
//			unsigned v = static_cast<unsigned>(255.0 * out_raw[i_offs] / max);
			if (pv > 255)
				pv = 255;
			out[o_offs + 0] = pv;
			out[o_offs + 1] = pv;
			out[o_offs + 2] = pv;
			out[o_offs + 3] = 255;
		}
	}

	lodepng::encode(output, out, width, height);

	return 0;
}

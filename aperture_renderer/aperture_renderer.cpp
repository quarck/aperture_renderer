#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <assert.h>

#include <immintrin.h> 

#include <iostream>
#include <vector>
#include <array>

#include "lodepng.h"
#include "ThreadGrid.h"

#include "wavelength_to_rgb.h"


constexpr int NUM_COLORS = 16;
constexpr float CLR_STEP = 1.0114602809799 * 1.0114602809799;

constexpr float PI = 3.14159265359f;

constexpr float GENERAL_MULTIPLIER = 10000.0;
constexpr float BRIGHT_RATIO = 90000.0; // basically defines how much "light" we want to see in the final render

struct lambda_profile
{
	float lambda; // wavelength 
	float velocity; // wave velocity through the medium
	//float omega; // we set such a value of v, so omega == 1.0 all the time. All we care is the wavelenght 

	lambda_profile(float lambda)
		: lambda{ lambda }
		, velocity{ static_cast<float>(lambda / (2.0 * PI)) }
	{
	}

	lambda_profile() : lambda_profile{ 0 } {}

};

template <size_t N, float C_STEP>
struct aperture
{
	using pixel = std::array<float, N>;

	// i = y * Width + x
	std::vector<float> intensity_mask;
	std::vector<float> z_values; // z-coordinates of the light emiting plane

	std::array<lambda_profile, N> lambda_profiles;

	int width;
	int height;


	float d; // distance between z==0 and the 'screen'
	
	// R is the radius of the 'lense', with the centre at (Width/2.0, Height/2.0, R), it affects the 
	// curvature of 'z_values'

	aperture(std::vector<unsigned char> img, 
		int width, int height, float d, float R, float lambda)
		: width { width }
		, height { height }
		, d { d }
	{
		intensity_mask.resize(width * height);
		z_values.resize(width * height);

		float cx = width / 2.0f - 0.5f;
		float cy = height / 2.0f - 0.5f;

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

				intensity_mask[y * width + x] = v > 0.5f ? 1.0 : 0.0;
				z_values[y * width + x] = 
					d - std::sqrt( R * R - std::powf(x - cx, 2.0) - std::powf(y - cy, 2.0) );
			}
		}

		for (int i = 0; i < N; i++)
		{
			float wl = std::powf(C_STEP, static_cast<int>(N) / 2 - i) * lambda;
			lambda_profiles[i] = lambda_profile{ wl };
		}
	}

	void diff_value(int x, int y, pixel& out, pixel& out_mx, pixel& out_my, pixel& out_mx_my) noexcept
	{
		pixel accum_a { 0 };
		pixel accum_b { 0 };

		pixel accum_a_mx{ 0 };
		pixel accum_b_mx{ 0 };

		pixel accum_a_my{ 0 };
		pixel accum_b_my{ 0 };

		pixel accum_a_mx_my{ 0 };
		pixel accum_b_mx_my{ 0 };

		for (int ay = 0; ay < height; ++ay)
		{
			for (int ax = 0; ax < width; ++ax)
			{
				int mx = width - 1 - x;
				int my = height - 1 - y;

				int offs = ay * width + ax;
				int offs_mx = ay * width + (width - ax - 1);
				int offs_my = (height - ay -1) * width + ax;
				int offs_mx_my = (height - ay - 1) * width + (width - ax - 1);

				float intensity = intensity_mask[offs];
				float intensity_mx = intensity_mask[offs_mx];
				float intensity_my = intensity_mask[offs_my];
				float intensity_mx_my = intensity_mask[offs_mx_my];

				if (intensity == 0 && intensity_mx == 0 && intensity_my == 0 && intensity_mx_my == 0)
					continue;

				float z = z_values[offs];

				assert(z_values[offs_mx] == z);
				assert(z_values[offs_my] == z);
				assert(z_values[offs_mx_my] == z);
				
				auto l_sqr = std::pow(ax - x, 2.0) + std::pow(ay - y, 2.0) + std::pow(z - d, 2.0);

				float ivq = (float)(GENERAL_MULTIPLIER / l_sqr);

				for (int i = 0; i < N; ++i)
				{
					auto d_tv = std::sqrt(l_sqr) / lambda_profiles[i].velocity;
					float c = (float)(ivq * std::cosf(d_tv));
					float s = (float)(ivq * std::sinf(d_tv));
					
					accum_a[i] += c * intensity;
					accum_b[i] += s * intensity;
					accum_a_mx[i] += c * intensity_mx;
					accum_b_mx[i] += s * intensity_mx;
					accum_a_my[i] += c * intensity_my;
					accum_b_my[i] += s * intensity_my;
					accum_a_mx_my[i] += c * intensity_mx_my;
					accum_b_mx_my[i] += s * intensity_mx_my;
				}
			}
		}

		for (int i = 0; i < N; ++i)
		{
			out[i] = (float)(PI * (std::pow(accum_a[i], 2.0) + std::pow(accum_b[i], 2.0)));
			out_mx[i] = (float)(PI * (std::pow(accum_a_mx[i], 2.0) + std::pow(accum_b_mx[i], 2.0)));
			out_my[i] = (float)(PI * (std::pow(accum_a_my[i], 2.0) + std::pow(accum_b_my[i], 2.0)));
			out_mx_my[i] = (float)(PI * (std::pow(accum_a_mx_my[i], 2.0) + std::pow(accum_b_mx_my[i], 2.0)));
		}
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
		std::cerr << "lambda_red1 = lambda * 1.21" << std::endl;
		std::cerr << "lambda_red2 = lambda * 1.16" << std::endl;
		std::cerr << "lambda_red4 = lambda * 1.10" << std::endl;
		std::cerr << "lambda_green1 = lambda * 1.05" << std::endl;
		std::cerr << "lambda_green2 = lambda * 1.0" << std::endl;
		std::cerr << "lambda_green3 = lambda * 0.95" << std::endl;
		std::cerr << "lambda_blue1 = lambda * 0.91" << std::endl;
		std::cerr << "lambda_blue2 = lambda * 0.86" << std::endl;
		std::cerr << "lambda_blue3 = lambda * 0.82" << std::endl;
		std::cerr << "(go to the source code to change those multipliers)";
		return -1;
	}

	std::string input = argv[1];
	std::string output = argv[2];

	std::cout << "Input: " << input << std::endl;
	std::cout << "Output: " << output << std::endl;

	float d = argc >= 4 ? std::atof(argv[3]) : 200.0;
	float R = argc >= 5 ? std::atof(argv[4]) : 1000.0;
	float lambda = argc >= 6 ? std::atof(argv[5]) : .75;

	std::vector<unsigned char> data;
	unsigned width;
	unsigned height;
	if (lodepng::decode(data, width, height, input) != 0)
	{
		std::cerr << "Failed to open " << input << std::endl;
		return -1;
	}
	if ((width % 2 != 0) || (height % 2 != 0))
	{
		std::cerr << "Image width & high must be an even number (as some optimisations are only possible in that case), please align your image first" << std::endl;
		return -1;
	}

	ThreadGrid _grid{ numWorkerThreads };

	aperture<NUM_COLORS, CLR_STEP> ap{ data, static_cast<int>(width), static_cast<int>(height), d, R, lambda };

	std::array<std::tuple<float, float, float>, NUM_COLORS> wavelenghts_as_rgb;

	std::vector<std::array<float, NUM_COLORS>> out_raw(width* height);

	float wl_max = std::numeric_limits<float>::min();
	float wl_min = std::numeric_limits<float>::max();

	for (int i = 0; i < NUM_COLORS; i++)
	{		
		wl_max = std::max(wl_max, ap.lambda_profiles[i].lambda);
		wl_min = std::min(wl_min, ap.lambda_profiles[i].lambda);
	}

	for (int i = 0; i < NUM_COLORS; i++)
	{
		wavelenghts_as_rgb[i] = wavelength_to_rgb(ap.lambda_profiles[i].lambda, wl_min * 1.04, wl_max / 1.03);
	}

	std::mutex m;
	int progress = 0;

	//report_progress(0, height);

	const auto start = std::chrono::system_clock::now();

	_grid.GridRun(
		[&](int thread_idx, int num_threads)
		{
			int slice = static_cast<int>(1.0 * height/2 / num_threads + 0.999);
			int from = thread_idx * slice;
			int to = std::min<int>(height/2, (thread_idx + 1) * slice);

			for (int y = from; y < to; ++y)
			{
				for (int x = 0; x < width/2; x++)
				{
					ap.diff_value(x, y, out_raw[y * width + x], 
						out_raw[y * width + width - x -1], 
						out_raw[(height - y -1) * width + x],
						out_raw[(height - y - 1) * width + width - x - 1]
						);
				}

				//std::lock_guard l{ m };
				//report_progress(++progress, height);
			}
		});

	const auto end = std::chrono::system_clock::now();

	std::cout << "run duration: " << std::chrono::system_clock::to_time_t(end) - std::chrono::system_clock::to_time_t(start) << " seconds" << std::endl;


	float sum{ 0 };

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; x++)
		{
			float v = 0;
			for (int i = 0; i < NUM_COLORS; ++i)
				v += out_raw[y * width + x][i];
			sum += v / NUM_COLORS;
		}
	}

	float max{ sum / BRIGHT_RATIO }; 

	std::vector<unsigned char> out(width * height * 4);

	for (size_t y = 0; y < height; ++y)
	{
		for (size_t x = 0; x < width; x++)
		{
			size_t i_offs = y * width + x;
			size_t o_offs = 4 * i_offs;

			float r = 0;
			float g = 0;
			float b = 0;

			for (size_t i = 0; i < NUM_COLORS; ++i)
			{
				float v = out_raw[i_offs][i] / max; // value for the given WL
				auto rgb = wavelenghts_as_rgb[i]; // RGB components for the given WL

				r += v * std::get<0>(rgb);
				g += v * std::get<1>(rgb);
				b += v * std::get<2>(rgb);
			}

			out[o_offs + 0] = std::min(255u, static_cast<unsigned>(r * 255.0f / 3.0f));
			out[o_offs + 1] = std::min(255u, static_cast<unsigned>(g * 255.0f / 3.0f));
			out[o_offs + 2] = std::min(255u, static_cast<unsigned>(b * 255.0f / 3.0f));
			out[o_offs + 3] = 255;
		}
	}

	lodepng::encode(output, out, width, height);

	return 0;
}

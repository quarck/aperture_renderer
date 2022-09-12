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
#include "aperture.h"
#include "wavelength_to_rgb.h"


constexpr int NUM_COLORS = 16; // 16
constexpr float CLR_STEP = 1.04427378242741f;
constexpr float DEFAULT_R = 1000.0f;
constexpr float DEFAULT_LAMBDA = .75f; // wavelength! not a functional prog lambda

using apr = aperture_double<NUM_COLORS>;

void report_progress(int value, int total)
{
	std::cout << "\rprogress: [";

	for (int i = 0; i < 60; i++)
	{
		if (i * total < 60 * value)
			std::cout << "=";
		else
			std::cout << "-";
	}
	std::cout << "]" ;
}


int main(int argc, char* argv[])
{
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

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
		std::cerr << "aperture_renderer <input.png> <output.png> [<R>] [<lambda>] [<unfocus_factor>]" << std::endl;
		std::cerr << "default values are: R = " << DEFAULT_R << " (px), lambda = " << DEFAULT_LAMBDA << std::endl;
		std::cerr << "Unfocus factor is the distance where to put the virtual sensor in relationship to the ideal focal distance" 
			<< " - negative values - closer, positive - further, unit is the same as lambda - px" << std::endl;
		std::cerr << "Note: lambda defines the wavelength for the mid-spectrum only, the remdering will be done using " 
			<< NUM_COLORS << " different wavelengths, where i-ths wavelenght is calculated as: " << std::endl;
		std::cerr << "lambdas[i] = pow(" << CLR_STEP << ", " << (NUM_COLORS / 2) << " - i) * lambda " << std::endl;
		std::cerr << "(go to the source code to change those multipliers and consts)" << std::endl;
		std::cerr << "The resulting spectrum will be visualized as a visible light by mapping to visible light spectrum" << std::endl;
		std::cerr << "The distance units used a completely arbitrary, they are in pixes of the orignal image," 
			<< " and thus wavelengths are defined in the same units" << std::endl;
		return -1;
	}

	std::string input = argv[1];
	std::string output = argv[2];

	std::cout << "Input: " << input << std::endl;
	std::cout << "Output: " << output << std::endl;

	float R = argc >= 4 ? static_cast<float>(std::atof(argv[3])) : DEFAULT_R;
	float lambda = argc >= 5 ? static_cast<float>(std::atof(argv[4])) : DEFAULT_LAMBDA;
	float unfocus_factor = argc >= 6 ? static_cast<float>(std::atof(argv[5])) : 0.0f;

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

	apr ap{
		data, 
		static_cast<int>(width),
		static_cast<int>(height), 
		R, 
		lambda, 
		CLR_STEP,
		unfocus_factor 
	};

	std::array<std::tuple<float, float, float>, NUM_COLORS> wavelenghts_as_rgb;

	apr::raw out_raw(width* height);

	float wl_max = std::numeric_limits<float>::min();
	float wl_min = std::numeric_limits<float>::max();

	for (int i = 0; i < NUM_COLORS; i++)
	{		
		wl_max = std::max(wl_max, ap.lambda_profiles[i].lambda);
		wl_min = std::min(wl_min, ap.lambda_profiles[i].lambda);
	}
	
	std::cout << "Input image size: " << width << "x" << height << std::endl;
	std::cout << "R: " << R << ", lambda mid: " << lambda << std::endl;

	std::cout << "Spectrum: " << std::endl;
	for (int i = 0; i < NUM_COLORS; i++)
	{
		float wl = ap.lambda_profiles[i].lambda;
		auto rgb = wavelength_to_rgb(wl , wl_min, wl_max);
		wavelenghts_as_rgb[i] = rgb;
		std::cout << "lambda[" << i << "] = " << wl << ", maps to RGB(" << std::get<0>(rgb) << ", " << std::get<1>(rgb) << ", " << std::get<2>(rgb) << ")" << std::endl;
	}

	std::atomic_int progress = 0;

	report_progress(0, height);

	const auto start = std::chrono::system_clock::now();

	_grid.GridRun(
		[&](int thread_idx, int num_threads)
		{
			for (int y = thread_idx; y < static_cast<int>(height/2); y += num_threads)
			{
				for (int x = 0; x < static_cast<int>(width/2); x++)
				{
					ap.diff_value(x, y, out_raw[y * width + x], 
						out_raw[y * width + width - x - 1], 
						out_raw[(height - y - 1) * width + x],
						out_raw[(height - y - 1) * width + width - x - 1]
						);
				}

				++progress;
				if (thread_idx == 0)
					report_progress(progress.load(), height / 2);
			}
		});

	const auto end = std::chrono::system_clock::now();

	std::cout << std::endl;
	std::cout << "run duration: " << std::chrono::system_clock::to_time_t(end) - std::chrono::system_clock::to_time_t(start) << " seconds" << std::endl;

	float max = static_cast<float>(64.0f * ap.total_light_per_pixel);

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
				float v = static_cast<float>(out_raw[i_offs][i] / max); // value for the given WL
				auto rgb = wavelenghts_as_rgb[i]; // RGB components for the given WL

				r += v * std::get<0>(rgb);
				g += v * std::get<1>(rgb);
				b += v * std::get<2>(rgb);
			}

			out[o_offs + 0] = std::min(255u, static_cast<unsigned>(r * 255.0f));
			out[o_offs + 1] = std::min(255u, static_cast<unsigned>(g * 255.0f));
			out[o_offs + 2] = std::min(255u, static_cast<unsigned>(b * 255.0f));
			out[o_offs + 3] = 255;
		}
	}

	lodepng::encode(output, out, width, height);

	return 0;
}

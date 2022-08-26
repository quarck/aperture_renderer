#pragma once
#include <vector>
#include <array>

constexpr float PI = 3.14159265359f;

constexpr float GENERAL_MULTIPLIER = 10000.0;


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
		: width{ width }
		, height{ height }
		, d{ d }
	{
		intensity_mask.resize(width* height);
		z_values.resize(width* height);

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
					d - std::sqrt(R * R - std::powf(x - cx, 2.0) - std::powf(y - cy, 2.0));
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
		pixel accum_a{ 0 };
		pixel accum_b{ 0 };

		pixel accum_a_mx{ 0 };
		pixel accum_b_mx{ 0 };

		pixel accum_a_my{ 0 };
		pixel accum_b_my{ 0 };

		pixel accum_a_mx_my{ 0 };
		pixel accum_b_mx_my{ 0 };

		int mx = width - 1 - x;
		int my = height - 1 - y;

		for (int ay = 0; ay < height; ++ay)
		{
			for (int ax = 0; ax < width; ++ax)
			{
				int offs = ay * width + ax;
				int offs_mx = ay * width + (width - ax - 1);
				int offs_my = (height - ay - 1) * width + ax;
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

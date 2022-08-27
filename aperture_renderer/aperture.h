#pragma once
#include <vector>
#include <array>

constexpr float PI = 3.14159265359f;

struct lambda_profile
{
	float lambda; // wavelength 
	float velocity; // wave velocity through the medium
	float inverse_velocity;
	//float omega; // we set such a value of v, so omega == 1.0 all the time. All we care is the wavelenght 

	lambda_profile(float lambda)
		: lambda{ lambda }
		, velocity{  }
		, inverse_velocity{ static_cast<float>((2.0 * PI) / lambda ) }
	{
	}

	lambda_profile() : lambda_profile{ 0 } {}

};

template <size_t N>
struct aperture
{
	using pixel = std::array<float, N>;

	// i = y * Width + x
	std::vector<float> intensity_mask;
	std::vector<float> z_sqr_values; // z^2-coordinates of the light emiting plane

	std::array<lambda_profile, N> lambda_profiles;

	int width;
	int height;


	// R is the radius of the 'lense', with the centre at (Width/2.0 - 0.5, Height/2.0 - 0.5, 0), 
	// it affects the curvature of the light wavefront. 
	// The screen is the plane with z==0. 

	aperture(std::vector<unsigned char> img,
		int width, int height, float R, float lambda, float clr_step)
		: width{ width }
		, height{ height }
	{
		intensity_mask.resize(width* height);
		z_sqr_values.resize(width* height);

		// 0.5 factor is subtracted, as the centre is supposedly in between the middle two pixels, 
		// so for more accurate calculations (and to enable symmetry-based optimisations), we
		// subtract that 
		float cx = width / 2.0f - 0.5f;
		float cy = height / 2.0f - 0.5f;

		// Loop through the images pixels to reset color.
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				auto img_offs = 4 * (y * width + x);
				auto dst_offs = y * width + x;

				float r = img[img_offs];
				float g = img[img_offs + 1];
				float b = img[img_offs + 2];

				float v = (r + g + b) / 3.0f / 255.0f;

				intensity_mask[dst_offs] = v > 0.5f ? 1.0 : 0.0;
				z_sqr_values[dst_offs] = R * R - std::powf(x - cx, 2.0) - std::powf(y - cy, 2.0);

				if (z_sqr_values[dst_offs] < 0)
				{
					intensity_mask[dst_offs] = 0.0;
				}
			}
		}

		for (int i = 0; i < N; i++)
		{
			float wl = std::powf(clr_step, static_cast<int>(N) / 2 - i) * lambda;
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

				float z_sqr = z_sqr_values[offs];

				assert(z_sqr_values[offs_mx] == z_sqr);
				assert(z_sqr_values[offs_my] == z_sqr);
				assert(z_sqr_values[offs_mx_my] == z_sqr);

				float l_sqr = std::powf(ax - x, 2.0) + std::powf(ay - y, 2.0) + z_sqr;
				float l = std::sqrtf(l_sqr);
				// Note: generally speaking/ the factor "1.0 / L^2" should be applied to the amplitude of the 
				// wave at distance L from the light point source, this way we can compute physically-correct 
				// distribution of the amplitudes. 
				// However, since the very purpose of this app is to draw the shape of the aperture of the tiny 
				// projection of the point light source at the infinite distance, and assuming that the "aperture" is physically 
				// much large than the projection display, we can conclude that the difference between max(1/L^2) and min(1/L^2) 
				// is mostly negledgible. Furthermore, the longer the focus distance the more negledgible it becomes, so we 
				// define it as a const simply. 

				constexpr float inv_l_sqr = 1.0; // 1.0 / l_sqr; // this factor has almost zero impact on the performance, but kind of brings simulation to the 'exact match' 

				for (int i = 0; i < N; ++i)
				{
					float d_tv = l * lambda_profiles[i].inverse_velocity;
					float c = inv_l_sqr * std::cosf(d_tv);
					float s = inv_l_sqr * std::sinf(d_tv);

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
			out[i] = PI * (std::powf(accum_a[i], 2.0) + std::powf(accum_b[i], 2.0));
			out_mx[i] = PI * (std::powf(accum_a_mx[i], 2.0) + std::powf(accum_b_mx[i], 2.0));
			out_my[i] = PI * (std::powf(accum_a_my[i], 2.0) + std::powf(accum_b_my[i], 2.0));
			out_mx_my[i] = PI * (std::powf(accum_a_mx_my[i], 2.0) + std::powf(accum_b_mx_my[i], 2.0));
		}
	}
};

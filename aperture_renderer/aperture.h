/*
 * The formula used is: 
 *
 *	\omega = 2\pi f
 *
 *	\Delta t_i = \frac{l_i}{v}
 *
 *	a = \sum_i{\frac{cos(\omega \Delta t_i )}{l_i^2}}
 *
 *	b = \sum_i{\frac{sin(\omega \Delta t_i )}{l_i^2}}
 *
 *	E = \pi (a^2 + b^2)
 * 
 * Or, in a more C++-ish style: 
 * 
 *	omega = 2*pi*f;
 *	d_t = l_i / v;
 *	a = sum(cos(omega * d_t) / l_i^2);
 *	b = sum(sin(omega * d_t) / l_i^2);
 *	E = pi * (a^2 + b^2);
 * 
 * Also lambda, wavelength is:
 * 
 *		lambda = v / f;
 * 	
 * thus:
 *  
 *		f = v / lambda;
 * 	
 * and then:
 * 
 *		omega = 2 * pi * v / lambda;
 * 
 * And then:
 * 
 * 		omega * d_t = 2 * pi * v / lambda  * l_i / v;
 * 	
 * 		omega * d_t = 2 * pi * l_i / lambda;
 * 	
 * Let's define '2 * pi / lambda' as 'two_pi_inverse_lambda', then: 
 * 	
 * 		a = sum(cos(two_pi_inverse_lambda * l_i) / l_i^2);
 * 		b = sum(sin(two_pi_inverse_lambda * l_i) / l_i^2);
 * 		E = pi * (a^2 + b^2);
 * 
 * Also we can claim, that most values of l_i are nearly perfectly equal, thus we can drop that factor to simplify calculation
 * 
 * 
*/

#pragma once
#include <vector>
#include <array>

#define _USE_MATH_DEFINES // for C++
#include <cmath>

#include "lambda_profile.h"

#include "kahan.h"

template <size_t N, typename TFloat, bool skip_r_square>
struct aperture
{
	static constexpr TFloat TWO = 2.0;

	using pixel = std::array<TFloat, N>;
	using pixel_acc = std::array<kahan::acc<TFloat>, N>;

	// output format
	using raw = std::vector<pixel>;

	// i = y * Width + x
	std::vector<TFloat> intensity_mask;
	std::vector<TFloat> z_sqr_values; // z^2-coordinates of the light emiting plane

	std::array<lambda_profile<TFloat>, N> lambda_profiles;

	int width;
	int height;

	int ap_skip_x{ 0 };
	int ap_skip_y{ 0 };

	TFloat total_light_per_pixel;

	TFloat unfocus_factor;

	// R is the radius of the 'lense', with the centre at (Width/2.0 - 0.5, Height/2.0 - 0.5, 0), 
	// it affects the curvature of the light wavefront. 
	// The screen is the plane with z==0. 

	aperture(std::vector<unsigned char> img,
		int width, int height, TFloat R, float lambda, float clr_step, TFloat unfocus_factor)
		: width{ width }
		, height{ height }
		, total_light_per_pixel { 0.0 }
		, unfocus_factor{ unfocus_factor  }
	{
		intensity_mask.resize(width* height);
		z_sqr_values.resize(width* height);

		// 0.5 factor is subtracted, as the centre is supposedly in between the middle two pixels, 
		// so for more accurate calculations (and to enable symmetry-based optimisations), we
		// subtract that 
		TFloat cx = width / TWO - 0.5f;
		TFloat cy = height / TWO - 0.5f;

		// Loop through the images pixels to reset color.
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				auto img_offs = 4 * (y * width + x);
				auto dst_offs = y * width + x;

				TFloat r = img[img_offs];
				TFloat g = img[img_offs + 1];
				TFloat b = img[img_offs + 2];

				TFloat v = (r + g + b) / 3.0f / 255.0f;

				intensity_mask[dst_offs] = v > 0.5f ? 1.0 : 0.0;
				z_sqr_values[dst_offs] = 
					R * R 
					- std::pow(x - cx, TWO)
					- std::pow(y - cy, TWO);

				if (std::abs(unfocus_factor) > 0.0001)
				{
					TFloat z = std::sqrt(z_sqr_values[dst_offs]) + unfocus_factor;
					z_sqr_values[dst_offs] = z * z;
				}

				if (z_sqr_values[dst_offs] < 0)
				{
					intensity_mask[dst_offs] = 0.0;
				}

				total_light_per_pixel += intensity_mask[dst_offs];

				if constexpr (!skip_r_square)
				{
					intensity_mask[dst_offs] *= R * R;
				}
			}
		}


		for (int y = 0; y < height/2; y++)
		{
			bool all_zero = true;
			for (int x = 0; x < width; x++)
			{
				if (intensity_mask[y * width + x] != 0.0) // exact! 
				{
					all_zero = false;
					break;
				}
			}

			if (!all_zero)
				break;
			ap_skip_y = y;
		}

		for (int x = 0; x < width/2; x++)
		{
			bool all_zero = true;
			for (int y = 0; y < height; y++)
			{
				if (intensity_mask[y * width + x] != 0.0) // exact! 
				{
					all_zero = false;
					break;
				}
			}
			if (!all_zero)
				break;
			ap_skip_x = x;
		}


		for (int i = 0; i < N; i++)
		{
			float wl = std::powf(clr_step, static_cast<int>(N) / 2 - static_cast<float>(i)) * lambda;
			lambda_profiles[i] = lambda_profile<TFloat>{ wl };
		}
	}

	void diff_value(int x, int y, pixel& out, pixel& out_mx, pixel& out_my, pixel& out_mx_my) noexcept
	{
		pixel_acc accum_a{ 0 };
		pixel_acc accum_b{ 0 };

		pixel_acc accum_a_mx{ 0 };
		pixel_acc accum_b_mx{ 0 };

		pixel_acc accum_a_my{ 0 };
		pixel_acc accum_b_my{ 0 };

		pixel_acc accum_a_mx_my{ 0 };
		pixel_acc accum_b_mx_my{ 0 };

		int mx = width - 1 - x;
		int my = height - 1 - y;

		for (int ay = ap_skip_y; ay < height - ap_skip_y; ++ay)
		{
			for (int ax = ap_skip_x; ax < width - ap_skip_x; ++ax)
			{
				int offs = ay * width + ax;
				int offs_mx = ay * width + (width - ax - 1);
				int offs_my = (height - ay - 1) * width + ax;
				int offs_mx_my = (height - ay - 1) * width + (width - ax - 1);

				TFloat intensity = intensity_mask[offs];
				TFloat intensity_mx = intensity_mask[offs_mx];
				TFloat intensity_my = intensity_mask[offs_my];
				TFloat intensity_mx_my = intensity_mask[offs_mx_my];

				if (intensity == 0 && intensity_mx == 0 && intensity_my == 0 && intensity_mx_my == 0)
					continue;

				TFloat z_sqr = z_sqr_values[offs];

				assert(z_sqr_values[offs_mx] == z_sqr);
				assert(z_sqr_values[offs_my] == z_sqr);
				assert(z_sqr_values[offs_mx_my] == z_sqr);

				TFloat l_sqr = std::pow(ax - x, TWO) + std::pow(ay - y, TWO) + z_sqr;
				TFloat l = std::sqrt(l_sqr);

				// Note: generally speaking/ the factor "1.0 / L^2" should be applied to the amplitude of the 
				// wave at distance L from the light point source, this way we can compute physically-correct 
				// distribution of the amplitudes. 
				// However, since the very purpose of this app is to draw the shape of the aperture of the tiny 
				// projection of the point light source at the infinite distance, and assuming that the "aperture" is physically 
				// much large than the projection display, we can conclude that the difference between max(1/L^2) and min(1/L^2) 
				// is mostly negledgible. Furthermore, the longer the focus distance the more negledgible it becomes, so we 
				// define it as a const simply. 

				// this factor has almost zero impact on the performance, but kind of brings simulation to the 'exact match' 
				const TFloat inv_l_sqr = skip_r_square ? 1.0 : (1.0 / l_sqr); 

				for (int i = 0; i < N; ++i)
				{
					TFloat d_tv = l * lambda_profiles[i].two_pi_inverse_lambda;
					TFloat c = inv_l_sqr * std::cos(d_tv);
					TFloat s = inv_l_sqr * std::sin(d_tv);

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
			static constexpr TFloat PI = static_cast<TFloat>(M_PI);

			out[i] = PI * (std::pow(accum_a[i], TWO) + std::pow(accum_b[i], TWO)) ;
			out_mx[i] = PI * (std::pow(accum_a_mx[i], TWO) + std::pow(accum_b_mx[i], TWO));
			out_my[i] = PI * (std::pow(accum_a_my[i], TWO) + std::pow(accum_b_my[i], TWO));
			out_mx_my[i] = PI * (std::pow(accum_a_mx_my[i], TWO) + std::pow(accum_b_mx_my[i], TWO));
		}
	}
};

template <size_t N>
using aperture_double = aperture<N, double, false>;

template <size_t N>
using aperture_double_no_rsqr = aperture<N, double, true>;

template <size_t N>
using aperture_float = aperture<N, float, false>;

template <size_t N>
using aperture_float_no_rsqr = aperture<N, float, true>;

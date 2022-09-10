#pragma once

namespace kahan
{
	//
	// a wrapper for Kahan algorithm summation of float-based objects
	//
	template <typename TValue>
	struct acc
	{
		TValue value{};
		TValue compensation{};

		acc()
		{
		}

		acc(const TValue& v)
			: value{ v }
		{
		}

		acc(const acc<TValue>& a) = default;

		inline acc<TValue>& operator+=(const TValue& input) noexcept
		{
			auto y = input - compensation;
			auto t = value + y;
			compensation = (t - value) - y;
			value = t;
			return *this;
		}

		operator TValue() noexcept
		{
			return value;
		}
	};

	template <typename TValue>
	inline acc<TValue> operator+(const acc<TValue>& a, const TValue& input) noexcept
	{
		acc<TValue> ret{ a };
		ret += input;
		return ret;
	}


	using acc_float = acc<float>;
	using acc_double = acc<double>;
	
}
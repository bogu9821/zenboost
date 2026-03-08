#pragma once

#include <cstddef>
#include <limits>
#include <type_traits>
#include <span>

namespace zenboost
{
	namespace string
	{
		template<typename T, std::size_t Size, auto GenerationFunc>
		struct TableGenerator
		{
			using UnsignedT = std::make_unsigned_t<T>;

			struct Table
			{
				T m_table[Size];
			};

			static constexpr Table generate()
			{
				Table t;
				for (std::size_t i = 0; i < Size; ++i)
				{
					t.m_table[i] = GenerationFunc(static_cast<T>(i));
				}

				return t;
			}

			static constexpr auto lookup(const T& t_value)
			{
				return s_table.m_table[static_cast<UnsignedT>(t_value)];
			}

			static constexpr auto s_table = generate();
		};
		
//TODO: don't undef globally	
#undef max
		using ToUpperTable = TableGenerator<char, std::numeric_limits<unsigned char>::max() + 1, 
			[](const char t_char)
			{
				return t_char >= 'a' && t_char <= 'z'
					? static_cast<char>(static_cast<unsigned char>(t_char) - ('a' - 'A'))
					: t_char;
			}>;

		/*
		using ToLowerTable = TableGenerator<char, std::numeric_limits<unsigned char>::max() + 1, 
			[](const char t_char)
			{
				return t_char >= 'A' && t_char <= 'B'
					? static_cast<char>(static_cast<unsigned char>(t_char) + ('a' - 'A'))
					: t_char;
			}> ;
		*/

		template<std::size_t Size>
		struct FixedStr
		{
			constexpr FixedStr(const char(&source)[Size - 1])
			{
				for (std::size_t i = 0; i < Size - 1; ++i)
				{
					m_array[i] = source[i];

					if (m_array[i] == '\0')
					{
						m_realSize = i;
						break;
					}
				}
			}

			template<std::size_t LeftSize, std::size_t RightSize>
			constexpr FixedStr(const FixedStr<LeftSize>& t_left, const FixedStr<RightSize>& t_right)
			{
				static_assert(LeftSize + RightSize == Size);

				const auto leftSize = t_left.size();
				const auto rightSize = t_right.size();

				for (std::size_t i = 0; i < leftSize; ++i)
				{
					m_array[i] = t_left.m_array[i];
				}

				for (size_t i = 0; i < rightSize; ++i)
				{
					m_array[leftSize + i] = t_right.m_array[i];
				}

				m_realSize = leftSize + rightSize;
			}

			constexpr std::size_t size() const noexcept
			{
				return m_realSize;
			}

			template<typename ViewT>
			constexpr ViewT as_view() & noexcept
			{
				return ViewT{ m_array, size() };
			}

			template<typename ViewT>
			constexpr ViewT as_view() const& noexcept
			{
				return ViewT{ m_array, size() };
			}

			template<std::size_t LeftSize, std::size_t RightSize>
			friend constexpr auto operator+(const FixedStr<LeftSize>& t_left, const FixedStr<RightSize>& t_right);

			char m_array[Size]{};
			std::size_t m_realSize = Size - 1;
		};

		template<std::size_t Size>
		FixedStr(const char(&)[Size]) -> FixedStr<Size + 1>;

		template<std::size_t LeftSize, std::size_t RightSize>
		FixedStr(FixedStr<LeftSize> t_left, FixedStr<RightSize> t_right) -> FixedStr<LeftSize + RightSize>;

		template<std::size_t LeftSize, std::size_t RightSize>
		constexpr auto operator+(const FixedStr<LeftSize>& t_left, const FixedStr<RightSize>& t_right)
		{
			return FixedStr{ t_left, t_right };
		};

		constexpr void to_upper(const std::span<char> t_str)
		{
			for (std::size_t i = 0; i < t_str.size(); ++i)
			{
				t_str[i] = ToUpperTable::lookup(t_str[i]);
			}
		}

		template<std::size_t Size>
		struct FixedUpperStr : FixedStr<Size>
		{
			consteval FixedUpperStr(const char(&source)[Size - 1]) 
				: FixedStr<Size>{ source }
			{
				to_upper(this->as_view<std::span<char>>());
			}
		};

		template<std::size_t Size>
		FixedUpperStr(const char(&)[Size]) -> FixedUpperStr<Size + 1>;

		inline void to_upper(ZENGIN_NAMESPACE::zSTRING& t_str)
		{
			(void)t_str.Upper();
		}

		template<std::size_t Size>
		constexpr void to_upper(FixedStr<Size>& t_str)
		{
			to_upper(t_str.as_view<std::span<char>>());
		}

		inline std::string_view zstr_to_view(const ZENGIN_NAMESPACE::zSTRING& t_string)
		{
			return std::string_view{ t_string.ToChar(), static_cast<size_t>(t_string.Length()) };
		}

		struct string_hash
		{
			using is_transparent = void;
			[[nodiscard]] size_t operator()(const char* const txt) const
			{
				return std::hash<std::string_view>{}(txt);
			}
			[[nodiscard]] size_t operator()(std::string_view txt) const
			{
				return std::hash<std::string_view>{}(static_cast<std::string_view&&>(txt));
			}
			[[nodiscard]] size_t operator()(const std::string& txt) const
			{
				return std::hash<std::string>{}(txt);
			}
		};
	}
}

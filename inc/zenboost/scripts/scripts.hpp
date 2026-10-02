#pragma once

#include "zenboost/string.hpp"
#include <concepts>
#include <string_view>
#include <string>
#include <type_traits>
#include <utility>

namespace zenboost
{
	namespace scripts
	{
		namespace detail
		{
			template<typename T>
			concept SingleLevelPointer = (std::is_pointer_v<std::decay_t<T>> && !std::is_pointer_v<std::remove_pointer_t<T>>);
		}

		enum class DaedalusType : int
		{
			void_ = 0,
			float_ = 1,
			int_ = 2,
			string = 3,
			function = 5,
			instance = 7
		};

		inline constexpr auto daedalus_flag_return = int{ 2 };
		inline constexpr auto daedalus_flag_external = int{ 8 };
		inline constexpr auto daedalus_token_push_int = int{ 64 };
		inline constexpr auto daedalus_token_push_string = int{ 66 };

		// Parser types live in separate Gothic_* namespaces.  Keep the public
		// helpers generic so an includer does not have to select one of them.
		template<typename T>
		using ParserSymbol = std::remove_pointer_t<decltype(std::declval<T&>().GetSymbol(int{}))>;

		template<typename T>
		using ParserString = std::remove_pointer_t<decltype(std::declval<T&>().PopString())>;

		//TODO: maybe use binary serach?
		template<bool MakeNameUpper = true, typename T>
		inline int parser_get_index(const T& t_parser, const std::string_view t_name)
		{
			[[maybe_unused]] std::string toUpperBuffer{};
			const auto get_symbol_name = [t_name, &toUpperBuffer]
			{
				if constexpr (MakeNameUpper == false)
				{
					return t_name;

				}
				else
				{
					toUpperBuffer = t_name;
					string::to_upper(toUpperBuffer);
					return std::string_view{ toUpperBuffer };
				}
			};

			const auto symbolToFind = get_symbol_name();
			const auto& symbTab = t_parser.symtab.table;
			for (int i = 0; i < symbTab.GetNum(); i++)
			{
				if (string::zstr_to_view(symbTab[i]->name) == symbolToFind)
				{
					return i;
				}
			}

			return -1;
		}

		template<typename T>
		inline auto parser_get_symbol(const T& t_parser, const int t_index) -> ParserSymbol<T>*
		{
			if (t_index < 0 || t_index >= t_parser.symtab.table.GetNum()) [[unlikely]]
			{
				return {};
			}

			return t_parser.symtab.table[t_index];
		}

		template<typename T>
		inline auto parser_get_symbol(const T& t_parser, const std::string_view t_name) -> ParserSymbol<T>*
		{
			const auto index = parser_get_index(t_parser, t_name);

			if (index == -1) [[unlikely]]
			{
				return {};
			}

			return t_parser.symtab.table[index];
		}


		struct DaedalusFunction
		{
			constexpr explicit DaedalusFunction(const int t_index)
				: m_index(t_index)
			{
			};
	
			template<typename T>
			auto GetSymbol(T& t_parser) const -> ParserSymbol<T>*
			{
				auto const symb = t_parser.GetSymbol(m_index);//ParserGetSymbol(t_parser, m_index);

				if (!symb) [[unlikely]]
				{
					return nullptr;
				}

				if (symb->type != static_cast<int>(DaedalusType::function)) [[unlikely]]
				{
					return nullptr;
				}

				return symb;
			}

			constexpr bool operator==(const DaedalusFunction& t_function) const = default;

			int m_index{ -1 };
		};

		inline DaedalusFunction find_function(const int t_index)
		{
			return DaedalusFunction{ t_index };
		}

		template<typename ParserT, string::ZenStringLike StringT>
		inline DaedalusFunction find_function(const ParserT& t_parser, const StringT& t_name)
		{
			return DaedalusFunction{ parser_get_index(t_parser, string::zstr_to_view(t_name)) };
		}

		template<bool MakeUpper = true, typename T>
		inline DaedalusFunction find_function(const T& t_parser, const std::string_view t_name)
		{
			return DaedalusFunction{ parser_get_index<MakeUpper>(t_parser, t_name) };
		}


		struct DaedalusVoid {};

		template<typename T>
		concept DaedalusData =
			std::is_same_v<std::decay_t<T>, int>
			|| std::is_same_v<std::decay_t<T>, DaedalusFunction>
			|| std::is_same_v<std::decay_t<T>, float>
			|| string::ZenStringLike<std::decay_t<T>>
			|| detail::SingleLevelPointer<T>;

		template<typename T>
		concept DaedalusReturn =
			std::is_same_v<T, int>
			//|| std::is_same_v<T, DaedalusFunction>
			|| std::is_same_v<T, float>
			|| string::ZenStringLike<T>
			|| std::is_same_v<T, DaedalusVoid> || std::is_void_v<T>
			|| detail::SingleLevelPointer<T>;


		template<DaedalusData T>
		constexpr int data_type_to_enum()
		{
			if constexpr (std::same_as<T, int>)
			{
				return static_cast<int>(DaedalusType::int_);
			}
			else if constexpr (std::same_as<T, scripts::DaedalusFunction>)
			{
				return static_cast<int>(DaedalusType::function);
			}
			else if constexpr (string::ZenStringLike<T>)
			{
				return static_cast<int>(DaedalusType::string);
			}
			else if constexpr (std::same_as<T, float>)
			{
				return static_cast<int>(DaedalusType::float_);
			}
			else if constexpr (std::is_pointer_v<T>)
			{
				return static_cast<int>(DaedalusType::instance);
			}
			else if constexpr (std::same_as<T, scripts::DaedalusVoid>)
			{
				return static_cast<int>(DaedalusType::void_);
			}
			else
			{
				throw;
			}
		}

		template<DaedalusReturn T>
		inline constexpr int return_type_to_enum()
		{
			if constexpr (std::is_same_v<T, int>)
			{
				return static_cast<int>(DaedalusType::int_);
			}
			else if constexpr (string::ZenStringLike<T>)
			{
				return static_cast<int>(DaedalusType::string);
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return static_cast<int>(DaedalusType::float_);
			}
			else if constexpr (std::is_pointer_v<T>)
			{
				return static_cast<int>(DaedalusType::instance);
			}
			else if constexpr (std::is_void_v<T>)
			{
				return static_cast<int>(DaedalusType::void_);
			}
		}
	}
}

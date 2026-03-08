#pragma once

#include "zenboost/string.hpp"
#include <concepts>

namespace zenboost
{
	namespace scripts
	{
		namespace detail
		{
			template<typename T>
			concept SingleLevelPointer = (std::is_pointer_v<std::decay_t<T>> && !std::is_pointer_v<std::remove_pointer_t<T>>);
		}

		using ParserSymbol = GOTHIC_NAMESPACE::zCPar_Symbol;
		using Parser = GOTHIC_NAMESPACE::zCParser;

		//TODO: maybe use binary serach?
		template<bool ToUpper = true>
		inline constexpr int parser_get_index(const Parser& t_parser, const std::string_view t_name)
		{
			//TODO use better way
			[[maybe_unused]]
			const auto upperName = std::string{};// ToUpper ? StrViewToUpperZengin(t_name) : std::string{};

			if constexpr (ToUpper)
			{
				//t_name = std::string_view{ upperName };
			}

			const auto& symbTab = t_parser.symtab.table;

			for (int i = 0; i < symbTab.GetNum(); i++)
			{
				if (string::zstr_to_view(symbTab[i]->name) == t_name)
				{
					return i;
				}
			}

			return -1;
		}

		inline constexpr ParserSymbol* parser_get_symbol(const Parser& t_parser, const int t_index)
		{
			if (t_index < 0 || t_index >= t_parser.symtab.table.GetNum()) [[unlikely]]
			{
				return {};
			}

			return t_parser.symtab.table[t_index];
		}

		inline constexpr ParserSymbol* parser_get_symbol(const Parser& t_parser, const std::string_view t_name)
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
	
			ParserSymbol* GetSymbol(Parser& t_parser) const
			{
				auto const symb = t_parser.GetSymbol(m_index);//ParserGetSymbol(t_parser, m_index);

				if (!symb)
				{
					return nullptr;
				}

				if (symb->type != GOTHIC_NAMESPACE::zPAR_TYPE_FUNC)
					//|| !(symb->flags & GOTHIC_NAMESPACE::zPAR_FLAG_CONST))
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

		template<typename zSTR = ZENGIN_NAMESPACE::zSTRING>
			requires(std::same_as<zSTR, ZENGIN_NAMESPACE::zSTRING>)
		inline DaedalusFunction find_function(const Parser& t_parser, const zSTR& t_name)
		{
			return DaedalusFunction{ parser_get_index(t_parser, string::zstr_to_view(t_name)) };
		}

		inline DaedalusFunction find_function(const Parser& t_parser, const std::string_view& t_name)
		{
			return DaedalusFunction{ parser_get_index(t_parser, t_name) };
		}


		struct DaedalusVoid {};

		template<typename T>
		concept DaedalusData =
			std::is_same_v<std::decay_t<T>, int>
			|| std::is_same_v<std::decay_t<T>, DaedalusFunction>
			|| std::is_same_v<std::decay_t<T>, float>
			|| std::is_same_v<std::decay_t<T>, GOTHIC_NAMESPACE::zSTRING>
			|| detail::SingleLevelPointer<T>;

		template<typename T>
		concept DaedalusReturn =
			std::is_same_v<T, int>
			//|| std::is_same_v<T, DaedalusFunction>
			|| std::is_same_v<T, float>
			|| std::is_same_v<T, GOTHIC_NAMESPACE::zSTRING>
			|| std::is_same_v<T, DaedalusVoid>
			|| detail::SingleLevelPointer<T>;
	}
}

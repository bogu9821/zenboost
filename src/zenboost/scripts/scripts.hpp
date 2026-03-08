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
		template<bool MakeNameUpper = true>
		inline constexpr int parser_get_index(const Parser& t_parser, const std::string_view t_name)
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

				if (!symb) [[unlikely]]
				{
					return nullptr;
				}

				if (symb->type != GOTHIC_NAMESPACE::zPAR_TYPE_FUNC) [[unlikely]]
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

		template<bool MakeUpper = true>
		inline DaedalusFunction find_function(const Parser& t_parser, const std::string_view t_name)
		{
			return DaedalusFunction{ parser_get_index<MakeUpper>(t_parser, t_name) };
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


		template<DaedalusData T>
		constexpr int data_type_to_enum()
		{
			using namespace ZENGIN_NAMESPACE;
			if constexpr (std::same_as<T, int>)
			{
				return zPAR_TYPE_INT;
			}
			else if constexpr (std::same_as<T, scripts::DaedalusFunction>)
			{
				return zPAR_TYPE_FUNC;
			}
			else if constexpr (std::same_as<T, zSTRING>)
			{
				return zPAR_TYPE_STRING;
			}
			else if constexpr (std::same_as<T, float>)
			{
				return zPAR_TYPE_FLOAT;
			}
			else if constexpr (std::is_pointer_v<T>)
			{
				return zPAR_TYPE_INSTANCE;
			}
			else if constexpr (std::same_as<T, scripts::DaedalusVoid>)
			{
				return zPAR_TYPE_VOID;
			}
			else
			{
				throw;
			}
		}

		template<DaedalusReturn T>
		inline constexpr int return_type_to_enum()
		{
			using namespace ZENGIN_NAMESPACE;
			if constexpr (std::is_same_v<T, int>)
			{
				return zPAR_TYPE_INT;
			}
			else if constexpr (std::is_same_v<T, zSTRING>)
			{
				return zPAR_TYPE_STRING;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				return zPAR_TYPE_FLOAT;
			}
			else if constexpr (std::is_pointer_v<T>)
			{
				return zPAR_TYPE_INSTANCE;
			}
			else if constexpr (std::is_void_v<T>)
			{
				return zPAR_TYPE_VOID;
			}
		}
	}
}

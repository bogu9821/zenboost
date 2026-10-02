#pragma once

#include "zenboost/scripts/scripts.hpp"
#include "zenboost/string.hpp"
#include "zenboost/hook.hpp"

#include <string_view>
#include <vector>
#include <cstddef>
#include <unordered_map>
#include <forward_list>
#include <tuple>
#include <concepts>


#define ZENBOOST_EXTERNAL(function) \
    ::zenboost::externals::DaedalusExternal<#function, function>

#define ZENBOOST_EXTERNAL_WITH_CONDITION(function, condition) \
    ::zenboost::externals::DaedalusExternal<#function, function, condition>

#define ZENBOOST_EXTERNAL_TABLE(name, parserPointer, ...) \
    inline const ::zenboost::externals::ExternalTable<__VA_ARGS__> \
        name{ parserPointer }

namespace zenboost
{
	namespace externals
	{
		namespace detail
		{
			template<typename T>
			struct MemberFunctionFirstArgument;

			template<typename ReturnT, typename ClassT, typename ArgumentT, typename... ArgumentTs>
			struct MemberFunctionFirstArgument<ReturnT(ClassT::*)(ArgumentT, ArgumentTs..., ...)>
			{
				using Type = std::remove_cvref_t<ArgumentT>;
			};

			template<typename ParserT>
			using ParserString = typename MemberFunctionFirstArgument<decltype(&ParserT::DefineExternal)>::Type;

			template<typename T> 
			struct FunctionPointerData;

			template<typename T, typename... Args>
			struct FunctionPointerUnpacked
			{
				using ReturnType = T;
				using ArgumentTypes = std::tuple<Args...>;
				static constexpr std::size_t s_argumentNum = sizeof...(Args);
				static constexpr bool s_returnValue = !std::same_as<std::decay_t<T>, void>;
			};

			template<typename T, typename... Args>
			struct FunctionPointerData<T(*)(Args...) noexcept(false)>
				: FunctionPointerUnpacked<T, Args...>

			{
				using Pointer = T(*)(Args...) noexcept(false);
			};

			template<typename T, typename... Args>
			struct FunctionPointerData<T(*)(Args..., ...) noexcept(false)>
				: FunctionPointerUnpacked<T, Args...>
			{
				using Pointer = T(*)(Args..., ...) noexcept(false);
			};

			template<typename T, typename... Args>
			struct FunctionPointerData<T(*)(Args...) noexcept(true)>
				: FunctionPointerUnpacked<T, Args...>
			{
				using Pointer = T(*)(Args...) noexcept(true);
			};

			template<typename T, typename... Args>
			struct FunctionPointerData<T(*)(Args..., ...) noexcept(true)>
				: FunctionPointerUnpacked<T, Args...>
			{
				using Pointer = T(*)(Args..., ...) noexcept(true);
			};

			template<scripts::DaedalusData T, typename ParserT>
			inline constexpr auto stack_pop_data(ParserT& t_parser);

			template<scripts::DaedalusReturn T, bool PerParserInstance = false, typename ParserT = void>
			auto get_external_return_buffer() -> decltype(auto);

			template<typename T1, typename T2>
			constexpr bool are_externals_same()
			{
				return T1::s_name == T2::s_name;
			}

			template<typename... Types>
			inline constexpr bool are_externals_unique_v = true;

			template<typename T, typename... Types>
			inline constexpr bool are_externals_unique_v<T, Types...> =
				((!are_externals_same<T, Types>()) && ...) && are_externals_unique_v<Types...>;

			auto apply_hook();
		}

		struct BaseExternal
		{
			static void define_external(const auto& t_table) {};
		};

		template<string::FixedStr Name, auto Callable, auto ConditionFunc = nullptr, bool PerParserInstance = false>
			// unnamed lambda doesn't have correct function name - don't support them
			requires(Name[0] != '[')
		struct DaedalusExternal final 
		{
			using CallableType = decltype(Callable);
			using NameType = decltype(Name);

			static constexpr bool s_isFunctionPointer = std::is_pointer_v<CallableType>
				&& std::is_function_v<typename std::remove_pointer_t<CallableType>>;

			static constexpr const NameType& s_name = Name;
			static constexpr const CallableType& s_callable = Callable;
		
			using SelfType = DaedalusExternal<Name, Callable, ConditionFunc>;
			using CallableInfo = typename detail::FunctionPointerData<
				std::conditional_t<s_isFunctionPointer, CallableType, decltype(+Callable)>
			>;

			using ReturnType = CallableInfo::ReturnType;

			static constexpr bool condition()
			{
				if constexpr (!std::same_as<decltype(ConditionFunc), std::nullptr_t>)
				{
					return ConditionFunc();
				}
				else
				{
					return true;
				}
			}

			template<typename ParserT>
			static int __cdecl definition()
			{
				[currentParser = ParserT::cur_parser] <std::size_t... Is>(std::index_sequence<Is...>) [[msvc::forceinline]]
				{
					if constexpr (!CallableInfo::s_returnValue)
					{
						[[msvc::flatten]]
						Callable(detail::stack_pop_data<std::decay_t<std::tuple_element_t<Is, CallableInfo::ArgumentTypes>>, ParserT>(*currentParser)...);
						
					}
					else
					{
						using ReturnDecay = std::decay_t<ReturnType>;
						decltype(auto) returnValue = detail::get_external_return_buffer<ReturnDecay, PerParserInstance, ParserT>();
						{
							[[msvc::flatten]]
							returnValue = Callable(detail::stack_pop_data<std::decay_t<std::tuple_element_t<Is, CallableInfo::ArgumentTypes>>, ParserT>(*currentParser)...);
						}
						currentParser->SetReturn(returnValue);
					}


				}(std::make_index_sequence<CallableInfo::s_argumentNum>());

				return 0;
			}

			
			template<typename ParserT>
			static void define_external(ParserT& t_parser)
			{
				if (!condition())
				{
					return;
				}

				using scripts::data_type_to_enum;
				using scripts::return_type_to_enum;
				static detail::ParserString<ParserT> funcName;
				funcName = Name.as_view<std::string_view>().data();

				auto const par = &t_parser;

				if constexpr (CallableInfo::s_argumentNum != 0)
				{
					[&] <std::size_t... Is>(std::index_sequence<Is...>)
					{
						par->DefineExternal(funcName, &DaedalusExternal::template definition<ParserT>, return_type_to_enum<std::decay_t<ReturnType>>(), data_type_to_enum<std::decay_t<std::tuple_element_t<Is, typename CallableInfo::ArgumentTypes>>>()..., 0);

					}(std::make_index_sequence<CallableInfo::s_argumentNum>());

				}
				else
				{
					par->DefineExternal(funcName, &DaedalusExternal::template definition<ParserT>, return_type_to_enum<std::decay_t<ReturnType>>(), 0);
				}
			}

		};

		struct BaseExternalTable
		{
			explicit BaseExternalTable(void* const t_parser)
				: m_parser(t_parser)
			{
				get_table_registry().push_back(this);
			}

			virtual ~BaseExternalTable()
			{
				std::erase(get_table_registry(), this);
			}

			virtual void define() const = 0;

			static void register_tables(const void* t_parser)
			{
				for (const auto table : get_table_registry())
				{
					if (table->m_parser == t_parser)
					{
						table->define();
					}
				}
			}

			void* m_parser{};

		private:
			static std::vector<BaseExternalTable*>& get_table_registry()
			{
				// we want to be sure that vector destructor won't be called
				// and memory will remain until program termination
				// because BaseExternalTable may add/erase element from the vector
				union Table
				{
					std::vector<BaseExternalTable*> m_storage{};
					constexpr ~Table() {};
				};

				static Table table{};
				return table.m_storage;
			}
		};

		template<typename...Args>
			requires detail::are_externals_unique_v<Args...>
		using ExternalsTuple = std::tuple<Args...>;


		template<typename... Args>
		struct ExternalTable final 
			: public BaseExternalTable
		{
			using Table = ExternalsTuple<Args...>;
			using DefineFunction = void(*)(const ExternalTable&);

			template<typename ParserT>
			explicit ExternalTable(ParserT* const t_parser)
				: BaseExternalTable(t_parser)
				, m_define{ &define_for_parser<ParserT> }
			{
			}

			void define() const override
			{
				m_define(*this);
			}

		private:
			template<typename ParserT>
			static void define_for_parser(const ExternalTable& t_table)
			{
				auto* const parser = static_cast<ParserT*>(t_table.m_parser);
				[&] <std::size_t... Is>(std::index_sequence<Is...>)
				{
					((std::tuple_element_t<Is, Table>::template define_external<ParserT>(*parser)), ...);
				}(std::make_index_sequence<std::tuple_size_v<Table>>{});
			}

			DefineFunction m_define{};
		};


		namespace detail
		{
			template<scripts::DaedalusData T, typename ParserT>
			inline constexpr auto stack_pop_data(ParserT& t_parser)
			{
				if constexpr (std::same_as<T, int> || std::same_as<T, scripts::DaedalusFunction>)
				{
					int parameter;
					t_parser.GetParameter(parameter);
					return T{ parameter };
				}
				else if constexpr (std::same_as<T, float>)
				{
					float parameter;
					t_parser.GetParameter(parameter);
					return parameter;
				}
				else if constexpr (string::ZenStringLike<T>)
				{
					return std::cref(*t_parser.PopString());
				}
				else if constexpr (std::is_pointer_v<T>)
				{
					return static_cast<T>(t_parser.GetInstance());
				}
				else
				{
					throw;
				}
			}

			template<scripts::DaedalusReturn T, bool PerParserInstance, typename ParserT>
			auto get_external_return_buffer() -> decltype(auto)
			{
				using VarType = std::decay_t<T>;
				static constexpr auto isStr = string::ZenStringLike<VarType>;
				if constexpr (isStr == false)
				{
					return VarType{};
				}
				else
				{
					if constexpr (PerParserInstance == false)
					{
						static scripts::ParserString<ParserT> str{};
						return (str);
					}
					else
					{
						// TODO: 
						// 1. maybe don't use that much STL to improve compilation
						// 2. maybe clear buffer on parser datastack clear
						using StringPool = std::forward_list<scripts::ParserString<ParserT>>;
						using PerParser = std::unordered_map<const ParserT*, StringPool>;
						static PerParser buffers;
						auto& str = buffers[ParserT::cur_parser].emplace_front();
						return (str);
					}
				}
			}

			int __fastcall zCParser__LoadDat_hook(void* t_parser, void* t_edx, void* t_datName);

			auto apply_hook()
			{
				// TODO: remove gothic-api dependency
				//const auto hookAddress = zSwitch(0x006495B0, 0x006715F0, 0x00677A00, 0x006D4780);
				const auto hookAddress = zSwitch(0x006E5680, 0x0071E130, 0x0072EEC0, 0x0078E900);
				return zenboost::hook::create_hook(zCParser__LoadDat_hook, hookAddress);
			}
			
			auto externalsHook = apply_hook();

			int __fastcall zCParser__LoadDat_hook(void* t_parser, void* t_edx, void* t_datName)
			{
				BaseExternalTable::register_tables(t_parser);
				const auto result = externalsHook(t_parser, t_edx, t_datName);				
				return result;
			}
			

		}
	}
}

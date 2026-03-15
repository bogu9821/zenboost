#pragma once

#include "zenboost/scripts/scripts.hpp"
#include "zenboost/string.hpp"

#include <string_view>
#include <vector>
#include <cstddef>
#include <unordered_map>
#include <forward_list>
#include <tuple>
#include <concepts>

namespace zenboost
{
	namespace externals
	{
		namespace detail
		{
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

			template<scripts::DaedalusData T>
			inline constexpr auto stack_pop_data(scripts::Parser& t_parser);

			template<scripts::DaedalusReturn T, bool PerParserInstance = false>
			auto get_external_return_buffer() -> decltype(auto);

			template<typename T1, typename T2>
			constexpr bool are_externals_same()
			{
				return T1::s_name == T2::s_name;
			}

			template<typename T, typename... Types>
			constexpr bool are_externals_unique_v = ((!are_externals_same<T, Types>()) && ...) && are_externals_unique_v<Types...>;

			template<typename T>
			constexpr bool are_externals_unique_v<T> = true;

		}

		struct BaseExternal
		{
			static void define_external(const auto& t_table) {};
		};

		template<string::FixedStr Name, auto Callable, auto ConditionFunc = nullptr, bool PerParserInstance = false>\
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

			static int __cdecl definition()
			{
				using namespace ZENGIN_NAMESPACE;
				[currentParser = scripts::Parser::cur_parser] <std::size_t... Is>(std::index_sequence<Is...>) [[msvc::forceinline]]
				{
					if constexpr (!CallableInfo::s_returnValue)
					{
						[[msvc::flatten]]
						Callable(detail::stack_pop_data<std::decay_t<std::tuple_element_t<Is, CallableInfo::ArgumentTypes>>>(*currentParser)...);
						
					}
					else
					{
						using ReturnDecay = std::decay_t<ReturnType>;
						decltype(auto) returnValue = detail::get_external_return_buffer<ReturnDecay, PerParserInstance>();
						{
							[[msvc::flatten]]
							returnValue = Callable(detail::stack_pop_data<std::decay_t<std::tuple_element_t<Is, CallableInfo::ArgumentTypes>>>(*currentParser)...);
						}
						currentParser->SetReturn(returnValue);
					}


				}(std::make_index_sequence<CallableInfo::s_argumentNum>());

				return 0;
			}

			
			static void define_external(const auto& t_table)
			{
				if (!condition())
				{
					return;
				}

				using namespace ZENGIN_NAMESPACE;
				using scripts::data_type_to_enum;
				using scripts::return_type_to_enum;
				static zSTRING funcName;
				funcName = Name.as_view<std::string_view>().data();

				auto const par = t_table.m_parser;

				if constexpr (CallableInfo::s_argumentNum != 0)
				{
					[&] <std::size_t... Is>(std::index_sequence<Is...>)
					{
						par->DefineExternal(funcName, &DaedalusExternal::definition, return_type_to_enum<std::decay_t<ReturnType>>(), data_type_to_enum<std::decay_t<std::tuple_element_t<Is, typename CallableInfo::ArgumentTypes>>>()..., 0);

					}(std::make_index_sequence<CallableInfo::s_argumentNum>());

				}
				else
				{
					par->DefineExternal(funcName, &DaedalusExternal::definition, return_type_to_enum<std::decay_t<ReturnType>>(), 0);
				}
			}

		};

		struct BaseExternalTable
		{
			BaseExternalTable()
			{
				get_table_registry().push_back(this);
			}

			virtual ~BaseExternalTable()
			{
				std::erase(get_table_registry(), this);
			}

			virtual void define() const = 0;

			static void register_tables()
			{
				static bool inited{};
				
				if (inited)
				{
					return;
				}

				inited = true;

				for (const auto table : get_table_registry())
				{
					table->define();
				}
		
			}

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

		template<typename E>
		concept IsExternalDefinition = requires(E external, const BaseExternalTable& table)
		{
			external.define_external(table);
		};

		template<typename...Args>
			requires (detail::are_externals_unique_v<Args...>&& (IsExternalDefinition<Args> && ...))
		using ExternalsTuple = std::tuple<Args...>;


		template<typename... Args>
		struct ExternalTable final 
			: public BaseExternalTable
		{
			using Table = ExternalsTuple<Args...>;

			ExternalTable(scripts::Parser* const t_parser)
				: m_parser(t_parser)

			{
			}

			void define() const override
			{
				[&] <std::size_t... Is>(std::index_sequence<Is...>)
				{
					((std::tuple_element_t<Is, Table>::define_external(*this)), ...);
				}(std::make_index_sequence<std::tuple_size_v<Table>>{});
			}

			scripts::Parser* m_parser;
		};


		namespace detail
		{
			template<scripts::DaedalusData T>
			inline constexpr auto stack_pop_data(scripts::Parser& t_parser)
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
				else if constexpr (std::same_as<T, ZENGIN_NAMESPACE::zSTRING>)
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

			template<scripts::DaedalusReturn T, bool PerParserInstance>
			auto get_external_return_buffer() -> decltype(auto)
			{
				using VarType = std::decay_t<T>;
				static constexpr auto isStr = std::same_as<VarType, ZENGIN_NAMESPACE::zSTRING>;
				if constexpr (isStr == false)
				{
					return VarType{};
				}
				else
				{
					if constexpr (PerParserInstance == false)
					{
						static ZENGIN_NAMESPACE::zSTRING str{};
						return (str);
					}
					else
					{
						// TODO: 
						// 1. maybe don't use that much STL to improve compilation
						// 2. maybe clear buffer on parser datastack clear
						using StringPool = std::forward_list<ZENGIN_NAMESPACE::zSTRING>;
						using PerParser = std::unordered_map<const scripts::Parser*, StringPool>;
						static PerParser buffers;
						auto& str = buffers[scripts::Parser::cur_parser].emplace_front();
						return (str);
					}
				}
			}
		}
	}
}

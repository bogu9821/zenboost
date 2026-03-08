#pragma once

#include "zenboost/scripts/scripts.hpp"
#include "zenboost/string.hpp"

#include <string_view>
#include <string>
#include <vector>
#include <cstddef>

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
		}

		struct BaseExternal
		{
			static void DefineExternal(const auto& t_table) {};
		};

		template<string::FixedStr Name, auto Callable, auto ConditionFunc = nullptr>
			requires(Name.as_view<std::string_view>()[0] != '[')
		struct DaedalusExternal final : public BaseExternal
		{
			using CallableType = decltype(Callable);
			using NameType = decltype(Name);

			static constexpr bool s_isFunctionPointer = std::is_pointer_v<CallableType>
				&& std::is_function_v<typename std::remove_pointer_t<CallableType>>;

			static constexpr NameType s_name = Name;
			static constexpr CallableType s_callable = Callable;
		
			using SelfType = DaedalusExternal<Name, Callable, ConditionFunc>;
			using CallableInfo = typename detail::FunctionPointerData<
				std::conditional_t<s_isFunctionPointer, CallableType, decltype(+Callable)>
			>;

			using ReturnType = CallableInfo::ReturnType;

			static constexpr bool condition()
			{
				return ConditionFunc ? ConditionFunc() : true;
			}

			static int __cdecl definition()
			{
				using namespace ZENGIN_NAMESPACE;
				[currentParser = zCParser::cur_parser] <std::size_t... Is>(std::index_sequence<Is...>) [[msvc::forceinline]]
				{
					if constexpr (!CallableInfo::s_returnValue)
					{
						[[msvc::flatten]]
						Callable(GetData<std::decay_t<std::tuple_element_t<Is, CallableInfo::ArgumentTypes>>>(currentParser)...);
						
					}
					else
					{
						auto return_value_buffer = []() -> decltype(auto)
							{
								using VarType = std::decay_t<ReturnType>;
								if constexpr (std::same_as<VarType, zSTRING>)
								{
									static zSTRING str{};
									return (str);
								}
								else
								{
									return VarType{};
								}
							};

						decltype(auto) returnValue = return_value_buffer();
						{
							[[msvc::flatten]]
							returnValue = Callable(GetData<std::decay_t<std::tuple_element_t<Is, CallableInfo::ArgumentTypes>>>(currentParser)...);
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

				t_table.m_nameBuffer = Name.Data().data();

				auto const par = t_table.m_parser;

				if constexpr (CallableInfo::ArgCount != 0)
				{
					[&] <std::size_t... Is>(std::index_sequence<Is...>)
					{
						par->DefineExternal(t_table.m_nameBuffer, &Definition, ReturnToEnum<std::decay_t<ReturnType>>(), TypeToEnum<std::decay_t<std::tuple_element_t<Is, typename CallableInfo::ArgTypes>>>()..., 0);

					}(std::make_index_sequence<CallableInfo::ArgCount>());

				}
				else
				{
					par->DefineExternal(t_table.m_nameBuffer, &Definition, ReturnToEnum<std::decay_t<ReturnType>>(), 0);
				}
			}

		};

		ZENGIN_NAMESPACE::zSTRING test() { return {}; }
		int test2() { return {}; }

		auto g = DaedalusExternal<"aa", test>::Definition;
		auto g2 = DaedalusExternal<"aa", test2>::Definition;

		template<scripts::DaedalusData T>
		inline constexpr auto pop_data(scripts::Parser* const t_parser);




		template<scripts::DaedalusData T>
		inline constexpr auto pop_data(scripts::Parser* const t_parser)
		{
			if constexpr (std::is_same_v<T, int> || std::is_same_v<T, scripts::DaedalusFunction>)
			{
				int parameter;
				t_parser->GetParameter(parameter);
				return T{ parameter };
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				float parameter;
				t_parser->GetParameter(parameter);
				return parameter;
			}
			else if constexpr (std::is_same_v<T, ZENGIN_NAMESPACE::zSTRING>)
			{
				return std::cref(*t_parser->PopString());
			}
			else if constexpr (std::is_pointer_v<T>)
			{
				return static_cast<T>(t_parser->GetInstance());
			}
		}

		namespace detail
		{
			

		}
	}
}
#pragma once
#include "zenboost/string.hpp"
#include "zenboost/scripts/scripts.hpp"

#include <optional>
#include <expected>
#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <concepts>

namespace zenboost
{
	namespace daedaluscall
	{
		enum class eCallFuncError
		{
			wrong_symbol,
			wrong_arg_size,
			wrong_arg_type,
			wrong_return_type
		};

		enum class eClearStack
		{
			clear,
			no_clear
		};

		struct IgnoreReturn {};

		template<typename T>
		concept ReturnType = scripts::DaedalusReturn<T> || std::same_as<T, IgnoreReturn>;

		template<ReturnType T = IgnoreReturn, bool SafeCall = true>
		std::expected<T, eCallFuncError> daedalus_call(
			scripts::Parser* const t_par,
			const scripts::DaedalusFunction t_function,
			const eClearStack t_clearStack,
			scripts::DaedalusData auto...  t_args
		);

		namespace detail
		{
			template<ReturnType T>
			constexpr int should_return();

			struct CallFuncContext
			{
				CallFuncContext(scripts::Parser* const t_par, const scripts::DaedalusFunction t_function);
				template<scripts::DaedalusData DataT>
				inline void push_one_argument(DataT&& t_argument, [[maybe_unused]] const size_t t_index) const;

				template<ReturnType T>
				inline auto return_script_value() const;

				inline void pop_return_value() const;

				template<scripts::DaedalusData... Args>
				constexpr bool check_all_types() const;

				template<ReturnType T>
				inline bool check_type(const size_t t_offset) const;

				template<ReturnType T, scripts::DaedalusData... Args>
				inline std::optional<eCallFuncError> check_call_error() const;

				scripts::Parser* m_parser;
				scripts::ParserSymbol* m_symbol;
				scripts::DaedalusFunction m_function;
			};

			class CallFuncStringCache
			{
			public:

				CallFuncStringCache(const scripts::Parser& t_parser);

				__declspec(noinline) void Add(std::string t_functionName, const scripts::DaedalusFunction t_function);

				[[nodiscard]] std::optional<scripts::DaedalusFunction> FindCache(const std::string_view t_name) const noexcept;

				[[nodiscard]] static CallFuncStringCache& Get(const scripts::Parser& t_parser);

			private:

				using CacheMap = std::unordered_map<std::string, scripts::DaedalusFunction, string::string_hash, std::equal_to<>>;
				const scripts::Parser* m_parser;
				CacheMap m_cache;

				inline static std::vector<CallFuncStringCache> s_cache;
			};
		}


		template<ReturnType T, bool SafeCall>
		std::expected<T, eCallFuncError> daedalus_call(
			scripts::Parser* const t_par,
			const scripts::DaedalusFunction t_function,
			const eClearStack t_clearStack,
			scripts::DaedalusData auto...  t_args
		)
		{
			using namespace zenboost::daedaluscall::detail;
			const CallFuncContext contex{ t_par,t_function };

			if constexpr (SafeCall)
			{
				if (const auto error = contex.check_call_error<T, std::decay_t<decltype(t_args)>...>();
					error.has_value())
				{
					return std::unexpected{ *error };
				}
			}

			if (t_clearStack == eClearStack::clear)
			{
				t_par->datastack.Clear();
			}

			auto PushFunc = [&, counter = 0]() mutable
				{
					((contex.push_one_argument(std::move(t_args), t_function.m_index + counter++)), ...);
				};

			PushFunc();

			if (contex.m_symbol->flags & ZENGIN_NAMESPACE::zPAR_FLAG_EXTERNAL) [[unlikely]]
			{
				const auto cur_par = scripts::Parser::cur_parser;
				const auto cur_instance = scripts::ParserSymbol::instance_sym;

				scripts::Parser::cur_parser = t_par;

				auto const Func = reinterpret_cast<int(*)()>(contex.m_symbol->single_intdata);
				Func();

				scripts::Parser::cur_parser = cur_par;
				scripts::ParserSymbol::SetUseInstance(cur_instance);
			}
			else [[likely]]
			{
				t_par->DoStack(contex.m_symbol->single_intdata);
			}

			if constexpr (std::same_as<T, IgnoreReturn>)
			{
				contex.pop_return_value();
				return {};
			}
			else
			{
				return std::expected<T, eCallFuncError>{ contex.return_script_value<T>() };
			}
		}

		template<size_t N>
		consteval auto function_name(const char(&t_constexprString)[N])
		{
			return string::FixedUpperStr{ t_constexprString };
		}

		auto function_name(const string::ZenString& t_stringType)
		{
			return string::zstr_to_view(t_stringType);
		}

		constexpr auto function_name(const auto& t_stringType)
		{
			return static_cast<std::string_view>(t_stringType);
		}

		template<ReturnType T = IgnoreReturn, bool Cache = true, bool Upper = true, typename StringView = std::string_view>
			requires (std::same_as<StringView, std::string_view>)
		std::expected<T, eCallFuncError> daedalus_call(scripts::Parser* const t_par, const StringView t_name, const eClearStack t_clearStack, scripts::DaedalusData auto...  t_args)
		{
			if constexpr (Cache == false)
			{
				return daedalus_call<T, true>(
					t_par,
					scripts::find_function<Upper>(*t_par, t_name),
					t_clearStack,
					std::move(t_args)...
				);
			}

			static constexpr auto invalidFunction = scripts::DaedalusFunction{ -1 };
			auto& parserCache = detail::CallFuncStringCache::Get(*t_par);
			const auto cachedFunctionIndex = parserCache.FindCache(t_name).value_or(invalidFunction);

			const auto cacheExist = invalidFunction != cachedFunctionIndex;
			const auto functionIndex = cacheExist ? cachedFunctionIndex : scripts::find_function<Upper>(*t_par, t_name);

			const auto result = daedalus_call<T, true>(t_par, functionIndex, t_clearStack, std::move(t_args)...);
			if (result.has_value() && !cacheExist)
			{
				parserCache.Add(std::string{ t_name }, functionIndex);
			}

			return result;
		}

		template<ReturnType T = IgnoreReturn, bool Cache = true, typename ZSTR = ZENGIN_NAMESPACE::zSTRING>
		//hack for implicit zSTRING conversion
			requires(std::same_as<ZSTR, ZENGIN_NAMESPACE::zSTRING>)
		__forceinline std::expected<T, eCallFuncError> daedalus_call(
			scripts::Parser* const t_par,
			const ZSTR& t_name,
			const eClearStack t_clearStack,
			scripts::DaedalusData auto...  t_args
		)
		{
			const auto asView = string::zstr_to_view(t_name);
			return daedalus_call<T, Cache>(t_par, asView, t_clearStack, std::move(t_args)...);
		}

		template<ReturnType T = IgnoreReturn, size_t N>
		__forceinline std::expected<T, eCallFuncError> daedalus_call(
			scripts::Parser* const t_par,
			const string::FixedUpperStr<N> t_name,
			const eClearStack t_clearStack,
			scripts::DaedalusData auto...  t_args
		)
		{
			return daedalus_call<T, true, false>(t_par, t_name.as_view<std::string_view>(), t_clearStack, std::move(t_args)...);
		}

		namespace detail
		{
			template<ReturnType T>
			constexpr int should_return()
			{
				return !(std::same_as<T, scripts::DaedalusVoid> || std::same_as<T, IgnoreReturn>);
			}

			CallFuncContext::CallFuncContext(scripts::Parser* const t_par, const scripts::DaedalusFunction t_function)
				: m_parser(t_par),
				m_function(t_function)
			{
				m_symbol = t_function.GetSymbol(*t_par);
				//m_symbol = ParserGetSymbol(m_parser, t_function.m_index);
			}

			template<scripts::DaedalusData DataT>
			inline void CallFuncContext::push_one_argument(DataT&& t_argument, [[maybe_unused]] const size_t t_index) const
			{
				using ArgType = std::decay_t<DataT>;

				if constexpr (std::is_pointer_v<ArgType>)
				{
					auto const argumentSymbol = m_parser->GetSymbol(t_index);//ParserGetSymbol(m_parser, t_index);

					argumentSymbol->offset = reinterpret_cast<int>(t_argument);
					m_parser->datastack.Push(t_index);
				}
				else if constexpr (std::same_as<ArgType, int>)
				{
					m_parser->datastack.Push(t_argument);
					m_parser->datastack.Push(ZENGIN_NAMESPACE::zPAR_TOK_PUSHINT);
				}
				else if constexpr (std::same_as<ArgType, scripts::DaedalusFunction>)
				{
					m_parser->datastack.Push(t_argument.m_index);
					m_parser->datastack.Push(ZENGIN_NAMESPACE::zPAR_TOK_PUSHINT);
				}
				else if constexpr (std::same_as<ArgType, float>)
				{
					m_parser->datastack.Push(std::bit_cast<int>(t_argument));
					m_parser->datastack.Push(ZENGIN_NAMESPACE::zPAR_TOK_PUSHINT);
				}
				else if constexpr (std::same_as<ArgType, ZENGIN_NAMESPACE::zSTRING>)
				{
					m_parser->datastack.Push(reinterpret_cast<std::intptr_t>(&t_argument));
					m_parser->datastack.Push(ZENGIN_NAMESPACE::zPAR_TOK_PUSHSTR);
				}
			}

			template<ReturnType T>
			inline auto CallFuncContext::return_script_value() const
			{
				if constexpr (std::same_as<T, scripts::DaedalusVoid>)
				{
					return {};
				}
				else if constexpr (std::is_pointer_v<T>)
				{
					//TODO check index?
					return static_cast<T>(m_parser->GetInstance());
				}
				else if constexpr (std::same_as<T, ZENGIN_NAMESPACE::zSTRING>)
				{
					return std::cref(*m_parser->PopString());
				}
				else if constexpr (std::same_as<T, int>)
				{
					return m_parser->PopDataValue();
				}
				else if constexpr (std::same_as<T, float>)
				{
					return m_parser->PopFloatValue();
				}
			}

			inline void CallFuncContext::pop_return_value() const
			{
				switch (m_symbol->offset)
				{
				case ZENGIN_NAMESPACE::zPAR_TYPE_INT:
					(void)return_script_value<int>();
					break;
				case ZENGIN_NAMESPACE::zPAR_TYPE_STRING:
					(void)return_script_value<ZENGIN_NAMESPACE::zSTRING>();
					break;
				case ZENGIN_NAMESPACE::zPAR_TYPE_FLOAT:
					(void)return_script_value<float>();
					break;
				case ZENGIN_NAMESPACE::zPAR_TYPE_INSTANCE:
					(void)return_script_value<void*>();
					break;
				case ZENGIN_NAMESPACE::zPAR_TYPE_VOID:
					break;
				default:
					break;
				}
			}

			template<scripts::DaedalusData... Args>
			constexpr bool CallFuncContext::check_all_types() const
			{
				size_t argumentOffset{};
				bool areArgumentsValid{ true };
				(((!check_type<Args>(argumentOffset++)
					? (areArgumentsValid = false, false) : true)
					&& ...));

				return areArgumentsValid;
			}

			template<ReturnType T>
			inline bool CallFuncContext::check_type(const size_t t_offset) const
			{
				const auto symbol = m_parser->symtab.table[m_function.m_index + 1 + t_offset];
				return symbol->type == static_cast<unsigned int>(scripts::data_type_to_enum<T>());
			}

			template<ReturnType T, scripts::DaedalusData... Args>
			inline std::optional<eCallFuncError> CallFuncContext::check_call_error() const
			{
				if (!m_symbol)
				{
					return eCallFuncError::wrong_symbol;
				}

				if (sizeof...(Args) != m_symbol->ele)
				{
					return eCallFuncError::wrong_arg_size;
				}

				const bool hasReturn = (m_symbol->flags & ZENGIN_NAMESPACE::zPAR_FLAG_RETURN) != 0;

				if (!hasReturn)
				{
					if constexpr (should_return<T>())
					{
						return eCallFuncError::wrong_return_type;
					}
				}

				if constexpr (!std::same_as<T, IgnoreReturn>)
				{
					if (m_symbol->offset != TypeToEnum<std::decay_t<T>>())
					{
						return eCallFuncError::wrong_return_type;
					}
				}

				if constexpr (sizeof...(Args))
				{
					if (!check_all_types<Args...>())
					{
						return eCallFuncError::wrong_arg_type;
					}
				}

				return {};
			}



			CallFuncStringCache::CallFuncStringCache(const scripts::Parser& t_parser)
				: m_parser(&t_parser)
			{
			}

			__declspec(noinline) void CallFuncStringCache::Add(std::string t_functionName, const scripts::DaedalusFunction t_function)
			{
				m_cache.emplace(std::move(t_functionName), t_function);
			}

			[[nodiscard]] std::optional<scripts::DaedalusFunction> CallFuncStringCache::FindCache(const std::string_view t_name) const noexcept
			{
				if (const auto it = m_cache.find(t_name);
					it != std::end(m_cache))
				{
					return it->second;
				}

				return {};
			}

			[[nodiscard]] CallFuncStringCache& CallFuncStringCache::Get(const scripts::Parser& t_parser)
			{
				const auto SameKey = [&t_parser](const auto& t_object)
					{
						return t_object.m_parser == &t_parser;
					};

				auto& value = [&]() -> CallFuncStringCache&
					{
						if (const auto it = std::ranges::find_if(s_cache, SameKey);
							it != std::end(s_cache)) [[likely]]
						{
							return *it;
						}
						else [[unlikely]]
						{
							return s_cache.emplace_back(CallFuncStringCache{ t_parser });
						}

					}();

				return value;
			}

		}
	}
}

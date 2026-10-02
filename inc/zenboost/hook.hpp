#pragma once
#include <cstdint>
#include <bit>

#include <union/Hook.h>
namespace zenboost
{
	namespace hook
	{
		// TODO: create templated base hook providers system
		template<typename Callable>
		auto create_hook(Callable t_hookFunction, std::uintptr_t t_hookedAddress)
		{
			const auto memoryAddress = reinterpret_cast<void*>(t_hookedAddress);
			return Union::CreateHook(memoryAddress, static_cast<Callable&&>(t_hookFunction), Union::HookType::Hook_CallPatch);
		}
	}
}

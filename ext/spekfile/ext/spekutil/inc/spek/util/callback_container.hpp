#pragma once

#include <functional>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace Spek
{
	template<typename... ArgTypes>
	class CallbackContainer
	{
	public:		
		using Callback = std::function<void(ArgTypes...)>;
		using Handle = std::shared_ptr<Callback>;

		[[nodiscard]] Handle Add(Callback a_Callback)
		{
			if (a_Callback == nullptr)
				return nullptr;

			auto t_Pointer = std::make_shared<Callback>(a_Callback);
			m_Callbacks.push_back(t_Pointer);
			return t_Pointer;
		}

		void Call(ArgTypes... a_Args)
		{
			int i = 0;
			for (auto& t_Callback : m_Callbacks)
				(*t_Callback)(a_Args...);
		}

		void Remove(const Handle& ID)
		{
			m_Callbacks.erase(ID);
		}
	
	private:
		std::vector<Handle> m_Callbacks;
	};
}
#include <spek/util/debug.hpp>

#include <ctime>
#include <iostream>

namespace Spek
{
	namespace Debug
	{
		std::mutex g_outputMutex;
		using LockGuard = std::lock_guard<std::mutex>;

		void Break()
		{
#if defined(_WIN32)
			__debugbreak();
#else
			// TODO: implement debugbreak for other OSes
#endif

		}

		DebugBuffer::DebugBuffer(const char* file)
		{
			LockGuard t_LockGuard(g_outputMutex);
			m_file = std::ofstream(file, std::ios::app);
			m_file << "\n\n\n---- New Session ---- \n\n\n";
		}

		DebugBuffer::~DebugBuffer()
		{
			m_file.close();
		}

		DebugBuffer& DebugBuffer::operator<<(const char* data)
		{
			LockGuard t_LockGuard(g_outputMutex);
			if (data == nullptr)
				return *this;

			if (m_shouldPrintTime)
			{
				std::time_t result = std::time(nullptr);
				std::string t_Time = std::string(std::asctime(std::localtime(&result)));
				t_Time = t_Time.substr(0, t_Time.size() - 1);
				m_shouldPrintTime = false;
				m_file << "[" << t_Time << "] ";
			}

			for (int i = 0; data[i] != 0; i++)
			{
				if (data[i] == '\n')
				{
					m_shouldPrintTime = true;
					break;
				}
			}

			m_file << data;
			std::cout << data;
			m_file.flush();
			return *this;
		}

		DebugBuffer& DebugBuffer::SetDec()
		{
			LockGuard lockGuard(g_outputMutex);
			m_integerMode = IntegerMode::INT_DEC;
			return *this;
		}

		DebugBuffer& DebugBuffer::SetHex()
		{
			LockGuard t_LockGuard(g_outputMutex);
			m_integerMode = IntegerMode::INT_HEX;
			return *this;
		}

		void DebugBuffer::SetIntegerMode(IntegerMode a_Mode)
		{
			LockGuard lockGuard(g_outputMutex);
			m_integerMode = a_Mode;
		}

		DebugBuffer& DebugBuffer::operator<<(const std::string& data)
		{
			*this << data.c_str();
			return *this;
		}

		DebugBuffer Output("log.txt");
		DebugBuffer Error("error.txt");
		std::string Endl = "\n";
		const DebugBuffer::IntegerMode Hex = DebugBuffer::IntegerMode::INT_HEX;
		const DebugBuffer::IntegerMode Dec = DebugBuffer::IntegerMode::INT_DEC;
	}
}
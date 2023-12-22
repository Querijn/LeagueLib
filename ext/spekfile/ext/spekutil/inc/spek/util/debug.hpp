#pragma once

#include <spek/util/types.hpp>

#include <thread>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>

#define DEBUG_COND(a) if((a)) Debug::Output

namespace Spek
{
	namespace Debug
	{
		void Break();

		extern std::mutex g_outputMutex;

		class DebugBuffer
		{
		public:
			enum IntegerMode
			{
				INT_DEC,
				INT_HEX
			};

			DebugBuffer(const char* file);
			~DebugBuffer();

			DebugBuffer& operator<<(const std::string& file);
			DebugBuffer& operator<<(const char* file);

			template<typename T, typename = std::enable_if_t<std::is_pointer<T>::value == false>>
			DebugBuffer& operator<<(T convertable)
			{
				*this << std::to_string(convertable);
				return *this;
			}

			DebugBuffer& SetDec();

			DebugBuffer& SetHex();

			void SetIntegerMode(IntegerMode a_Mode);

			template<> DebugBuffer& operator<<(std::string& a_String) { *this << std::string(a_String); return *this; }

			template<> DebugBuffer& operator<<(IntegerMode mode) { SetIntegerMode(mode); return *this; }
			template<> DebugBuffer& operator<<(f32 integer) { return PrintFloat(integer); }
			template<> DebugBuffer& operator<<(f64 integer) { return PrintFloat(integer); }
			template<> DebugBuffer& operator<<(u64 integer) { return PrintInt(integer); }
			template<> DebugBuffer& operator<<(i64 integer) { return PrintInt(integer); }
			template<> DebugBuffer& operator<<(u32 integer) { return PrintInt(integer); }
			template<> DebugBuffer& operator<<(i32 integer) { return PrintInt(integer); }
			template<> DebugBuffer& operator<<(u16 integer) { return PrintInt(integer); }
			template<> DebugBuffer& operator<<(i16 integer) { return PrintInt(integer); }
			template<> DebugBuffer& operator<<(u8 integer) { return PrintInt(integer); }
			template<> DebugBuffer& operator<<(i8 integer) { return PrintInt(integer); }

			template<typename T>
			DebugBuffer& PrintInt(T integer)
			{
				std::stringstream stream;
				{
					std::lock_guard<std::mutex> t_Lock(g_outputMutex);
					if (m_integerMode == IntegerMode::INT_HEX)
					{
						stream << "0x" << (std::hex) << integer;
					}
					else
					{
						stream << (std::dec) << integer;
					}
				}
				*this << stream.str();
				return *this;
			}

			template<typename T>
			DebugBuffer& PrintFloat(T integer)
			{
				std::stringstream stream;
				{
					std::lock_guard<std::mutex> t_Lock(g_outputMutex);
					stream << integer;
				}
				*this << stream.str();
				return *this;
			}

		private:
			IntegerMode m_integerMode = INT_DEC;

			bool m_shouldPrintTime = true;
			std::ofstream m_file;
		};

		extern const DebugBuffer::IntegerMode Hex;
		extern const DebugBuffer::IntegerMode Dec;
		extern DebugBuffer Output;
		extern DebugBuffer Error;
		extern std::string Endl;
	}
}

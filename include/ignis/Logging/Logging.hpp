#ifndef IGNIS_LOGGING_HPP
#define IGNIS_LOGGING_HPP

#include <string_view>

namespace Ignis
{
	enum class LogLevel
	{
		Error,
		Warning,
		Info,
		Debug,
	};

	void Log(std::string_view message, LogLevel level);
}

#endif // IGNIS_LOGGING_HPP


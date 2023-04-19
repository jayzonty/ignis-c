#include "Ignis/Logging/Logging.hpp"

#include <iostream>

namespace Ignis
{
	void Log(std::string_view message, LogLevel level)
	{
		std::cout << message << std::endl;
	}
}

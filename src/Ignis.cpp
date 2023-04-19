#include "Ignis/Ignis.hpp"

#include <Ignis/Logging/Logging.hpp>

namespace Ignis
{
	bool Init()
	{
		Log("[Ignis] Init...", LogLevel::Info);
		return true;
	}

	void Cleanup()
	{
		Log("[Ignis] Cleanup...", LogLevel::Info);
	}
}

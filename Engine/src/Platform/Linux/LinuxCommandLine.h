#pragma once

#include "Core/IO.h"

namespace Engine {
	class LinuxCommandLine : public CommandLine
	{
	public:
		void ExecuteCommand(std::string& command, CommandArgs& arguments) override;
	};
}

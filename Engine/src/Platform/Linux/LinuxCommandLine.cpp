#include "pch.h"
#include "LinuxCommandLine.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace Engine {

	void LinuxCommandLine::ExecuteCommand(std::string& command, CommandArgs& arguments)
	{
		HVE_CORE_TRACE_TAG("CommandLine", "Attempting to run command: {}", command);

		pid_t pid = fork();
		if (pid < 0)
		{
			HVE_CORE_ERROR_TAG("CommandLine", "fork() failed with errno: {}", strerror(errno));
			return;
		}

		if (pid == 0)
		{
			// Child process
			if (arguments.UseAnotherWorkingDir && !arguments.WorkingDir.empty())
			{
				if (chdir(arguments.WorkingDir.c_str()) != 0)
				{
					_exit(127);
				}
			}

			if (arguments.NewProcess)
			{
				setsid();
			}

			execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
			_exit(127); // exec failed
		}

		// Parent process
		if (arguments.SleepUntilFinished)
		{
			int status;
			waitpid(pid, &status, 0);

			if (WIFEXITED(status))
			{
				int exitCode = WEXITSTATUS(status);
				if (exitCode != 0)
				{
					HVE_CORE_ERROR_TAG("CommandLine", "Command failed with exit code {}", exitCode);
					return;
				}
				HVE_CORE_TRACE_TAG("CommandLine", "Command executed successfully with exit code {}", exitCode);
			}
			else
			{
				HVE_CORE_ERROR_TAG("CommandLine", "Command terminated abnormally");
			}
		}
	}

	Ref<CommandLine> CommandLine::Create()
	{
		return CreateRef<LinuxCommandLine>();
	}
}

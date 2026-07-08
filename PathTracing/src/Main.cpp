
#include "Application.h"

namespace {
	std::filesystem::path GetExecutableDirectory(char* executablePathArg) {
	#if defined(__linux__)
		std::error_code errorCode;
		const std::filesystem::path executablePath = std::filesystem::read_symlink("/proc/self/exe", errorCode);
		if (!errorCode && !executablePath.empty())
			return executablePath.parent_path();
	#endif

		if (executablePathArg != nullptr)
			return std::filesystem::absolute(executablePathArg).parent_path();

		return std::filesystem::current_path();
	}
}

int main(int, char** argv) {
	std::filesystem::current_path(GetExecutableDirectory(argv[0]));

	PathTracer::Application application;

	bool success = application.Initialize(1600, 900);

	if (success)
		application.RunLoop();

	application.Shutdown();

	return 0;
}

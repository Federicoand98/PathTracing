#include "Application.h"

int main(int, char**) {
	Application application;

	bool success = application.Initialize(1600, 900);

	if (success)
		application.RunLoop();

	application.Shutdown();

	return 0;
}
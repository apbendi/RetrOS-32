/* abort(): musl's raises SIGABRT, and RetrOS has no signals, so it says so and ends the program */
#include <stdlib.h>
#include <unistd.h>

_Noreturn void abort(void)
{
	static const char message[] = "Aborted\n";
	write(STDERR_FILENO, message, sizeof(message) - 1);
	_Exit(127);
}

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "dep/nob/nob.h"

int main(int argc, char **argv) {
	NOB_GO_REBUILD_URSELF(argc, argv);

	Cmd cmd = {};
	//cmd_append(&cmd, "g++", "example/main.cpp", "-o", "example/main");
	cmd_append(&cmd, "cl", "example/main.cpp", "/Zi", "/std:c++20", "/EHsc", "/link", "/out:example/main.exe");
	if (!cmd_run_sync_and_reset(&cmd))
		return 1;

	return 0;
}
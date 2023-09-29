#pragma once
#include "Platform.hpp"

bool ProcessExecute(cstring workingDirectory, cstring processFullpath, cstring arguments, cstring searchErrorString = "");
cstring ProcessGetOutput();

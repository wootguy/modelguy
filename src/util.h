#pragma once
#include "types.h"
#include <string>

#define __WINDOWS__

bool fileExists(const string& fileName);

char * loadFile( const string& fileName, int& length);


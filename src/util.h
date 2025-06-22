#pragma once
#include "types.h"
#include <string>

#define __WINDOWS__

bool fileExists(const string& fileName);

char * loadFile( const string& fileName, int& length);

string sanitize_string(string input);

std::string toLowerCase(std::string str);

std::string replaceString(std::string subject, std::string search, std::string replace);
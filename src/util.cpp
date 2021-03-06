#include "util.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

bool fileExists(const string& fileName)
{
	if (FILE *file = fopen(fileName.c_str(), "r"))
	{
		fclose(file);
		return true;
	}
	return false; 
}

char * loadFile( const string& fileName, int& length)
{
	if (!fileExists(fileName))
		return NULL;
	ifstream fin(fileName.c_str(), ifstream::in|ios::binary);
	long long begin = fin.tellg();
	fin.seekg (0, ios::end);
	uint size = (uint)((int)fin.tellg() - begin);
	char * buffer = new char[size];
	fin.seekg(0);
	fin.read(buffer, size);
	fin.close();
	length = (int)size; // surely models will never exceed 2 GB
	return buffer;
}

bool invalidChar(char c) {
	return !(c >= 32 && c < 127);
}

string sanitize_string(string input) {
	input.erase(remove_if(input.begin(), input.end(), invalidChar), input.end());
	return input;
}

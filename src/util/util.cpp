#include "util.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

using namespace std::chrono;

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

std::string toLowerCase(std::string str) {
	std::string out = str;

	for (int i = 0; str[i]; i++) {
		out[i] = tolower(str[i]);
	}

	return out;
}

std::string replaceString(std::string subject, std::string search, std::string replace) {
	size_t pos = 0;
	while ((pos = subject.find(search, pos)) != std::string::npos)
	{
		subject.replace(pos, search.length(), replace);
		pos += replace.length();
	}
	return subject;
}

float clamp(float val, float min, float max) {
	if (val > max) {
		return max;
	}
	else if (val < min) {
		return min;
	}
	return val;
}

// https://stackoverflow.com/questions/1628386/normalise-orientation-between-0-and-360
float normalizeRangef(const float value, const float start, const float end)
{
	const float width = end - start;
	const float offsetValue = value - start;   // value relative to 0

	return (offsetValue - (floorf(offsetValue / width) * width)) + start;
	// + start to reset back to start of original range
}

void expandBoundingBox(vec3 v, vec3& mins, vec3& maxs) {
	if (v.x > maxs.x) maxs.x = v.x;
	if (v.y > maxs.y) maxs.y = v.y;
	if (v.z > maxs.z) maxs.z = v.z;

	if (v.x < mins.x) mins.x = v.x;
	if (v.y < mins.y) mins.y = v.y;
	if (v.z < mins.z) mins.z = v.z;
}

uint64_t getEpochMillis() {
	return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

double TimeDifference(uint64_t start, uint64_t end) {
	if (end > start) {
		return (end - start) / 1000.0;
	}
	else {
		return -((start - end) / 1000.0);
	}
}

char* strcpy_safe(char* dest, const char* src, size_t size) {
	if (size > 0) {
		size_t i;
		for (i = 0; i < size - 1 && src[i]; i++) {
			dest[i] = src[i];
		}
		dest[i] = '\0';
	}
	return dest;
}

uint64_t getFileModifiedTime(const std::string& path) {
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA fileInfo;
	if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
		return -1;
	}

	ULARGE_INTEGER ull;
	ull.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
	ull.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;

	// Convert FILETIME (100-ns intervals since 1601) to seconds since UNIX epoch (1970)
	const uint64_t EPOCH_DIFF = 116444736000000000ULL; // 100ns ticks between 1601 and 1970
	const uint64_t HUNDRED_NANOSECONDS = 10000000ULL;

	return (ull.QuadPart - EPOCH_DIFF) / HUNDRED_NANOSECONDS;

#else
	struct stat result;
	if (stat(path.c_str(), &result) != 0) {
		return -1;
	}

	return result.st_mtime;
#endif
}

string getFileName(const string& path) {
	string ret = path;

	size_t start = path.find_last_of("/\\");
	if (start != string::npos)
		ret = ret.substr(start+1);

	size_t end = ret.find_last_of(".");
	if (end != string::npos) {
		ret = ret.substr(0, end);
	}

	return ret;
}
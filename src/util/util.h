#pragma once
#include "types.h"
#include <string>

#define PI 3.141592f
#define EPSILON	(0.03125f) // 1/32 (to keep floating point happy -Carmack)

bool fileExists(const string& fileName);

char * loadFile( const string& fileName, int& length);

string sanitize_string(string input);

std::string toLowerCase(std::string str);

std::string replaceString(std::string subject, std::string search, std::string replace);

float clamp(float val, float min, float max);

float normalizeRangef(const float value, const float start, const float end);

void expandBoundingBox(vec3 v, vec3& mins, vec3& maxs);

uint64_t getEpochMillis();

double TimeDifference(uint64_t start, uint64_t end);

char* strcpy_safe(char* dest, const char* src, size_t size);

uint64_t getFileModifiedTime(const std::string& path);

string getFileName(const string& path);

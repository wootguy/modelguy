#include "mstream.h"
#include "stdio.h"
#include "string.h"

mstream::mstream()
{
	start = end = pos = 0;
	eomFlag = true;
}

mstream::mstream(char * buf, size_t len)
{
	start = (size_t)buf;
	end = start + len;
	pos = start;
	eomFlag = false;
}

size_t mstream::read( void * dest, size_t bytes )
{
	if (eomFlag)
		return 0;
	size_t newpos = pos + bytes;
	if (newpos >= end || newpos < start)
	{
		eomFlag = true;
		bytes = end - pos;
	}
	memcpy(dest, (void*)pos, bytes);
	pos = newpos;

	return bytes;
}

size_t mstream::write( void * src, size_t bytes )
{
	if (eomFlag)
		return 0;
	size_t newpos = pos + bytes;
	if (newpos >= end || newpos < start)
	{
		eomFlag = true;
		bytes = end - pos;
	}
	memcpy((void*)pos, src, bytes);
	pos = newpos;

	return bytes;
}

void mstream::resize(size_t newSize) {
	size_t offset = pos - start;
	size_t oldSize = end - start;

	char* newData = new char[newSize];
	memcpy(newData, (void*)start, oldSize);
	delete[](char*)start;

	start = (size_t)newData;
	end = start + newSize;
	pos = start + offset;
	eomFlag = offset >= newSize;
}

void mstream::insert(void* src, size_t bytes) {
	size_t offset = pos - start;
	size_t oldSize = end - start;
	size_t newSize = oldSize + bytes;

	char* newData = new char[newSize];
	memcpy(newData, (void*)start, offset);
	memcpy(newData+offset, src, bytes);
	memcpy(newData+offset+bytes, (void*)pos, oldSize - offset);
	delete[](char*)start;

	start = (size_t)newData;
	end = start + newSize;
	pos = start + offset + bytes;
	eomFlag = offset >= newSize;
}

void mstream::seek(size_t to )
{
	pos = start + to;
	eomFlag = false;
	if (pos >= end || pos < start)
		eomFlag = true;
}

void mstream::seek(size_t to, int whence )
{
	switch(whence)
	{
	case (SEEK_SET):
		pos = start + to;
		break;
	case (SEEK_CUR):
		pos += to;
		break;
	case (SEEK_END):
		pos = end + to;
		break;
	}
	eomFlag = false;
	if (pos >= end || pos < start)
		eomFlag = true;
}

size_t mstream::skip(size_t bytes )
{
	if (eomFlag)
		return 0;
	size_t newpos = pos + bytes;
	if (newpos >= end || newpos < start)
	{
		bytes = end - pos;
		eomFlag = true;
	}
	pos = newpos;
	return bytes;
}

size_t mstream::tell()
{
	return pos - start;
}

char* mstream::getBuffer() {
	return (char*)start;
}

char * mstream::get()
{
	return (char*)pos;
}

bool mstream::eom()
{
	return eomFlag;
}

void mstream::freeBuf()
{
	delete [] (char*)start;
}

mstream::~mstream( void )
{

}

size_t mstream::size()
{
	return end - start;
}

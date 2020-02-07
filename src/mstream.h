#pragma once
#include "types.h"

class mstream
{
public:
	mstream();

	// stream an existing buffer
	mstream(char * buf, size_t len);

	~mstream(void);

	// copy data from buffer into the destination
	// returns the number of bytes read
	size_t read(void * dest, size_t bytes);

	// returns number of bytes that could be written
	// into the buffer
	size_t write(void * src, size_t bytes);

	// inserts src data at current position, moving existing data after it
	// deletes data and creates a new buffer to hold the new size
	void insert(void* src, size_t bytes);

	// returns the offset in the buffer
	size_t tell();

	// returns the size of the buffer
	size_t size();

	// returns pointer to the current offset
	char * get();

	// returns pointer to the start of the data
	char* getBuffer();

	// changes the read position in the buffer
	// resets eom flag if set to a valid position
	void seek(size_t to);

	// like seek but implements SEEK_CUR/SET/END functionality
	void seek(size_t to, int whence);

	// returns number of bytes skipped
	size_t skip(size_t bytes);

	// returns true if pointer has seeked past end or beginning of the buffer
	bool eom();

	// deletes associated memory buffer
	void freeBuf();

private:
	size_t start, end, pos;
	bool eomFlag; // end of memory buffer reached

	void resize(size_t newSize);
};


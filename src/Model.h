#include "types.h"
#include "studio.h"
#include <string>
#include "mstream.h"

class Model {
public:
	studiohdr_t* header;
	mstream data;

	Model(string fpath);
	~Model();

	bool validate();

	// has a modelT.mdl?
	bool hasExternalTextures();

	// has at least a model01.mdl?
	bool hasExternalSequences(); 

	// model has no triangles?
	bool isEmpty();

	bool mergeExternalTextures(bool deleteSource);

	bool mergeExternalSequences(bool deleteSource);

	void write(string fpath);

private:
	string fpath;

	void insertData(void * src, size_t bytes);

	void removeData(size_t bytes);

	// updates all indexes with values greater than 'afterIdx', adding 'delta' to it.
	void updateIndexes(int afterIdx, int delta);

	void alignIndexes();

	// algins data pointed to by index and updates the index accordingly
	bool alignIndex(int& index);
};
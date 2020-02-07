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

	bool mergeExternalTextures();

	char* getTextures();

	void write(string fpath);

	void getTextureDataSize();

private:
	string fpath;

	void insertData(void * src, size_t bytes);
};
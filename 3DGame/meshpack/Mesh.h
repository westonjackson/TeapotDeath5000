#pragma once
#include "float2.h"
#include "float3.h"
#include <vector>

class   Mesh
{
	struct  Face
	{
		int       positionIndices[5];
		int       normalIndices[5];
		int       texcoordIndices[5];
		bool      isQuad;
        bool      isPentagon;
	};

	std::vector<std::string*>	rows;
	std::vector<float3*>		positions;
	std::vector<std::vector<Face*> >          submeshFaces;
	std::vector<float3*>		normals;
	std::vector<float2*>		texcoords;

	int            modelid;

public:
    Mesh(const char *filename);
	~Mesh();

	void        draw();
	void        drawSubmesh(unsigned int iSubmesh);
};


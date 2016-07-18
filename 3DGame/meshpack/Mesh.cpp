
// OBJ Parsing by Soeren Walls, 2015

#include <fstream>
#include <algorithm>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
// Needed on MsWindows
#include <windows.h>
#endif // Win32 platform

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
// Download glut from: http://www.opengl.org/resources/libraries/glut/
#include <GLUT/glut.h>

#include "mesh.h"
#include <cstdio>
#include <sstream>

using namespace std;

Mesh::Mesh(const char *filename)
{
	fstream file(filename); 
	if(!file.is_open())       
    {
        // char * dir = getcwd(NULL, 0); // Platform-dependent, see reference link below
        // printf("Current dir: %s\n", dir);
        printf("file %s not found\n", filename);
		return;
	}

	char buffer[256];
	while(!file.eof())
	{
		file.getline(buffer,256);
		rows.push_back(new string(buffer));
	}

	submeshFaces.push_back(std::vector<Face*>());
	std::vector<Face*>* faces = &submeshFaces.at(submeshFaces.size()-1);

	for(int i = 0; i < rows.size(); i++)
	{
		if(rows[i]->empty() || (*rows[i])[0] == '#' || (*rows[i])[0] == 's' || (*rows[i])[0] == 'u') // usemtl
			continue;      
		else if((*rows[i])[0] == 'v' && (*rows[i])[1] == ' ')
		{
			float tmpx,tmpy,tmpz;
			sscanf(rows[i]->c_str(), "v %f %f %f" ,&tmpx,&tmpy,&tmpz);      
			positions.push_back(new float3(tmpx,tmpy,tmpz));  
		}
		else if((*rows[i])[0] == 'v' && (*rows[i])[1] == 'n')
		{
			float tmpx,tmpy,tmpz;
			sscanf(rows[i]->c_str(), "vn %f %f %f" ,&tmpx,&tmpy,&tmpz);
			normals.push_back(new float3(tmpx,tmpy,tmpz));
		}
		else if((*rows[i])[0] == 'v' && (*rows[i])[1] == 't')
		{
			float tmpx,tmpy;
			sscanf(rows[i]->c_str(), "vt %f %f" ,&tmpx,&tmpy);
			texcoords.push_back(new float2(tmpx,tmpy));
		}
		else if((*rows[i])[0] == 'f')  
		{
            Face* f = new Face();
            int numVert = -1; // instantiate to -1 in order to skip the "f" at the beginning
            string format1 = "%d/%d/%d"; // given position, texture, and normal
            string format2 = "%d//%d"; // given only position and normal
            string format3 = "%d/%d"; // given only position and texture
            istringstream row(*rows[i]);
            string word;
            // for each description of vertex position, texture, and normal
            while(row >> word) {
                if(numVert > -1) {
                    int matched = sscanf(word.c_str(), format1.c_str(),
                           &f->positionIndices[numVert],
                           &f->texcoordIndices[numVert],
                           &f->normalIndices[numVert]);
                    if(matched < 3) {
                        matched = sscanf(word.c_str(), format2.c_str(),
                               &f->positionIndices[numVert],
                               &f->normalIndices[numVert]);
                        f->texcoordIndices[numVert] = 0;
                        if(matched == 2)
                            printf("Texture cannot be applied to this .obj without texCoords.\n");
                    }
                    if(matched < 2) {
                        matched = sscanf(word.c_str(), format3.c_str(),
                                         &f->positionIndices[numVert],
                                         &f->texcoordIndices[numVert]);
                        f->normalIndices[numVert] = 0;
                    }
                    if(matched < 2) {
                        printf("Error parsing OBJ file: %s\n", filename);
                        return;
                    }
                }
                numVert++;
                if(numVert > 5) {
                    break; // don't allow polygons with > 5 vertices
                }
            }
            f->isQuad = numVert == 4;
            f->isPentagon = numVert == 5;
            faces->push_back(f);
		}
		else if((*rows[i])[0] == 'g')
		{
			if(faces->size() > 0)
			{
				submeshFaces.push_back(std::vector<Face*>());
				faces = &submeshFaces.at(submeshFaces.size()-1);
			}
        }
                  
	}

	modelid = glGenLists(submeshFaces.size());

	for(int iSubmesh=0; iSubmesh<submeshFaces.size(); iSubmesh++)
	{
		std::vector<Face*>& faces = submeshFaces.at(iSubmesh);

        glNewList(modelid + iSubmesh,GL_COMPILE);
        
        float3 *normal, *pos;
        float2 *texCoord;

		glBegin(GL_TRIANGLES);
		for(int i=0;i<faces.size();i++)
        {
            if(faces[i]->isPentagon)
            {
                for(int j=0; j<3; j++) {
                    if(normals.size() > 0) {
                        normal = normals[faces[i]->normalIndices[j]-1];
                        glNormal3f(normal->x,normal->y,normal->z);
                    }
                    if(texcoords.size() > 0) {
                        texCoord = texcoords[faces[i]->texcoordIndices[j]-1];
                        glTexCoord2f(texCoord->x,1-texCoord->y);
                    }
                    if(positions.size() > 0) {
                        pos = positions[faces[i]->positionIndices[j]-1];
                        glVertex3f(pos->x,pos->y,pos->z);
                    }
                }
                for(int j=1; j<4; j++) {
                    if(normals.size() > 0) {
                        normal = normals[faces[i]->normalIndices[j]-1];
                        glNormal3f(normal->x,normal->y,normal->z);
                    }
                    if(texcoords.size() > 0) {
                        texCoord = texcoords[faces[i]->texcoordIndices[j]-1];
                        glTexCoord2f(texCoord->x,1-texCoord->y);
                    }
                    if(positions.size() > 0) {
                        pos = positions[faces[i]->positionIndices[j]-1];
                        glVertex3f(pos->x,pos->y,pos->z);
                    }
                }
                for(int j=2; j<5; j++) {
                    if(normals.size() > 0) {
                        normal = normals[faces[i]->normalIndices[j]-1];
                        glNormal3f(normal->x,normal->y,normal->z);
                    }
                    if(texcoords.size() > 0) {
                        texCoord = texcoords[faces[i]->texcoordIndices[j]-1];
                        glTexCoord2f(texCoord->x,1-texCoord->y);
                    }
                    if(positions.size() > 0) {
                        pos = positions[faces[i]->positionIndices[j]-1];
                        glVertex3f(pos->x,pos->y,pos->z);
                    }
                }
            }
			else if(faces[i]->isQuad)
            {
                for(int j=0; j<4; j++) {
                    if(j==3) continue;
                    if(normals.size() > 0) {
                        normal = normals[faces[i]->normalIndices[j]-1];
                        glNormal3f(normal->x,normal->y,normal->z);
                    }
                    if(texcoords.size() > 0) {
                        texCoord = texcoords[faces[i]->texcoordIndices[j]-1];
                        glTexCoord2f(texCoord->x,1-texCoord->y);
                    }
                    if(positions.size() > 0) {
                        pos = positions[faces[i]->positionIndices[j]-1];
                        glVertex3f(pos->x,pos->y,pos->z);
                    }
                }
                for(int j=0; j<4; j++) {
                    if(j==1) continue;
                    if(normals.size() > 0) {
                        normal = normals[faces[i]->normalIndices[j]-1];
                        glNormal3f(normal->x,normal->y,normal->z);
                    }
                    if(texcoords.size() > 0) {
                        texCoord = texcoords[faces[i]->texcoordIndices[j]-1];
                        glTexCoord2f(texCoord->x,1-texCoord->y);
                    }
                    if(positions.size() > 0) {
                        pos = positions[faces[i]->positionIndices[j]-1];
                        glVertex3f(pos->x,pos->y,pos->z);
                    }
                }
			}
			else
            {
                for(int j=0; j<3; j++) {
                    if(normals.size() > 0) {
                        normal = normals[faces[i]->normalIndices[j]-1];
                        glNormal3f(normal->x,normal->y,normal->z);
                    }
                    if(texcoords.size() > 0) {
                        texCoord = texcoords[faces[i]->texcoordIndices[j]-1];
                        glTexCoord2f(texCoord->x,1-texCoord->y);
                    }
                    if(positions.size() > 0) {
                        pos = positions[faces[i]->positionIndices[j]-1];
                        glVertex3f(pos->x,pos->y,pos->z);
                    }
                }
			}
		}
		glEnd();

		glEndList();
	}
}

void Mesh::draw()
{
	for(int iSubmesh=0; iSubmesh<submeshFaces.size(); iSubmesh++)
		glCallList(modelid + iSubmesh);
}

void Mesh::drawSubmesh(unsigned int iSubmesh)
{
	glCallList(modelid + iSubmesh);
}

Mesh::~Mesh()
{
	for(unsigned int i = 0; i < rows.size(); i++)
		delete rows[i];   
	for(unsigned int i = 0; i < positions.size(); i++)
		delete positions[i];
	for(unsigned int i = 0; i < submeshFaces.size(); i++)
		for(unsigned int j = 0; j < submeshFaces.at(i).size(); j++)
			delete submeshFaces.at(i).at(j); 
	for(unsigned int i = 0; i < normals.size(); i++)
		delete normals[i];
	for(unsigned int i = 0; i < texcoords.size(); i++)
		delete texcoords[i];
}

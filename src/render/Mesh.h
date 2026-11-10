#ifndef MESH_H
#define MESH_H

#include <vector>
#include <string>

#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;

	// tangente pour le normal mapping : xyz = axe U de la texture, w = +1 ou -1 selon le sens du bitangent
	glm::vec4 tangent = glm::vec4(0.0f);
};

class Mesh
{
public:

	 Mesh();
	~Mesh();

	// copie interdite : le destructeur libère le VAO/VBO
	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;

	bool loadOBJ(const std::string& filename);

	bool loadVertices(const std::vector<Vertex>& vertices);

	// mesh indexé (format glTF) : un sommet partagé n'est stocké qu'une fois
	bool loadIndexed(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);

	static void computeTangents(std::vector<Vertex>& vertices, const std::vector<unsigned int>* indices);

	unsigned int getTriangleCount() const;

	void draw();

	// dessin instancié : 32 octets par instance (x, y, z, cap, échelle, phase, teinte) à partir de byteOffset
	void drawInstanced(GLuint instanceVbo, size_t byteOffset, int count);

private:

	void initBuffers();

	bool mLoaded;
	std::vector<Vertex> mVertices;
	GLuint mVBO, mVAO, mEBO;
	std::vector<unsigned int> mIndices;
};
#endif

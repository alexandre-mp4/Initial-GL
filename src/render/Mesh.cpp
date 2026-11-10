#include "Mesh.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include "glm/gtc/constants.hpp"

std::vector<std::string> split(std::string s, std::string t)
{
	std::vector<std::string> res;
	while(1)
	{
		int pos = s.find(t);
		if(pos == -1)
		{
			res.push_back(s);
			break;
		}
		res.push_back(s.substr(0, pos));
		s = s.substr(pos+1, s.size() - pos - 1);
	}
	return res;
}

Mesh::Mesh()
	:mLoaded(false),
	 mVBO(0),
	 mVAO(0),
	 mEBO(0)
{
}

Mesh::~Mesh()
{
	glDeleteVertexArrays(1, &mVAO);
	glDeleteBuffers(1, &mVBO);
	glDeleteBuffers(1, &mEBO);
}

bool Mesh::loadOBJ(const std::string& filename)
{
	std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
	std::vector<glm::vec3> tempVertices;
	std::vector<glm::vec2> tempUVs;
	std::vector<glm::vec3> tempNormals;

	if (filename.find(".obj") != std::string::npos)
	{
		std::ifstream fin(filename, std::ios::in);
		if (!fin)
		{
			std::cerr << "Cannot open " << filename << std::endl;
			return false;
		}

		std::cout << "Loading OBJ file " << filename << " ..." << std::endl;

		std::string lineBuffer;
		while (std::getline(fin, lineBuffer))
		{
			std::stringstream ss(lineBuffer);
			std::string cmd;
			ss >> cmd;

			if (cmd == "v")
			{
				glm::vec3 vertex;
				int dim = 0;
				while (dim < 3 && ss >> vertex[dim])
					dim++;

				tempVertices.push_back(vertex);
			}
			else if (cmd == "vt")
			{
				glm::vec2 uv;
				int dim = 0;
				while (dim < 2 && ss >> uv[dim])
					dim++;

				tempUVs.push_back(uv);
			}
			else if (cmd == "vn")
			{
				glm::vec3 normal;
				int dim = 0;
				while (dim < 3 && ss >> normal[dim])
					dim++;
				normal = glm::normalize(normal);
				tempNormals.push_back(normal);
			}
			else if (cmd == "f")
			{
				std::string faceData;
				int vertexIndex, uvIndex, normalIndex;

				while (ss>>faceData)
				{
					std::vector<std::string> data = split(faceData, "/");

					if (data[0].size() > 0)
					{
						sscanf(data[0].c_str(), "%d", &vertexIndex);
						vertexIndices.push_back(vertexIndex);
					}

					if (data.size() > 1)
					{
						if (data[1].size() > 0)
						{
							sscanf(data[1].c_str(), "%d", &uvIndex);
							uvIndices.push_back(uvIndex);
						}
					}

					if (data.size() > 2)
					{
						if (data[2].size() > 0)
						{
							sscanf(data[2].c_str(), "%d", &normalIndex);
							normalIndices.push_back(normalIndex);
						}
					}
				}
			}
		}

		fin.close();

		for (unsigned int i = 0; i < vertexIndices.size(); i++)
		{
			Vertex meshVertex;

			if (tempVertices.size() > 0)
			{
				glm::vec3 vertex = tempVertices[vertexIndices[i] - 1];
				meshVertex.position = vertex;
			}

			if (tempNormals.size() > 0)
			{
				glm::vec3 normal = tempNormals[normalIndices[i] - 1];
				meshVertex.normal = normal;
			}

			if (tempUVs.size() > 0)
			{
				glm::vec2 uv = tempUVs[uvIndices[i] - 1];
				meshVertex.texCoords = uv;
			}

			mVertices.push_back(meshVertex);
		}

		// tangentes nécessaires seulement pour le normal mapping
		computeTangents(mVertices, nullptr);

		initBuffers();

		return (mLoaded = true);
	}

	return false;
}

bool Mesh::loadVertices(const std::vector<Vertex>& vertices)
{
	if (vertices.empty())
		return false;

	mVertices = vertices;
	computeTangents(mVertices, nullptr);
	initBuffers();

	return (mLoaded = true);
}

bool Mesh::loadIndexed(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
	if (vertices.empty() || indices.empty())
		return false;

	mVertices = vertices;
	mIndices = indices;
	if (mVertices[0].tangent.w == 0.0f)
		computeTangents(mVertices, &mIndices);
	initBuffers();

	return (mLoaded = true);
}

unsigned int Mesh::getTriangleCount() const
{
	return (unsigned int)(mIndices.empty() ? mVertices.size() : mIndices.size()) / 3;
}

// tangentes : pour un triangle d'arêtes E1, E2 et de variations d'UV dUV1, dUV2,
// T = (E1*dV2 - E2*dV1) / det et B = (E2*dU1 - E1*dU2) / det, avec det = dU1*dV2 - dU2*dV1
// les T et B des triangles voisins sont additionnés puis T est rendu perpendiculaire à N (Gram-Schmidt)
// le bitangent est recalculé dans le shader par cross(N, T) * w
void Mesh::computeTangents(std::vector<Vertex>& vertices, const std::vector<unsigned int>* indices)
{
	std::vector<glm::vec3> tan(vertices.size(), glm::vec3(0.0f));
	std::vector<glm::vec3> bitan(vertices.size(), glm::vec3(0.0f));

	size_t numIndices = indices ? indices->size() : vertices.size();
	for (size_t i = 0; i + 2 < numIndices; i += 3)
	{
		unsigned int i0 = indices ? (*indices)[i]     : (unsigned int)i;
		unsigned int i1 = indices ? (*indices)[i + 1] : (unsigned int)i + 1;
		unsigned int i2 = indices ? (*indices)[i + 2] : (unsigned int)i + 2;

		glm::vec3 e1 = vertices[i1].position - vertices[i0].position;
		glm::vec3 e2 = vertices[i2].position - vertices[i0].position;
		glm::vec2 d1 = vertices[i1].texCoords - vertices[i0].texCoords;
		glm::vec2 d2 = vertices[i2].texCoords - vertices[i0].texCoords;

		float det = d1.x * d2.y - d2.x * d1.y;
		if (fabsf(det) < 1e-12f)
			continue;
		float r = 1.0f / det;

		glm::vec3 t = (e1 * d2.y - e2 * d1.y) * r;
		glm::vec3 b = (e2 * d1.x - e1 * d2.x) * r;

		tan[i0] += t; tan[i1] += t; tan[i2] += t;
		bitan[i0] += b; bitan[i1] += b; bitan[i2] += b;
	}

	for (size_t i = 0; i < vertices.size(); i++)
	{
		glm::vec3 n = vertices[i].normal;
		glm::vec3 t = tan[i];

		t = t - n * glm::dot(n, t);
		if (glm::dot(t, t) < 1e-12f)
		{
			glm::vec3 helper = fabsf(n.y) < 0.99f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
			t = glm::cross(helper, n);
		}
		t = glm::normalize(t);

		float w = (glm::dot(glm::cross(n, t), bitan[i]) < 0.0f) ? -1.0f : 1.0f;
		vertices[i].tangent = glm::vec4(t, w);
	}
}

void Mesh::initBuffers()
{
	glGenVertexArrays(1, &mVAO);
	glGenBuffers(1, &mVBO);

	glBindVertexArray(mVAO);
	glBindBuffer(GL_ARRAY_BUFFER, mVBO);
	glBufferData(GL_ARRAY_BUFFER, mVertices.size() * sizeof(Vertex), &mVertices[0], GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(6 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(8 * sizeof(GLfloat)));
	glEnableVertexAttribArray(3);

	if (!mIndices.empty())
	{
		glGenBuffers(1, &mEBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndices.size() * sizeof(unsigned int), mIndices.data(), GL_STATIC_DRAW);
	}

	glBindVertexArray(0);
}

// rendu instancié : les attributs 4 et 5 avancent d'un élément par instance (diviseur 1)
void Mesh::drawInstanced(GLuint instanceVbo, size_t byteOffset, int count)
{
	if (!mLoaded || count <= 0) return;

	glBindVertexArray(mVAO);
	glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 32, (GLvoid*)byteOffset);
	glEnableVertexAttribArray(4);
	glVertexAttribDivisor(4, 1);
	glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 32, (GLvoid*)(byteOffset + 16));
	glEnableVertexAttribArray(5);
	glVertexAttribDivisor(5, 1);

	if (mIndices.empty())
		glDrawArraysInstanced(GL_TRIANGLES, 0, (GLsizei)mVertices.size(), count);
	else
		glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)mIndices.size(), GL_UNSIGNED_INT, 0, count);
	glBindVertexArray(0);
}

void Mesh::draw()
{
	if (!mLoaded) return;

	glBindVertexArray(mVAO);
	if (mIndices.empty())
		glDrawArrays(GL_TRIANGLES, 0, mVertices.size());
	else
		glDrawElements(GL_TRIANGLES, (GLsizei)mIndices.size(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

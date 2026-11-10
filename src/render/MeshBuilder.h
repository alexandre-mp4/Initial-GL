// assemble de la géométrie simple (quads, boîtes) puis crée un Mesh
#ifndef MESHBUILDER_H
#define MESHBUILDER_H

#include <memory>
#include <vector>
#include "glm/glm.hpp"
#include "Mesh.h"

class MeshBuilder
{
public:

	// quad p0 p1 p2 p3 dans le sens trigonométrique vu du côté de la normale
	void quad(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
			  const glm::vec3& n, glm::vec2 uv0 = glm::vec2(0, 0), glm::vec2 uv1 = glm::vec2(1, 0),
			  glm::vec2 uv2 = glm::vec2(1, 1), glm::vec2 uv3 = glm::vec2(0, 1))
	{
		unsigned int base = (unsigned int)mVertices.size();
		const glm::vec3 p[4] = { p0, p1, p2, p3 };
		const glm::vec2 uv[4] = { uv0, uv1, uv2, uv3 };
		for (int i = 0; i < 4; i++)
		{
			Vertex v;
			v.position = p[i];
			v.normal = n;
			v.texCoords = uv[i];
			mVertices.push_back(v);
		}
		mIndices.insert(mIndices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
	}

	void box(const glm::vec3& c, const glm::vec3& ax, const glm::vec3& ay, const glm::vec3& az,
			 float hx, float hy, float hz)
	{
		glm::vec3 X = ax * hx, Y = ay * hy, Z = az * hz;

		face(c + X, ax, Y, Z); face(c - X, -ax, Z, Y);
		face(c + Y, ay, Z, X); face(c - Y, -ay, X, Z);
		face(c + Z, az, X, Y); face(c - Z, -az, Y, X);
	}

	std::unique_ptr<Mesh> build()
	{
		if (mVertices.empty())
			return nullptr;
		std::unique_ptr<Mesh> m(new Mesh());
		m->loadIndexed(mVertices, mIndices);
		return m;
	}

	std::vector<Vertex>& vertices() { return mVertices; }
	std::vector<unsigned int>& indices() { return mIndices; }

private:

	void face(const glm::vec3& fc, const glm::vec3& n, const glm::vec3& a, const glm::vec3& b)
	{
		quad(fc - a - b, fc + a - b, fc + a + b, fc - a + b, n);
	}

	std::vector<Vertex> mVertices;
	std::vector<unsigned int> mIndices;
};
#endif

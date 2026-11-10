// relief de la montagne en chunks de 64 x 64 cases avec 4 niveaux de détail (heightmap 16 bits générée par tools/gen_world.py)
#ifndef TERRAIN_H
#define TERRAIN_H

#include <string>
#include <vector>
#include "glm/glm.hpp"
#define GLEW_STATIC
#include "GL/glew.h"
#include "Frustum.h"

class Terrain
{
public:

	Terrain();
	~Terrain();
	Terrain(const Terrain&) = delete;
	Terrain& operator=(const Terrain&) = delete;

	bool load(const std::string& dir);

	// hauteur selon les triangles du maillage affiché
	float getHeight(float x, float z) const;

	glm::vec3 getNormal(float x, float z) const;

	void draw(const Frustum& frustum, const glm::vec3& eye, float lodScale);
	void drawShadow(const glm::vec3& focus, float radius);

	float getSize() const     { return (mN - 1) * mCell; }
	float getHalfSize() const { return 0.5f * getSize(); }
	unsigned int getTriangleCountLastDraw() const { return mLastTriangles; }

private:

	struct Chunk
	{
		GLuint vbo, ebo[4], vao[4];
		int indexCount[4];
		glm::vec3 boundsMin, boundsMax, center;
	};

	int cellIndex(int i, int j) const { return j * mN + i; }
	void buildChunk(Chunk& chunk, int cx, int cz);

	int mN, mChunksPerSide;
	float mCell, mHeightRange;
	std::vector<float> mHeights;
	std::vector<Chunk> mChunks;
	unsigned int mLastTriangles;
};
#endif

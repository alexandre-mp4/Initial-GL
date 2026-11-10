#include "Terrain.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include "stb_image/stb_image.h"

static const int CHUNK_CELLS = 64;
static const int CHUNK_VERTS = CHUNK_CELLS + 1;
static const int SKIRT_BASE = CHUNK_VERTS * CHUNK_VERTS;
static const float SKIRT_DEPTH = 7.0f;

Terrain::Terrain() : mN(0), mChunksPerSide(0), mCell(1.5f), mHeightRange(1200.0f), mLastTriangles(0)
{
}

Terrain::~Terrain()
{
	for (Chunk& c : mChunks)
	{
		glDeleteBuffers(1, &c.vbo);
		glDeleteBuffers(4, c.ebo);
		glDeleteVertexArrays(4, c.vao);
	}
}

bool Terrain::load(const std::string& dir)
{
	std::ifstream cfg(dir + "/world.cfg");
	if (!cfg)
	{
		std::cerr << "Terrain : " << dir << "/world.cfg introuvable (lancer tools/gen_world.py)" << std::endl;
		return false;
	}
	std::map<std::string, float> values;
	std::string line;
	while (std::getline(cfg, line))
	{
		size_t eq = line.find('=');
		if (eq != std::string::npos)
			values[line.substr(0, eq)] = (float)atof(line.substr(eq + 1).c_str());
	}
	mN = (int)values["grid"];
	mCell = values["cell"];
	mHeightRange = values["height_range"];
	mChunksPerSide = (mN - 1) / CHUNK_CELLS;

	// heightmap : entiers 16 bits convertis en mètres
	int w, h, comp;
	unsigned short* pixels = stbi_load_16((dir + "/heightmap.png").c_str(), &w, &h, &comp, 1);
	if (!pixels || w != mN || h != mN)
	{
		std::cerr << "Terrain : heightmap.png invalide" << std::endl;
		return false;
	}
	mHeights.resize((size_t)mN * mN);
	for (size_t i = 0; i < mHeights.size(); i++)
		mHeights[i] = pixels[i] / 65535.0f * mHeightRange;
	stbi_image_free(pixels);

	mChunks.resize((size_t)mChunksPerSide * mChunksPerSide);
	for (int cz = 0; cz < mChunksPerSide; cz++)
		for (int cx = 0; cx < mChunksPerSide; cx++)
			buildChunk(mChunks[cz * mChunksPerSide + cx], cx, cz);

	std::cout << "Terrain : " << mN << "x" << mN << " points, " << getSize() << " m de côté, "
			  << mChunks.size() << " chunks" << std::endl;
	return true;
}

// sommets (position et normale) et 4 tampons d'indices, un par niveau de détail
void Terrain::buildChunk(Chunk& chunk, int cx, int cz)
{
	const int i0 = cx * CHUNK_CELLS, j0 = cz * CHUNK_CELLS;
	const float half = getHalfSize();

	std::vector<float> verts;
	verts.reserve((SKIRT_BASE + 4 * CHUNK_VERTS) * 6);
	float yMin = 1e9f, yMax = -1e9f;

	for (int r = 0; r < CHUNK_VERTS; r++)
	{
		for (int c = 0; c < CHUNK_VERTS; c++)
		{
			int i = i0 + c, j = j0 + r;
			float y = mHeights[cellIndex(i, j)];

			// normale = (-dh/dx, 1, -dh/dz) normalisée, dérivées par différences centrées
			int il = std::max(i - 1, 0), ir = std::min(i + 1, mN - 1);
			int jl = std::max(j - 1, 0), jr = std::min(j + 1, mN - 1);
			float dhdx = (mHeights[cellIndex(ir, j)] - mHeights[cellIndex(il, j)]) / ((ir - il) * mCell);
			float dhdz = (mHeights[cellIndex(i, jr)] - mHeights[cellIndex(i, jl)]) / ((jr - jl) * mCell);
			glm::vec3 n = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));

			verts.insert(verts.end(), { i * mCell - half, y, j * mCell - half, n.x, n.y, n.z });
			yMin = std::min(yMin, y); yMax = std::max(yMax, y);
		}
	}

	// jupes : bord abaissé qui cache les fissures entre deux niveaux de détail
	for (int side = 0; side < 4; side++)
	{
		for (int k = 0; k < CHUNK_VERTS; k++)
		{
			int src = (side == 0) ? k : (side == 1) ? (CHUNK_VERTS - 1) * CHUNK_VERTS + k
					: (side == 2) ? k * CHUNK_VERTS : k * CHUNK_VERTS + CHUNK_VERTS - 1;
			float* v = &verts[src * 6];
			verts.insert(verts.end(), { v[0], v[1] - SKIRT_DEPTH, v[2], v[3], v[4], v[5] });
		}
	}
	chunk.boundsMin = glm::vec3(i0 * mCell - half, yMin - SKIRT_DEPTH, j0 * mCell - half);
	chunk.boundsMax = glm::vec3((i0 + CHUNK_CELLS) * mCell - half, yMax, (j0 + CHUNK_CELLS) * mCell - half);
	chunk.center = 0.5f * (chunk.boundsMin + chunk.boundsMax);

	glGenBuffers(1, &chunk.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

	glGenBuffers(4, chunk.ebo);
	glGenVertexArrays(4, chunk.vao);

	for (int lod = 0; lod < 4; lod++)
	{
		const int s = 1 << lod;
		std::vector<unsigned short> idx;

		// chaque case est coupée en 2 triangles, comme dans getHeight
		for (int r = 0; r < CHUNK_CELLS; r += s)
		{
			for (int c = 0; c < CHUNK_CELLS; c += s)
			{
				unsigned short i00 = r * CHUNK_VERTS + c, i10 = i00 + s;
				unsigned short i01 = (r + s) * CHUNK_VERTS + c, i11 = i01 + s;
				idx.insert(idx.end(), { i00, i01, i10, i10, i01, i11 });
			}
		}

		for (int side = 0; side < 4; side++)
		{
			for (int k = 0; k < CHUNK_CELLS; k += s)
			{
				int b0 = (side == 0) ? k : (side == 1) ? (CHUNK_VERTS - 1) * CHUNK_VERTS + k
						: (side == 2) ? k * CHUNK_VERTS : k * CHUNK_VERTS + CHUNK_VERTS - 1;
				int b1 = (side < 2) ? b0 + s : b0 + s * CHUNK_VERTS;
				unsigned short s0 = SKIRT_BASE + side * CHUNK_VERTS + k, s1 = s0 + s;
				idx.insert(idx.end(), { (unsigned short)b0, s0, (unsigned short)b1, (unsigned short)b1, s0, s1 });
			}
		}
		chunk.indexCount[lod] = (int)idx.size();

		glBindVertexArray(chunk.vao[lod]);
		glBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.ebo[lod]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned short), idx.data(), GL_STATIC_DRAW);
	}
	glBindVertexArray(0);
}

// hauteur : interpolation barycentrique dans le triangle du maillage qui contient (x, z)
float Terrain::getHeight(float x, float z) const
{
	float half = getHalfSize();
	float gx = (x + half) / mCell, gz = (z + half) / mCell;
	gx = std::min(std::max(gx, 0.0f), (float)(mN - 1) - 0.0001f);
	gz = std::min(std::max(gz, 0.0f), (float)(mN - 1) - 0.0001f);
	int i = (int)gx, j = (int)gz;
	float fx = gx - i, fz = gz - j;

	float h00 = mHeights[cellIndex(i, j)],     h10 = mHeights[cellIndex(i + 1, j)];
	float h01 = mHeights[cellIndex(i, j + 1)], h11 = mHeights[cellIndex(i + 1, j + 1)];

	if (fx + fz <= 1.0f)
		return h00 + fx * (h10 - h00) + fz * (h01 - h00);
	return h11 + (1.0f - fx) * (h01 - h11) + (1.0f - fz) * (h10 - h11);
}

glm::vec3 Terrain::getNormal(float x, float z) const
{
	const float e = mCell;
	float dhdx = (getHeight(x + e, z) - getHeight(x - e, z)) / (2 * e);
	float dhdz = (getHeight(x, z + e) - getHeight(x, z - e)) / (2 * e);
	return glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
}

// un appel de dessin par chunk visible, au niveau de détail de sa distance
void Terrain::draw(const Frustum& frustum, const glm::vec3& eye, float lodScale)
{
	mLastTriangles = 0;
	const float chunkRadius = 0.5f * CHUNK_CELLS * mCell * 1.4142f;

	for (const Chunk& c : mChunks)
	{
		if (!frustum.intersectsBox(c.boundsMin, c.boundsMax))
			continue;

		float dx = c.center.x - eye.x, dz = c.center.z - eye.z;
		float dist = sqrtf(dx * dx + dz * dz) - chunkRadius;
		dist /= lodScale;
		int lod = (dist < 160.0f) ? 0 : (dist < 380.0f) ? 1 : (dist < 800.0f) ? 2 : 3;

		glBindVertexArray(c.vao[lod]);
		glDrawElements(GL_TRIANGLES, c.indexCount[lod], GL_UNSIGNED_SHORT, 0);
		mLastTriangles += c.indexCount[lod] / 3;
	}
	glBindVertexArray(0);
}

void Terrain::drawShadow(const glm::vec3& focus, float radius)
{
	const float chunkRadius = 0.5f * CHUNK_CELLS * mCell * 1.4142f;
	for (const Chunk& c : mChunks)
	{
		float dx = c.center.x - focus.x, dz = c.center.z - focus.z;
		float dist = sqrtf(dx * dx + dz * dz) - chunkRadius;
		if (dist > radius)
			continue;
		int lod = (dist < 40.0f) ? 0 : 1;
		glBindVertexArray(c.vao[lod]);
		glDrawElements(GL_TRIANGLES, c.indexCount[lod], GL_UNSIGNED_SHORT, 0);
	}
	glBindVertexArray(0);
}

#include "Forest.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>

const float Forest::CELL_SIZE = 64.0f;

Forest::Forest() : mModel(nullptr), mNumVariants(0), mNumLods(1), mMaxDist(400), mLod0Dist(34), mLod1Dist(115), mHasWind(true), mGridW(0), mGridMin(0), mYMin(0), mYMax(0), mInstanceVbo(0), mVboSize(0), mLastCount(0)
{
}

Forest::~Forest()
{
	if (mInstanceVbo) glDeleteBuffers(1, &mInstanceVbo);
}

bool Forest::load(AssetManager& assets, const std::string& modelPath, const std::string& instancesPath,
				  const std::string& prefix, float maxDist, float lod0Dist, float lod1Dist)
{
	mMaxDist = maxDist; mLod0Dist = lod0Dist; mLod1Dist = lod1Dist;
	mHasWind = (prefix == "tree");
	mModel = assets.model(modelPath);
	if (!mModel)
		return false;

	// noeuds du .glb nommés <prefix><v>_lod<l>
	for (int v = 0; v < MAX_VARIANTS; v++)
	{
		int lods = 0;
		for (int l = 0; l < LODS; l++)
		{
			char name[64];
			snprintf(name, sizeof(name), "%s%d_lod%d", prefix.c_str(), v, l);
			mNode[v][l] = mModel->findNode(name);
			if (mNode[v][l] >= 0) lods = l + 1; else break;
		}
		if (lods == 0) break;
		mNumLods = lods;
		mNumVariants = v + 1;
		mHeight[v] = 0.0f;
		char hp[48];
		snprintf(hp, sizeof(hp), "%s%d_height_", prefix.c_str(), v);
		for (int i = 0; i < mModel->getNodeCount(); i++)
		{
			int dm;
			const std::string& nm = mModel->getNodeName(i);
			if (nm.compare(0, strlen(hp), hp) == 0 && sscanf(nm.c_str() + strlen(hp), "%d", &dm) == 1)
				mHeight[v] = dm / 10.0f;
		}
	}
	if (mNumVariants == 0)
	{
		std::cerr << "Forest : aucune variante trouvée dans " << modelPath << std::endl;
		return false;
	}

	// instances : x, y, z, cap, échelle (float) et variante (uint32)
	FILE* f = fopen(instancesPath.c_str(), "rb");
	if (!f)
	{
		std::cerr << "Forest : " << instancesPath << " introuvable (tools/gen_world.py)" << std::endl;
		return false;
	}
	struct Record { float x, y, z, yaw, scale; unsigned int variant; } rec;
	unsigned int seed = 12345;
	auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0f; };
	mYMin = 1e9f; mYMax = -1e9f;
	float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
	while (fread(&rec, sizeof(rec), 1, f) == 1)
	{
		Tree t = { rec.x, rec.y, rec.z, rec.yaw, rec.scale, rec.variant % mNumVariants, rnd() * 6.2832f, 0.82f + 0.36f * rnd() };
		mTrees.push_back(t);
		mYMin = std::min(mYMin, rec.y); mYMax = std::max(mYMax, rec.y + 25.0f);
		minX = std::min(minX, rec.x); maxX = std::max(maxX, rec.x);
		minZ = std::min(minZ, rec.z); maxZ = std::max(maxZ, rec.z);
	}
	fclose(f);

	// grille de cases
	mGridMin = std::min(minX, minZ) - 1.0f;
	mGridW = (int)((std::max(maxX, maxZ) - mGridMin) / CELL_SIZE) + 1;
	mCells.assign((size_t)mGridW * mGridW, std::vector<int>());
	for (size_t i = 0; i < mTrees.size(); i++)
	{
		int cx = (int)((mTrees[i].x - mGridMin) / CELL_SIZE), cz = (int)((mTrees[i].z - mGridMin) / CELL_SIZE);
		mCells[(size_t)cz * mGridW + cx].push_back((int)i);
	}

	glGenBuffers(1, &mInstanceVbo);
	std::cout << "Forêt : " << mTrees.size() << " arbres, " << mNumVariants << " variantes" << std::endl;
	return true;
}

void Forest::draw(Renderer& renderer)
{
	const bool shadow = renderer.isShadowPass();
	const float lodScale = renderer.getLodScale();

	// portée et distances de LOD ; aux ombres, seulement autour du point de focus et avec moins de détail
	const glm::vec3 center = shadow ? renderer.getShadowFocus() : renderer.getEye();
	const float maxDist = shadow ? renderer.shadowExtent * 1.7f : mMaxDist * lodScale;
	const float lod0 = shadow ? 12.0f : mLod0Dist * lodScale;
	const float lod1 = shadow ? 1e9f : mLod1Dist * lodScale;
	const Frustum& frustum = renderer.getFrustum();

	// 1. tri des arbres visibles par variante et par LOD
	static std::vector<int> bucket[MAX_VARIANTS][LODS];
	for (int v = 0; v < MAX_VARIANTS; v++)
		for (int l = 0; l < LODS; l++)
			bucket[v][l].clear();

	int cx0 = std::max(0, (int)((center.x - maxDist - mGridMin) / CELL_SIZE));
	int cx1 = std::min(mGridW - 1, (int)((center.x + maxDist - mGridMin) / CELL_SIZE));
	int cz0 = std::max(0, (int)((center.z - maxDist - mGridMin) / CELL_SIZE));
	int cz1 = std::min(mGridW - 1, (int)((center.z + maxDist - mGridMin) / CELL_SIZE));

	for (int cz = cz0; cz <= cz1; cz++)
	{
		for (int cx = cx0; cx <= cx1; cx++)
		{
			// case entière hors du champ de vue
			if (!shadow)
			{
				glm::vec3 mn(mGridMin + cx * CELL_SIZE, mYMin, mGridMin + cz * CELL_SIZE);
				if (!frustum.intersectsBox(mn - glm::vec3(0, 0, 0), mn + glm::vec3(CELL_SIZE, mYMax - mYMin, CELL_SIZE)))
					continue;
			}
			for (int idx : mCells[(size_t)cz * mGridW + cx])
			{
				const Tree& t = mTrees[idx];
				float dx = t.x - center.x, dz = t.z - center.z;
				float d2 = dx * dx + dz * dz;
				if (d2 > maxDist * maxDist)
					continue;
				float h = std::max(mHeight[t.variant], 1.5f) * t.scale;
				if (!shadow && !frustum.intersectsSphere(glm::vec3(t.x, t.y + 0.5f * h, t.z), 0.7f * h))
					continue;

				float d = sqrtf(d2);
				int lod = std::min((d < lod0) ? 0 : (d < lod1) ? 1 : 2, mNumLods - 1);
				bucket[t.variant][lod].push_back(idx);
			}
		}
	}

	// 2. instances rangées dans un seul tampon, un bloc par variante et par LOD
	size_t total = 0;
	for (int v = 0; v < mNumVariants; v++)
		for (int l = 0; l < LODS; l++)
			total += bucket[v][l].size();
	mLastCount = (unsigned int)total;
	if (total == 0)
		return;

	mStaging.clear();
	mStaging.reserve(total * 8);
	size_t offsets[MAX_VARIANTS][LODS];
	for (int v = 0; v < mNumVariants; v++)
	{
		for (int l = 0; l < LODS; l++)
		{
			offsets[v][l] = mStaging.size() * sizeof(float);
			for (int idx : bucket[v][l])
			{
				const Tree& t = mTrees[idx];
				mStaging.insert(mStaging.end(), { t.x, t.y, t.z, t.yaw, t.scale, t.phase, t.tint, 0.0f });
			}
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, mInstanceVbo);
	size_t bytes = mStaging.size() * sizeof(float);
	if (bytes > mVboSize)
		mVboSize = bytes * 2;
	glBufferData(GL_ARRAY_BUFFER, mVboSize, nullptr, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, mStaging.data());

	// 3. un appel de dessin instancié par variante et par LOD
	for (int v = 0; v < mNumVariants; v++)
		for (int l = 0; l < LODS; l++)
			if (!bucket[v][l].empty())
				mModel->drawNodeInstanced(renderer, mNode[v][l], mInstanceVbo, offsets[v][l], (int)bucket[v][l].size(), mHeight[v]);
}

void Forest::queryNear(float x, float z, float radius, std::vector<Hit>& out) const
{
	if (mGridW == 0) return;
	int cx0 = std::max(0, (int)((x - radius - mGridMin) / CELL_SIZE)), cx1 = std::min(mGridW - 1, (int)((x + radius - mGridMin) / CELL_SIZE));
	int cz0 = std::max(0, (int)((z - radius - mGridMin) / CELL_SIZE)), cz1 = std::min(mGridW - 1, (int)((z + radius - mGridMin) / CELL_SIZE));
	for (int cz = cz0; cz <= cz1; cz++)
		for (int cx = cx0; cx <= cx1; cx++)
			for (int idx : mCells[(size_t)cz * mGridW + cx])
			{
				const Tree& t = mTrees[idx];
				float dx = t.x - x, dz = t.z - z;
				if (dx * dx + dz * dz < radius * radius)
					out.push_back({ t.x, t.y, t.z, t.scale });
			}
}

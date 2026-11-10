// arbres et rochers dessinés en instances
// - culling : seuls les arbres proches et dans le champ de la caméra
// - LOD : 3 versions de chaque arbre selon la distance
// - instanciation : un appel de dessin par variante et par LOD, 32 octets par instance
// les arbres sont rangés dans une grille de cases de 64 m
#ifndef FOREST_H
#define FOREST_H

#include <string>
#include <vector>
#include "AssetManager.h"
#include "Renderer.h"

class Forest
{
public:

	Forest();
	~Forest();
	Forest(const Forest&) = delete;
	Forest& operator=(const Forest&) = delete;

	bool load(AssetManager& assets, const std::string& modelPath, const std::string& instancesPath,
			  const std::string& prefix, float maxDist, float lod0Dist, float lod1Dist);

	void draw(Renderer& renderer);

	unsigned int getInstanceCountLastDraw() const { return mLastCount; }

	// obstacles autour d'un point, pour les collisions
	struct Hit { float x, y, z, scale; };
	void queryNear(float x, float z, float radius, std::vector<Hit>& out) const;

private:

	struct Tree
	{
		float x, y, z, yaw, scale;
		unsigned int variant;
		float phase, tint;
	};

	static const int MAX_VARIANTS = 8;
	static const int LODS = 3;
	static const float CELL_SIZE;

	Model* mModel;
	int mNode[MAX_VARIANTS][LODS];
	float mHeight[MAX_VARIANTS];
	int mNumVariants;
	int mNumLods;
	float mMaxDist, mLod0Dist, mLod1Dist;
	bool mHasWind;

	std::vector<Tree> mTrees;
	int mGridW;
	float mGridMin;
	std::vector<std::vector<int>> mCells;
	float mYMin, mYMax;

	GLuint mInstanceVbo;
	size_t mVboSize;
	std::vector<float> mStaging;
	unsigned int mLastCount;
};
#endif

// route de col chargée depuis assets/world/road.csv
// un échantillon par mètre : altitude, demi-largeur, dévers, courbure, indicateurs
#ifndef ROAD_H
#define ROAD_H

#include <string>
#include <unordered_map>
#include <vector>
#include "glm/glm.hpp"
#include "Mesh.h"

enum RoadFlags
{
	ROAD_HAIRPIN    = 1,
	ROAD_RAIL_LEFT  = 2,
	ROAD_RAIL_RIGHT = 4,
	ROAD_LAMP       = 8,
	ROAD_CHEVRON    = 16,
	ROAD_START      = 32,
	ROAD_FINISH     = 64
};

class Road
{
public:

	bool load(const std::string& csvFile);
	Mesh& getMesh() { return mMesh; }

	int getNumPoints() const { return (int)mCenters.size(); }
	const glm::vec3& getCenter(int i) const { return mCenters[i]; }
	const glm::vec3& getTangent(int i) const { return mTangents[i]; }
	const glm::vec3& getLeft(int i) const { return mLefts[i]; }
	float getHalfWidth(int i) const { return mHalfWidth[i]; }
	float getBank(int i) const { return mBank[i]; }
	float getCurvature(int i) const { return mCurvature[i]; }
	int   getFlags(int i) const { return mFlags[i]; }

	// échantillon le plus proche de (x, z), ou -1 ; lateral = décalage signé par rapport à l'axe (gauche > 0)
	int query(float x, float z, float& lateral, float maxDist = 20.0f) const;

	// altitude interpolée entre l'échantillon i et son voisin
	float surfaceHeightAt(int i, float x, float z, float lateral) const;

	glm::vec3 getSurfaceNormal(int i) const;

	float getSurfaceHeight(int i, float lateral) const { return mCenters[i].y + mBank[i] * lateral; }

private:

	std::vector<glm::vec3> mCenters, mTangents, mLefts;
	std::vector<float> mHalfWidth, mBank, mCurvature;
	std::vector<int> mFlags;
	std::unordered_map<long long, std::vector<int>> mGrid;
	static long long cellKey(int cx, int cz) { return (long long)cx * 100003LL + cz; }
	Mesh mMesh;
};
#endif

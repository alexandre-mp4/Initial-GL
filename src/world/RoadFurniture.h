// garde-corps, poteaux, panneaux et lignes de départ/arrivée, générés en code à partir des échantillons de la route
#ifndef ROADFURNITURE_H
#define ROADFURNITURE_H

#include <memory>
#include "AssetManager.h"
#include "Renderer.h"
#include "Road.h"
#include "Terrain.h"
#include "Material.h"

class RoadFurniture
{
public:

	void build(const Road& road, AssetManager& assets);
	void draw(Renderer& renderer);

private:

	std::unique_ptr<Mesh> mRail, mRailPosts, mReflectors, mDelineators, mSignPosts, mSignLeft, mSignRight, mLines;
	Material mRailMat, mPostMat, mReflectorMat, mDelineatorMat, mSignLeftMat, mSignRightMat, mLineMat;
};
#endif

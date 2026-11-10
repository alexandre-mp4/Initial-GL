// le monde du jeu : montagne, route, mobilier, voiture et lumières (unités en mètres)
#ifndef SCENE_H
#define SCENE_H

#include <string>
#include <vector>
#include "glm/glm.hpp"
#include "AssetManager.h"
#include "Renderer.h"
#include "Road.h"
#include "Terrain.h"
#include "Car.h"
#include "Model.h"
#include "RoadFurniture.h"
#include "Forest.h"
#include "Lights.h"
#include "Material.h"

class Scene : public PhysicsWorld
{
public:

	bool load(AssetManager& assets, const std::string& carId);

	bool setCarModel(AssetManager& assets, const std::string& carId);

	// avance la physique par pas fixes (accumulateur)
	void update(float deltaTime, const CarInput& input);

	bool changeCar(AssetManager& assets, const std::string& carId);
	void resetToStart();
	const std::string& getCarId() const { return mCarId; }
	const glm::vec3& getEyePosition() const { return mEyePos; }
	int getFinishIndex() const { return mRoad.getNumPoints() - 90; }
	float getRoadOffset() const { float l = 0.0f; mRoad.query(mCar.getPosition().x, mCar.getPosition().z, l, 60.0f); return l; }
	int getNearestRoadIndex() const { return mNearestRoadIndex; }

	// remet la voiture sur la route à l'endroit le plus proche, sinon au départ
	void respawnCar();

	// PhysicsWorld : ce que la physique demande au monde
	float groundHeight(float x, float z) const override;
	glm::vec3 groundNormal(float x, float z) const override;
	int pointContacts(const glm::vec3& p, Contact* out, int maxOut) const override;

	bool debugDraw = false;

	void render(Renderer& renderer, const Camera& camera, float aspectRatio, bool flashlight);

	Car& getCar()                 { return mCar; }
	const Car& getCar() const     { return mCar; }
	Terrain& getTerrain()         { return mTerrain; }
	const Road& getRoad() const   { return mRoad; }
	const glm::vec3& getStartDirection() const { return mStartDir; }
	const glm::vec3& getStartPosition() const  { return mStartPos; }

	bool headlightsOn = true;
	bool neonsOn = true;
	int  neonColorIndex = 0;
	static const int NUM_NEON_COLORS = 3;

private:

	// tous les objets du monde, utilisé dans la passe d'ombres et la passe principale
	void drawWorld(Renderer& renderer);

	void addCarLights(std::vector<PointLight>& pointLights, std::vector<SpotLight>& spotLights) const;

	Road mRoad;
	RoadFurniture mFurniture;
	Forest mForest, mRocks;
	Terrain mTerrain;
	Car mCar;
	glm::vec3 mStartPos, mStartDir;

	TerrainMaterial mTerrainMat;
	Material mRoadMat, mLampMat, mBulbMat;
	Mesh *mLampMesh, *mBulbMesh;

	// voiture : modèle glTF dont les roues sont des noeuds séparés (wheel_fl, wheel_fr, wheel_rl, wheel_rr)
	Model* mCarModel = nullptr;
	int mWheelNode[4];
	glm::vec3 mEyePos = glm::vec3(-0.38f, 1.1f, -0.05f);
	glm::vec3 mHeadlightPos, mTaillightPos;
	float mHalfTrack, mHalfWheelbase;
	std::vector<glm::mat4> mCarExtra;
	std::string mCarId;
	float mPhysicsAccumulator = 0.0f;
	int mStartIndex = 45;
	int mNearestRoadIndex = 0;
	int mLastRoadIndex = 0;

	std::vector<glm::vec3> mLampPos;
	std::vector<PointLight> mLampLights;
	DirLight mMoonLight;
};
#endif

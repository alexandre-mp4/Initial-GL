#include "Scene.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include "glm/gtc/matrix_transform.hpp"

static const float LAMP_LIGHT_HEIGHT = 3.8f;
static const int   MAX_LAMP_LIGHTS = 12;
static const int   START_INDEX = 45;

// couleurs des néons (touche M)
static const glm::vec3 NEON_COLORS[Scene::NUM_NEON_COLORS] = {
	glm::vec3(0.7f, 0.1f, 1.0f),
	glm::vec3(0.0f, 0.9f, 1.0f),
	glm::vec3(1.0f, 0.1f, 0.5f)
};

bool Scene::load(AssetManager& assets, const std::string& carId)
{
	// montagne et route générées par tools/gen_world.py
	if (!mTerrain.load("assets/world") || !mRoad.load("assets/world/road.csv"))
		return false;

	mFurniture.build(mRoad, assets);
	mForest.load(assets, "assets/models/trees/hemlock.glb", "assets/world/trees.bin", "tree", 430.0f, 34.0f, 115.0f);
	mRocks.load(assets, "assets/models/rocks/rocks.glb", "assets/world/rocks.bin", "rock", 260.0f, 1e9f, 1e9f);

	// matériau du terrain : 3 sols et la roche, mélangés dans le shader
	mTerrainMat.control      = assets.texture("assets/world/control.png", false, true, false);
	mTerrainMat.forestAlbedo = assets.texture("assets/textures/forest_floor/albedo.jpg", COLOR_TEXTURES_SRGB);
	mTerrainMat.forestNormal = assets.texture("assets/textures/forest_floor/normal.jpg");
	mTerrainMat.gravelAlbedo = assets.texture("assets/textures/grass/albedo.jpg", COLOR_TEXTURES_SRGB);
	mTerrainMat.gravelNormal = assets.texture("assets/textures/grass/normal.jpg");
	mTerrainMat.meadowAlbedo = assets.texture("assets/textures/meadow/albedo.jpg", COLOR_TEXTURES_SRGB);
	mTerrainMat.rockAlbedo   = assets.texture("assets/textures/rock_cliff/albedo.jpg", COLOR_TEXTURES_SRGB);
	mTerrainMat.rockNormal   = assets.texture("assets/textures/rock_cliff/normal.jpg");
	mTerrainMat.rockOrm      = assets.texture("assets/textures/rock_cliff/orm.jpg");
	mTerrainMat.worldSize    = mTerrain.getSize();

	// asphalte répété tous les ~2 m, avec les lignes peintes en calque
	mRoadMat.albedo = assets.texture("assets/textures/asphalt/albedo.jpg", COLOR_TEXTURES_SRGB);
	mRoadMat.normal = assets.texture("assets/textures/asphalt/normal.jpg");
	mRoadMat.orm    = assets.texture("assets/textures/asphalt/orm.jpg");
	mRoadMat.uvScale = glm::vec2(3.2f, 0.5f);
	mRoadMat.overlay = assets.texture("assets/textures/road_markings.png", COLOR_TEXTURES_SRGB);
	mRoadMat.overlayPeriod = 16.0f;
	mRoadMat.roughness = 0.9f;
	mRoadMat.specularF0 = 0.06f;
	mRoadMat.doubleSided = true;

	// un seul mesh de lampadaire, dessiné à chaque emplacement marqué dans la route
	mLampMesh = assets.mesh("models/lampPost.obj");
	mLampMat.albedo = assets.texture("textures/lamp_post_diffuse.png", COLOR_TEXTURES_SRGB);
	mLampMat.roughness = 0.4f;
	mLampMat.metallic = 0.5f;
	mLampMat.doubleSided = true;

	// ampoule : petit cube émissif, sa valeur HDR élevée fait rayonner le bloom
	mBulbMesh = assets.mesh("models/light.obj");
	mBulbMat.baseColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	mBulbMat.emissive = glm::vec3(30.0f, 14.0f, 4.0f);
	mBulbMat.doubleSided = true;

	for (int i = 0; i < mRoad.getNumPoints(); i++)
	{
		if (!(mRoad.getFlags(i) & ROAD_LAMP))
			continue;

		// côté extérieur du virage
		float side = (mRoad.getCurvature(i) < 0.0f) ? 1.0f : -1.0f;
		glm::vec3 p = mRoad.getCenter(i) + side * (mRoad.getHalfWidth(i) + 2.4f) * mRoad.getLeft(i);
		p.y = mTerrain.getHeight(p.x, p.z);
		mLampPos.push_back(p);
	}

	if (!setCarModel(assets, carId))
		return false;

	// départ sur la voie de gauche, dans le sens de la route
	// yaw : direction avant = (sin(yaw), 0, cos(yaw))
	mStartIndex = START_INDEX;
	mCar.setWorld(this);
	resetToStart();

	// lune : lumière directionnelle, sa direction suit celle du disque dans le ciel (mise à jour dans render)
	mMoonLight.direction = glm::vec3(0.0f, -1.0f, 0.0f);
	mMoonLight.ambient   = glm::vec3(0.002f, 0.003f, 0.006f);
	mMoonLight.diffuse   = glm::vec3(0.55f, 0.68f, 1.0f) * 0.42f;
	mMoonLight.specular  = glm::vec3(0.55f, 0.68f, 1.0f) * 0.42f;

	// une point light par lampadaire
	// atténuation 1 / (Kc + Kl*d + Kq*d^2)
	for (const glm::vec3& p : mLampPos)
	{
		PointLight lamp;
		lamp.position = p + glm::vec3(0.0f, LAMP_LIGHT_HEIGHT, 0.0f);
		lamp.ambient  = glm::vec3(0.05f, 0.03f, 0.01f);

		// intensité > 1 : multipliée ensuite par l'atténuation
		lamp.diffuse  = glm::vec3(2.2f, 1.3f, 0.5f);
		lamp.specular = glm::vec3(1.0f, 0.7f, 0.4f);
		lamp.constant = 1.0f;
		lamp.linear   = 0.09f;
		lamp.exponent = 0.032f;
		mLampLights.push_back(lamp);
	}

	std::cout << "Scène : " << mRoad.getNumPoints() << " m de route, " << mLampPos.size() << " lampadaires" << std::endl;
	return true;
}

// charge le modèle de voiture et en déduit empattement, voie et rayon des roues à partir des noeuds de roues
bool Scene::setCarModel(AssetManager& assets, const std::string& carId)
{
	Model* model = assets.model("assets/models/cars/" + carId + ".glb");
	if (!model)
		return false;

	const char* wheelNames[4] = { "wheel_fl", "wheel_fr", "wheel_rl", "wheel_rr" };
	for (int i = 0; i < 4; i++)
	{
		mWheelNode[i] = model->findNode(wheelNames[i]);
		if (mWheelNode[i] < 0)
		{
			std::cerr << "Voiture '" << carId << "' : noeud " << wheelNames[i] << " introuvable" << std::endl;
			return false;
		}
	}

	glm::vec3 fl = model->getNodePosition(mWheelNode[0]);
	glm::vec3 rl = model->getNodePosition(mWheelNode[2]);
	mHalfWheelbase = 0.5f * (fl.z - rl.z);
	mHalfTrack = fl.x;
	mCarId = carId;
	mCar.configure(carId, mHalfWheelbase, mHalfTrack, fl.y, model->getBoundsMin(), model->getBoundsMax());

	mHeadlightPos = model->getNodePosition(model->findNode("headlight_l"));
	mTaillightPos = model->getNodePosition(model->findNode("taillight_l"));
	int eyeNode = model->findNode("eye");
	if (eyeNode >= 0) mEyePos = model->getNodePosition(eyeNode);

	mCarExtra.assign(model->getNodeCount(), glm::mat4(1.0f));
	mCarModel = model;
	return true;
}

// pas de physique fixe de 1/240 s avec accumulateur, pour une simulation identique quelle que soit la fréquence d'images
void Scene::update(float deltaTime, const CarInput& input)
{
	mPhysicsAccumulator += glm::min(deltaTime, 0.1f);
	while (mPhysicsAccumulator >= Car::PHYSICS_DT)
	{
		mCar.step(input);
		mCar.limitArea(mTerrain.getHalfSize() - 6.0f);
		mPhysicsAccumulator -= Car::PHYSICS_DT;
	}

	// dernier point connu sur la route, pour la remise sur route
	float lat;
	int idx = mRoad.query(mCar.getPosition().x, mCar.getPosition().z, lat, 6.0f);
	if (idx >= 0 && fabsf(lat) < mRoad.getHalfWidth(idx))
		mLastRoadIndex = idx;
	int near = mRoad.query(mCar.getPosition().x, mCar.getPosition().z, lat, 45.0f);
	if (near >= 0)
		mNearestRoadIndex = near;

	// voiture retournée ou tombée : remise sur la route après un délai
	static float flipped = 0.0f;
	if (mCar.localToWorldDirection(glm::vec3(0, 1, 0)).y < 0.2f && mCar.getSpeedKmh() < 8.0f) flipped += deltaTime; else flipped = 0.0f;
	if (flipped > 2.5f) { respawnCar(); flipped = 0.0f; }
}

bool Scene::changeCar(AssetManager& assets, const std::string& carId)
{
	if (!setCarModel(assets, carId))
		return false;
	resetToStart();
	return true;
}

void Scene::resetToStart()
{
	int i = mStartIndex;
	mStartDir = mRoad.getTangent(i);
	mStartPos = mRoad.getCenter(i) + 1.7f * mRoad.getLeft(i);
	mCar.reset(mStartPos, atan2f(mStartDir.x, mStartDir.z));
	mLastRoadIndex = mNearestRoadIndex = i;
	mPhysicsAccumulator = 0.0f;
}

void Scene::respawnCar()
{
	int i = glm::clamp(mLastRoadIndex, 0, mRoad.getNumPoints() - 2);
	glm::vec3 d = mRoad.getTangent(i);
	mCar.reset(mRoad.getCenter(i) + 1.7f * mRoad.getLeft(i), atan2f(d.x, d.z));
}

// sol sous un point : surface de la route sur la chaussée, terrain ailleurs avec un fondu de 60 cm
float Scene::groundHeight(float x, float z) const
{
	float terrain = mTerrain.getHeight(x, z);
	float lat;
	int i = mRoad.query(x, z, lat, 14.0f);
	if (i < 0)
		return terrain;
	float hw = mRoad.getHalfWidth(i), a = fabsf(lat);
	if (a <= hw)
		return mRoad.surfaceHeightAt(i, x, z, lat);
	float edge = mRoad.surfaceHeightAt(i, x, z, lat > 0 ? hw : -hw);
	return glm::mix(edge, terrain, glm::smoothstep(hw, hw + 0.6f, a));
}

glm::vec3 Scene::groundNormal(float x, float z) const
{
	float lat;
	int i = mRoad.query(x, z, lat, 14.0f);
	if (i >= 0 && fabsf(lat) <= mRoad.getHalfWidth(i))
		return mRoad.getSurfaceNormal(i);
	return mTerrain.getNormal(x, z);
}

// obstacles solides pour un point de la carrosserie
int Scene::pointContacts(const glm::vec3& P, Contact* out, int maxOut) const
{
	int n = 0;

	// garde-corps : mur vertical à 60 cm du bord de la chaussée
	float lat;
	int i = mRoad.query(P.x, P.z, lat, 14.0f);
	if (i >= 0)
	{
		float surf = mRoad.surfaceHeightAt(i, P.x, P.z, glm::clamp(lat, -mRoad.getHalfWidth(i), mRoad.getHalfWidth(i)));
		float h = P.y - surf;
		if (h > 0.0f && h < 1.0f)
		{
			float wall = mRoad.getHalfWidth(i) + 0.6f;
			int flags = mRoad.getFlags(i);
			if ((flags & ROAD_RAIL_LEFT) && lat > wall && lat < wall + 1.5f && n < maxOut)
				out[n++] = { -mRoad.getLeft(i), lat - wall, 0.30f };
			if ((flags & ROAD_RAIL_RIGHT) && lat < -wall && lat > -wall - 1.5f && n < maxOut)
				out[n++] = { mRoad.getLeft(i), -lat - wall, 0.30f };
		}
	}

	// terrain : point sous le sol (paroi de déblai, talus raide)
	float g = groundHeight(P.x, P.z);
	if (g > P.y && n < maxOut)
	{
		glm::vec3 nrm = groundNormal(P.x, P.z);

		// une paroi raide repousse la voiture au lieu de la lancer en l'air : on aplatit la composante verticale de sa normale
		if (nrm.y < 0.9f)
			nrm = glm::normalize(glm::vec3(nrm.x, nrm.y * 0.2f, nrm.z));
		out[n++] = { nrm, (g - P.y) * 0.5f, 0.45f };
	}

	// troncs (cylindres) et rochers (demi-sphères)
	static thread_local std::vector<Forest::Hit> hits;
	hits.clear();
	mForest.queryNear(P.x, P.z, 3.0f, hits);
	for (const Forest::Hit& t : hits)
	{
		float dx = P.x - t.x, dz = P.z - t.z, d = sqrtf(dx * dx + dz * dz);
		float radius = 0.34f * t.scale + 0.05f;
		if (d < radius && P.y > t.y - 0.2f && n < maxOut)
			out[n++] = { glm::vec3(dx, 0.0f, dz) / std::max(d, 1e-3f), radius - d, 0.35f };
	}
	hits.clear();
	mRocks.queryNear(P.x, P.z, 4.0f, hits);
	for (const Forest::Hit& t : hits)
	{
		float dx = P.x - t.x, dz = P.z - t.z, d = sqrtf(dx * dx + dz * dz);
		float radius = 0.75f * t.scale;
		if (d < radius && P.y < t.y + 0.75f * t.scale && n < maxOut)
			out[n++] = { glm::vec3(dx, 0.0f, dz) / std::max(d, 1e-3f), radius - d, 0.45f };
	}
	return n;
}

void Scene::render(Renderer& renderer, const Camera& camera, float aspectRatio, bool flashlight)
{
	// la lumière de la lune va dans le sens opposé à sa direction dans le ciel
	glm::vec3 toMoon = renderer.getEnvironment().getMoonDirection();
	mMoonLight.direction = -toMoon;

	// lumières de l'image : lampadaires fixes (seuls les plus proches éclairent) et lumières mobiles recalculées
	std::vector<PointLight> pointLights;
	{
		std::vector<std::pair<float, int>> byDistance;
		for (size_t i = 0; i < mLampLights.size(); i++)
			byDistance.push_back(std::make_pair(glm::distance(mLampLights[i].position, camera.getPosition()), (int)i));
		std::sort(byDistance.begin(), byDistance.end());
		for (size_t k = 0; k < byDistance.size() && (int)k < MAX_LAMP_LIGHTS; k++)
			pointLights.push_back(mLampLights[byDistance[k].second]);
	}
	std::vector<SpotLight> spotLights;

	// lampe torche : spot parti de la caméra
	if (flashlight)
	{
		SpotLight torch;
		torch.position     = camera.getPosition() - glm::vec3(0.0f, 0.5f, 0.0f);
		torch.direction    = camera.getLook();
		torch.cosInnerCone = glm::cos(glm::radians(15.0f));
		torch.cosOuterCone = glm::cos(glm::radians(20.0f));
		torch.ambient      = glm::vec3(0.0f);
		torch.diffuse      = glm::vec3(0.8f);
		torch.specular     = glm::vec3(1.0f);
		torch.constant     = 1.0f;
		torch.linear       = 0.07f;
		torch.exponent     = 0.017f;
		spotLights.push_back(torch);
	}

	addCarLights(pointLights, spotLights);

	// passe d'ombres depuis la lune, centrée sur la voiture
	if (renderer.shadowsEnabled())
	{
		renderer.beginShadowPass(toMoon, mCar.getPosition());
		drawWorld(renderer);
		renderer.endShadowPass();
	}

	// passe principale : ciel puis monde
	renderer.beginScene(camera, aspectRatio);
	renderer.drawSky();
	renderer.setLights(mMoonLight, pointLights, spotLights);
	drawWorld(renderer);

	if (debugDraw)
	{
		DebugDraw& dbg = renderer.getDebugDraw();
		glm::vec3 origin = mCar.localToWorldPoint(glm::vec3(0.0f, 0.6f, 0.0f));
		dbg.line(origin, origin + mCar.getVelocity() * 0.5f, glm::vec3(0.0f, 1.2f, 1.5f));
		for (int i = 0; i < 4; i++)
		{
			const WheelState& w = mCar.getWheel(i);
			if (!w.contact) continue;
			dbg.line(w.contactPoint, w.contactPoint + w.springForce * 0.0002f, glm::vec3(0.1f, 1.5f, 0.1f));
			float slip = glm::clamp(fabsf(w.slipAngle) * 4.0f + fabsf(w.slipRatio), 0.0f, 1.0f);
			dbg.line(w.contactPoint, w.contactPoint + w.tireForce * 0.0006f, glm::mix(glm::vec3(1.5f, 1.4f, 0.1f), glm::vec3(2.0f, 0.1f, 0.1f), slip));
		}
	}

	// post-traitement et affichage
	renderer.endScene();
}

void Scene::drawWorld(Renderer& renderer)
{
	renderer.setTerrainMaterial(mTerrainMat);
	renderer.drawTerrain(mTerrain);

	// la route est 14 cm au-dessus du terrain : le décalage de polygone la fait gagner le test de profondeur de loin
	// elle ne projette pas d'ombre
	if (!renderer.isShadowPass())
	{
		renderer.setMaterial(mRoadMat);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -2.0f);
		renderer.draw(mRoad.getMesh(), glm::mat4(1.0f));
		glDisable(GL_POLYGON_OFFSET_FILL);
	}

	mFurniture.draw(renderer);
	mForest.draw(renderer);
	mRocks.draw(renderer);

	renderer.setMaterial(mLampMat);
	for (const glm::vec3& p : mLampPos)
		renderer.draw(*mLampMesh, glm::translate(glm::mat4(1.0f), p));

	renderer.setMaterial(mBulbMat);
	for (const glm::vec3& p : mLampPos)
		renderer.draw(*mBulbMesh, glm::translate(glm::mat4(1.0f), p + glm::vec3(0.0f, LAMP_LIGHT_HEIGHT, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.35f)));

	// roues : la matrice extra vaut braquage(Y) * rotation(X), appliquée autour du centre de la roue
	for (int i = 0; i < 4; i++)
	{
		bool front = (i < 2);
		glm::mat4 m = glm::rotate(glm::mat4(1.0f), mCar.getWheel(i).spin, glm::vec3(1.0f, 0.0f, 0.0f));
		if (front)
			m = glm::rotate(glm::mat4(1.0f), mCar.getWheelSteer(), glm::vec3(0.0f, 1.0f, 0.0f)) * m;

		// la roue monte quand le ressort est comprimé
		m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, mCar.getWheelOffset(i), 0.0f)) * m;
		mCarExtra[mWheelNode[i]] = m;
	}
	renderer.darkBackfaces = true;
	mCarModel->draw(renderer, mCar.getModelMatrix(), &mCarExtra);
	renderer.darkBackfaces = false;
}

// lumières de la voiture : phares, néons et feux arrière
// les emplacements sont convertis du repère local au monde avec localToWorldPoint
void Scene::addCarLights(std::vector<PointLight>& pointLights, std::vector<SpotLight>& spotLights) const
{
	// phares : spots légèrement inclinés vers le bas
	if (headlightsOn)
	{
		for (int side = -1; side <= 1; side += 2)
		{
			SpotLight headlight;
			headlight.position     = mCar.localToWorldPoint(glm::vec3(side * mHeadlightPos.x, mHeadlightPos.y, mHeadlightPos.z));
			headlight.direction    = mCar.localToWorldDirection(glm::vec3(0.0f, -0.06f, 1.0f));
			headlight.cosInnerCone = glm::cos(glm::radians(14.0f));
			headlight.cosOuterCone = glm::cos(glm::radians(28.0f));
			headlight.ambient      = glm::vec3(0.0f);
			headlight.diffuse      = glm::vec3(10.0f, 9.2f, 7.5f);
			headlight.specular     = glm::vec3(2.0f, 1.9f, 1.6f);
			headlight.constant     = 1.0f;
			headlight.linear       = 0.022f;
			headlight.exponent     = 0.0019f;
			spotLights.push_back(headlight);
		}
	}

	// néons : spots dirigés vers le sol sous la caisse
	if (neonsOn)
	{
		for (int sx = -1; sx <= 1; sx += 2)
		{
			for (int sz = -1; sz <= 1; sz += 2)
			{
				SpotLight neon;
				neon.position = mCar.localToWorldPoint(glm::vec3(sx * (mHalfTrack - 0.25f), 0.12f, sz * mHalfWheelbase));
				neon.direction = mCar.localToWorldDirection(glm::vec3(0.0f, -1.0f, 0.0f));
				neon.cosInnerCone = cosf(glm::radians(60.0f));
				neon.cosOuterCone = cosf(glm::radians(85.0f));
				neon.ambient  = glm::vec3(0.0f);
				neon.diffuse  = NEON_COLORS[neonColorIndex] * 5.0f;
				neon.specular = NEON_COLORS[neonColorIndex] * 0.25f;
				neon.constant = 1.0f;
				neon.linear   = 1.0f;
				neon.exponent = 2.0f;
				spotLights.push_back(neon);
			}
		}
	}

	// feux arrière : faibles en roulant, intenses au freinage
	float tailIntensity = mCar.isBraking() ? 4.0f : 0.6f;
	for (int side = -1; side <= 1; side += 2)
	{
		PointLight tail;
		tail.position = mCar.localToWorldPoint(glm::vec3(side * mTaillightPos.x, mTaillightPos.y, mTaillightPos.z - 0.1f));
		tail.ambient  = glm::vec3(0.0f);
		tail.diffuse  = glm::vec3(1.0f, 0.02f, 0.02f) * tailIntensity;
		tail.specular = glm::vec3(1.0f, 0.1f, 0.1f) * tailIntensity * 0.5f;
		tail.constant = 1.0f;
		tail.linear   = 0.35f;
		tail.exponent = 0.44f;
		pointLights.push_back(tail);
	}
}

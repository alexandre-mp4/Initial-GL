#include "RoadFurniture.h"
#include <cmath>
#include <iostream>
#include "MeshBuilder.h"
#include "glm/gtc/matrix_transform.hpp"

static const glm::vec3 UP(0.0f, 1.0f, 0.0f);

// profil en W de la glissière (x vers la route, y hauteur au-dessus de la chaussée)
static const int PROFILE_N = 9;
static const float PROFILE_X[PROFILE_N] = { 0.000f, 0.035f, 0.045f, 0.020f, 0.000f, 0.020f, 0.045f, 0.035f, 0.000f };
static const float PROFILE_Y[PROFILE_N] = { 0.440f, 0.475f, 0.520f, 0.565f, 0.600f, 0.635f, 0.680f, 0.715f, 0.750f };

static glm::vec3 roadPoint(const Road& road, int i, float lateral)
{
	glm::vec3 p = road.getCenter(i) + road.getLeft(i) * lateral;
	p.y = road.getSurfaceHeight(i, lateral);
	return p;
}

void RoadFurniture::build(const Road& road, AssetManager& assets)
{
	const int n = road.getNumPoints();
	MeshBuilder rail, railPosts, reflectors, delineators, signPosts, signLeft, signRight, lines;

	// garde-corps : le profil est extrudé le long de la route (une coupe tous les 2 m), un poteau tous les 4 m
	for (int side = -1; side <= 1; side += 2)
	{
		int flag = (side > 0) ? ROAD_RAIL_LEFT : ROAD_RAIL_RIGHT;
		int i = 0;
		while (i < n)
		{
			if (!(road.getFlags(i) & flag)) { i++; continue; }
			int a = i;
			while (i < n && (road.getFlags(i) & flag)) i++;
			int b = i;

			std::vector<int> stations;
			for (int k = a; k < b; k += 2) stations.push_back(k);
			if (stations.size() < 2) continue;

			unsigned int firstVertex = (unsigned int)rail.vertices().size();
			for (int idx : stations)
			{
				float lateral = side * (road.getHalfWidth(idx) + 0.6f);
				glm::vec3 base = roadPoint(road, idx, lateral);
				glm::vec3 toRoad = -(float)side * road.getLeft(idx);
				for (int p = 0; p < PROFILE_N; p++)
				{
					// normale 2D du profil : moyenne des deux segments voisins
					glm::vec2 n2(0.0f);
					for (int d = -1; d <= 0; d++)
					{
						int s0 = std::max(p + d, 0), s1 = std::min(p + d + 1, PROFILE_N - 1);
						if (s0 == s1) continue;
						glm::vec2 seg(PROFILE_X[s1] - PROFILE_X[s0], PROFILE_Y[s1] - PROFILE_Y[s0]);
						n2 += glm::normalize(glm::vec2(seg.y, -seg.x));
					}
					n2 = glm::normalize(n2);

					Vertex v;
					v.position = base + toRoad * PROFILE_X[p] + UP * PROFILE_Y[p];
					v.normal = glm::normalize(toRoad * n2.x + UP * n2.y);
					v.texCoords = glm::vec2((float)idx / 3.0f, (PROFILE_Y[p] - 0.44f) * 3.0f);
					rail.vertices().push_back(v);
				}
			}
			for (size_t s = 0; s + 1 < stations.size(); s++)
			{
				for (int p = 0; p + 1 < PROFILE_N; p++)
				{
					unsigned int v00 = firstVertex + (unsigned int)(s * PROFILE_N + p), v01 = v00 + 1;
					unsigned int v10 = v00 + PROFILE_N, v11 = v10 + 1;
					rail.indices().insert(rail.indices().end(), { v00, v10, v01, v01, v10, v11 });
				}
			}

			for (int k = a; k < b; k += 4)
			{
				float lateral = side * (road.getHalfWidth(k) + 0.6f);
				glm::vec3 base = roadPoint(road, k, lateral);
				glm::vec3 toRoad = -(float)side * road.getLeft(k);
				glm::vec3 tang = road.getTangent(k);
				railPosts.box(base - toRoad * 0.10f + UP * 0.38f, tang, UP, toRoad, 0.05f, 0.58f, 0.07f);
				if (((k - a) / 4) % 2 == 1)
					reflectors.box(base + toRoad * 0.050f + UP * 0.60f, tang, UP, toRoad, 0.06f, 0.035f, 0.004f);
			}
		}
	}

	for (int k = 12; k < n - 12; k += 24)
	{
		for (int side = -1; side <= 1; side += 2)
		{
			int flag = (side > 0) ? ROAD_RAIL_LEFT : ROAD_RAIL_RIGHT;
			bool nearRail = false;
			for (int d = -6; d <= 6; d += 3)
				if (road.getFlags(std::max(0, std::min(n - 1, k + d))) & flag) nearRail = true;
			if (nearRail || (road.getFlags(k) & ROAD_START)) continue;

			float lateral = side * (road.getHalfWidth(k) + 0.9f);
			glm::vec3 base = roadPoint(road, k, lateral);
			glm::vec3 toRoad = -(float)side * road.getLeft(k);
			glm::vec3 tang = road.getTangent(k);
			delineators.box(base + UP * 0.45f, tang, UP, toRoad, 0.04f, 0.55f, 0.04f);
			reflectors.box(base + toRoad * 0.042f + UP * 0.85f, tang, UP, toRoad, 0.05f, 0.09f, 0.004f);
		}
	}

	// panneaux chevrons à l'extérieur des épingles, face à la route
	int nSigns = 0;
	for (int k = 0; k < n; k++)
	{
		if (!(road.getFlags(k) & ROAD_CHEVRON)) continue;
		float side = (road.getCurvature(k) > 0.0f) ? -1.0f : 1.0f;
		float lateral = side * (road.getHalfWidth(k) + 1.9f);
		glm::vec3 base = roadPoint(road, k, lateral);
		glm::vec3 toRoad = -side * road.getLeft(k);
		glm::vec3 tang = road.getTangent(k);

		signPosts.box(base + UP * 0.85f, tang, UP, toRoad, 0.035f, 1.0f, 0.035f);

		glm::vec3 right = glm::normalize(glm::cross(-toRoad, UP));
		glm::vec3 center = base + toRoad * 0.06f + UP * 1.55f;
		MeshBuilder& target = (glm::dot(tang, right) > 0.0f) ? signRight : signLeft;
		target.quad(center - right * 0.5f - UP * 0.25f, center + right * 0.5f - UP * 0.25f,
					center + right * 0.5f + UP * 0.25f, center - right * 0.5f + UP * 0.25f, toRoad);
		nSigns++;
	}

	// lignes de départ et d'arrivée
	int lineIdx[2] = { 58, n - 90 };
	for (int s = 0; s < 2; s++)
	{
		int i0 = lineIdx[s], i1 = i0 + 2;
		float h0 = road.getHalfWidth(i0), h1 = road.getHalfWidth(i1);
		glm::vec3 a = roadPoint(road, i0, -h0) + UP * 0.012f, b = roadPoint(road, i0, h0) + UP * 0.012f;
		glm::vec3 c = roadPoint(road, i1, h1) + UP * 0.012f, d = roadPoint(road, i1, -h1) + UP * 0.012f;
		lines.quad(a, b, c, d, UP, glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));
	}

	mRail = rail.build();
	mRailPosts = railPosts.build();
	mReflectors = reflectors.build();
	mDelineators = delineators.build();
	mSignPosts = signPosts.build();
	mSignLeft = signLeft.build();
	mSignRight = signRight.build();
	mLines = lines.build();
	std::cout << "Mobilier : " << (rail.indices().size() / 3) << " triangles de glissière, " << nSigns << " panneaux" << std::endl;

	mRailMat.albedo = assets.texture("assets/textures/guardrail_metal/albedo.jpg", COLOR_TEXTURES_SRGB);
	mRailMat.normal = assets.texture("assets/textures/guardrail_metal/normal.jpg");
	mRailMat.orm    = assets.texture("assets/textures/guardrail_metal/orm.jpg");
	mRailMat.baseColor = glm::vec4(1.6f, 1.6f, 1.55f, 1.0f);
	mRailMat.roughness = 0.45f;
	mRailMat.metallic = 0.7f;
	mRailMat.doubleSided = true;

	mPostMat.baseColor = glm::vec4(0.38f, 0.40f, 0.41f, 1.0f);
	mPostMat.roughness = 0.6f;
	mPostMat.metallic = 0.5f;
	mPostMat.doubleSided = true;

	mReflectorMat.baseColor = glm::vec4(1.0f, 0.55f, 0.08f, 1.0f);
	mReflectorMat.emissive = glm::vec3(0.35f, 0.14f, 0.0f);
	mReflectorMat.roughness = 0.25f;
	mReflectorMat.doubleSided = true;

	mDelineatorMat.baseColor = glm::vec4(0.82f, 0.82f, 0.8f, 1.0f);
	mDelineatorMat.roughness = 0.6f;
	mDelineatorMat.doubleSided = true;

	mSignRightMat.albedo = assets.texture("assets/textures/chevron_right.png", COLOR_TEXTURES_SRGB);
	mSignLeftMat.albedo  = assets.texture("assets/textures/chevron_left.png", COLOR_TEXTURES_SRGB);
	for (Material* m : { &mSignRightMat, &mSignLeftMat })
	{
		m->roughness = 0.35f;
		m->emissive = glm::vec3(0.05f, 0.04f, 0.0f);
		m->doubleSided = true;
	}

	mLineMat.albedo = assets.texture("assets/textures/finish_line.png", COLOR_TEXTURES_SRGB);
	mLineMat.roughness = 0.7f;
	mLineMat.doubleSided = true;
}

void RoadFurniture::draw(Renderer& renderer)
{
	glm::mat4 identity(1.0f);
	if (mRail)       { renderer.setMaterial(mRailMat);      renderer.draw(*mRail, identity); }
	if (mRailPosts)  { renderer.setMaterial(mPostMat);      renderer.draw(*mRailPosts, identity); }
	if (mSignPosts)  { renderer.setMaterial(mPostMat);      renderer.draw(*mSignPosts, identity); }
	if (mDelineators){ renderer.setMaterial(mDelineatorMat); renderer.draw(*mDelineators, identity); }
	if (mReflectors) { renderer.setMaterial(mReflectorMat); renderer.draw(*mReflectors, identity); }
	if (mSignRight)  { renderer.setMaterial(mSignRightMat); renderer.draw(*mSignRight, identity); }
	if (mSignLeft)   { renderer.setMaterial(mSignLeftMat);  renderer.draw(*mSignLeft, identity); }

	if (mLines && !renderer.isShadowPass())
	{
		renderer.setMaterial(mLineMat);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-2.0f, -4.0f);
		renderer.draw(*mLines, identity);
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
}

#include "Road.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>

bool Road::load(const std::string& csvFile)
{
	std::ifstream in(csvFile);
	if (!in)
	{
		std::cerr << "Road : " << csvFile << " introuvable (lancer tools/gen_world.py)" << std::endl;
		return false;
	}

	std::string line;
	while (std::getline(in, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		float x, y, z, hw, bank, kappa;
		int flags;
		if (sscanf(line.c_str(), "%f,%f,%f,%f,%f,%f,%d", &x, &y, &z, &hw, &bank, &kappa, &flags) != 7)
			continue;
		mCenters.push_back(glm::vec3(x, y, z));
		mHalfWidth.push_back(hw);
		mBank.push_back(bank);
		mCurvature.push_back(kappa);
		mFlags.push_back(flags);
	}

	const int n = (int)mCenters.size();
	if (n < 3)
		return false;

	// tangente = différence centrée entre les voisins, gauche = haut x avant
	mTangents.resize(n);
	mLefts.resize(n);
	for (int i = 0; i < n; i++)
	{
		glm::vec3 d = mCenters[std::min(i + 1, n - 1)] - mCenters[std::max(i - 1, 0)];
		d.y = 0.0f;
		d = glm::normalize(d);
		mTangents[i] = d;
		mLefts[i] = glm::vec3(d.z, 0.0f, -d.x);
	}

	// grille spatiale pour query
	for (int i = 0; i < n; i++)
		mGrid[cellKey((int)floorf(mCenters[i].x / 12.0f), (int)floorf(mCenters[i].z / 12.0f))].push_back(i);

	// deux sommets par échantillon (bord droit, bord gauche) ; normale = produit vectoriel route x latéral
	std::vector<Vertex> verts;
	verts.reserve(6 * n);
	const float SKIRT = 0.30f;
	for (int i = 0; i < n; i++)
	{
		glm::vec3 c = mCenters[i];
		float hw = mHalfWidth[i];
		glm::vec3 L = mLefts[i];

		glm::vec3 fwd3 = mCenters[std::min(i + 1, n - 1)] - mCenters[std::max(i - 1, 0)];
		glm::vec3 lat3 = glm::normalize(glm::vec3(L.x, mBank[i], L.z));
		glm::vec3 normal = glm::normalize(glm::cross(glm::normalize(fwd3), lat3));
		if (normal.y < 0) normal = -normal;

		for (int side = 0; side < 2; side++)
		{
			float lateral = side ? hw : -hw;
			glm::vec3 edge = c + L * lateral + glm::vec3(0.0f, mBank[i] * lateral, 0.0f);

			Vertex v;
			v.position = edge;
			v.normal = normal;
			v.texCoords = glm::vec2(side ? 1.0f : 0.0f, (float)i);
			v.tangent = glm::vec4(lat3, -1.0f);
			verts.push_back(v);
		}

		// flancs de 30 cm sous la chaussée pour ne pas voir de vide avec le terrain
		for (int side = 0; side < 2; side++)
		{
			float lateral = side ? hw : -hw;
			glm::vec3 top = c + L * lateral + glm::vec3(0.0f, mBank[i] * lateral, 0.0f);
			glm::vec3 out = side ? L : -L;
			for (int k = 0; k < 2; k++)
			{
				Vertex v;
				v.position = top - glm::vec3(0.0f, k * SKIRT, 0.0f);
				v.normal = out;
				v.texCoords = glm::vec2(side ? 1.0f : 0.0f, (float)i);
				v.tangent = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
				verts.push_back(v);
			}
		}
	}
	const unsigned int STRIDE = 6;

	// triangles (R_i, R_i+1, L_i) et (L_i, R_i+1, L_i+1)
	std::vector<unsigned int> idx;
	idx.reserve(18 * (n - 1));
	for (int i = 0; i < n - 1; i++)
	{
		unsigned int a0 = STRIDE * i, a1 = STRIDE * (i + 1);
		unsigned int r0 = a0, l0 = a0 + 1, r1 = a1, l1 = a1 + 1;
		idx.insert(idx.end(), { r0, r1, l0, l0, r1, l1 });

		idx.insert(idx.end(), { a0 + 2, a0 + 3, a1 + 2, a1 + 2, a0 + 3, a1 + 3 });
		idx.insert(idx.end(), { a0 + 4, a0 + 5, a1 + 4, a1 + 4, a0 + 5, a1 + 5 });
	}

	std::cout << "Road : " << n << " échantillons (" << n << " m)" << std::endl;
	return mMesh.loadIndexed(verts, idx);
}

int Road::query(float x, float z, float& lateral, float maxDist) const
{
	int cx = (int)floorf(x / 12.0f), cz = (int)floorf(z / 12.0f);
	int best = -1;
	float bestD2 = maxDist * maxDist;
	for (int dz = -1; dz <= 1; dz++)
	{
		for (int dx = -1; dx <= 1; dx++)
		{
			auto it = mGrid.find(cellKey(cx + dx, cz + dz));
			if (it == mGrid.end()) continue;
			for (int i : it->second)
			{
				float ex = x - mCenters[i].x, ez = z - mCenters[i].z;
				float d2 = ex * ex + ez * ez;
				if (d2 < bestD2) { bestD2 = d2; best = i; }
			}
		}
	}
	if (best >= 0)
		lateral = (x - mCenters[best].x) * mLefts[best].x + (z - mCenters[best].z) * mLefts[best].z;
	return best;
}

glm::vec3 Road::getSurfaceNormal(int i) const
{
	int n = (int)mCenters.size();
	glm::vec3 fwd = glm::normalize(mCenters[std::min(i + 1, n - 1)] - mCenters[std::max(i - 1, 0)]);
	glm::vec3 lat = glm::normalize(glm::vec3(mLefts[i].x, mBank[i], mLefts[i].z));
	glm::vec3 nrm = glm::normalize(glm::cross(fwd, lat));
	return nrm.y < 0 ? -nrm : nrm;
}

float Road::surfaceHeightAt(int i, float x, float z, float lateral) const
{
	int n = (int)mCenters.size();
	float ex = x - mCenters[i].x, ez = z - mCenters[i].z;
	float along = ex * mTangents[i].x + ez * mTangents[i].z;
	int j = glm::clamp(along >= 0.0f ? i + 1 : i - 1, 0, n - 1);
	float seg = glm::length(glm::vec2(mCenters[j].x - mCenters[i].x, mCenters[j].z - mCenters[i].z));
	float t = (seg > 1e-4f) ? glm::clamp(fabsf(along) / seg, 0.0f, 1.0f) : 0.0f;
	float centerY = glm::mix(mCenters[i].y, mCenters[j].y, t);
	float bank = glm::mix(mBank[i], mBank[j], t);
	return centerY + bank * lateral;
}

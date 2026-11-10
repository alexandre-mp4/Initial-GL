// aspect d'une surface : l'éclairage est du Blinn-Phong dont les paramètres viennent de textures de type PBR
// albedo : couleur (sRGB), normal : normal map (repère tangent)
// orm : R = occlusion ambiante, G = roughness, B = metallic
#ifndef MATERIAL_H
#define MATERIAL_H

#include "glm/glm.hpp"
#include "Texture2D.h"

// les textures de couleur sont en sRGB : le GPU les convertit en linéaire à la lecture, le post-traitement ré-encode à la fin
const bool COLOR_TEXTURES_SRGB = true;

struct Material
{
	Texture2D* albedo = nullptr;
	Texture2D* normal = nullptr;
	Texture2D* orm    = nullptr;

	glm::vec4 baseColor = glm::vec4(1.0f);
	float roughness = 0.8f;
	float metallic = 0.0f;
	float specularF0 = 0.04f;
	glm::vec3 emissive = glm::vec3(0.0f);

	float alphaCutoff = -1.0f;
	bool blend = false;
	bool doubleSided = false;
	glm::vec2 uvScale = glm::vec2(1.0f);

	// calque RGBA peint par-dessus l'albédo (lignes de la route), répété tous les overlayPeriod mètres
	Texture2D* overlay = nullptr;
	float overlayPeriod = 16.0f;
};

// le terrain mélange plusieurs textures selon la pente, l'altitude et une carte de contrôle
struct TerrainMaterial
{
	Texture2D* control = nullptr;
	Texture2D* forestAlbedo = nullptr; Texture2D* forestNormal = nullptr;
	Texture2D* gravelAlbedo = nullptr; Texture2D* gravelNormal = nullptr;
	Texture2D* meadowAlbedo = nullptr;
	Texture2D* rockAlbedo = nullptr;   Texture2D* rockNormal = nullptr;   Texture2D* rockOrm = nullptr;
	float worldSize = 2016.0f;
};
#endif

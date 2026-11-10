// dessin d'une image en plusieurs passes :
// 1. passe d'ombres depuis la lune (ShadowMap)
// 2. passe principale dans une image HDR hors écran (RenderTarget)
// 3. post-traitement : bloom, tone mapping, gamma (PostProcess)
// Scene décrit ce qu'il faut dessiner, le Renderer décide comment selon la passe
#ifndef RENDERER_H
#define RENDERER_H

#include <vector>
#include "glm/glm.hpp"
#include "ShaderProgram.h"
#include "Texture2D.h"
#include "Mesh.h"
#include "Camera.h"
#include "Lights.h"
#include "Material.h"
#include "RenderTarget.h"
#include "ShadowMap.h"
#include "Environment.h"
#include "PostProcess.h"
#include "Frustum.h"
#include "Terrain.h"
#include "DebugDraw.h"

// 3 niveaux de qualité (touche Q)
struct QualityPreset
{
	const char* name;
	float renderScale;
	int msaa;
	int shadowSize;
	bool bloom;
	float lodScale;
};

class Renderer
{
public:
	bool darkBackfaces = false;

	static const int NUM_PRESETS = 3;

	bool init(int framebufferWidth, int framebufferHeight);

	void resize(int framebufferWidth, int framebufferHeight);

	void setQuality(int preset);
	int  getQuality() const { return mPreset; }
	const QualityPreset& getQualityPreset() const;

	bool shadowsEnabled() const { return mShadowsOn; }
	void beginShadowPass(const glm::vec3& toLight, const glm::vec3& focus);
	void endShadowPass();

	void beginScene(const Camera& camera, float aspectRatio);
	void drawSky();
	void setLights(const DirLight& dirLight,
				   const std::vector<PointLight>& pointLights,
				   const std::vector<SpotLight>& spotLights);
	void setMaterial(const Material& material);
	void setTerrainMaterial(const TerrainMaterial& material);
	void draw(Mesh& mesh, const glm::mat4& model);

	// dessin instancié pour la forêt
	void drawInstanced(Mesh& mesh, GLuint instanceVbo, size_t byteOffset, int count, float treeHeight);
	void drawTerrain(Terrain& terrain);
	void endScene();

	Environment& getEnvironment() { return mEnvironment; }
	const Frustum& getFrustum() const { return mFrustum; }
	glm::mat4 getViewProjection() const { return mProjection * mView; }
	DebugDraw& getDebugDraw() { return mDebug; }
	const glm::vec3& getEye() const { return mEye; }
	float getLodScale() const { return getQualityPreset().lodScale; }
	bool isShadowPass() const { return mInShadowPass; }
	const glm::vec3& getShadowFocus() const { return mShadowFocus; }

	// fil de fer (F1), seulement pour la scène
	void setWireframe(bool on) { mWireframe = on; }

	float exposure = 1.0f;
	float bloomStrength = 0.10f;
	float fogDensity = 0.0017f;
	float time = 0.0f;
	float windStrength = 0.45f;
	float shadowExtent = 45.0f;

private:

	void rebuildTargets();

	ShaderProgram mShader, mShadowShader;
	Texture2D mWhite;
	Texture2D mFlatNormal, mClear;
	RenderTarget mTarget;
	ShadowMap mShadowMap;
	Environment mEnvironment;
	PostProcess mPost;
	DebugDraw mDebug;

	int mFbWidth, mFbHeight;
	int mPreset;
	bool mShadowsOn, mInShadowPass, mSkipDraw, mWireframe = false;
	glm::mat4 mView, mProjection;
	Frustum mFrustum;
	glm::vec3 mEye, mShadowFocus;
};
#endif

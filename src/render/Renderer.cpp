#include "Renderer.h"
#include <iostream>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/matrix_inverse.hpp"

static const QualityPreset PRESETS[Renderer::NUM_PRESETS] = {
	{ "Faible", 0.75f, 0, 1024, false, 0.7f },
	{ "Moyen",  1.0f,  0, 1024, true,  1.0f },
	{ "Eleve",  1.0f,  4, 2048, true,  1.3f }
};

// unités de texture : 0, 1, 2 pour les matériaux, 3 ciel, 4 ombres, 5 calque, 6 à 14 terrain
static const int UNIT_ENV = 3;
static const int UNIT_SHADOW = 4;
static const int UNIT_OVERLAY = 5;

static const int UNIT_TERRAIN = 6;

bool Renderer::init(int framebufferWidth, int framebufferHeight)
{
	mFbWidth = framebufferWidth;
	mFbHeight = framebufferHeight;
	mPreset = 1;
	mShadowsOn = false;
	mInShadowPass = mSkipDraw = false;

	mWhite.createSolid(255, 255, 255);
	mFlatNormal.createSolid(128, 128, 255);
	mClear.createSolid(255, 255, 255, 0);

	if (!mShader.loadShaders("shaders/scene.vert", "shaders/scene.frag")) return false;
	if (!mShadowShader.loadShaders("shaders/shadow.vert", "shaders/shadow.frag")) return false;
	if (!mEnvironment.create()) return false;
	if (!mPost.init()) return false;
	if (!mDebug.init()) return false;

	rebuildTargets();
	return true;
}

void Renderer::resize(int framebufferWidth, int framebufferHeight)
{
	if (framebufferWidth == mFbWidth && framebufferHeight == mFbHeight)
		return;
	mFbWidth = framebufferWidth;
	mFbHeight = framebufferHeight;
	rebuildTargets();
}

void Renderer::setQuality(int preset)
{
	mPreset = ((preset % NUM_PRESETS) + NUM_PRESETS) % NUM_PRESETS;
	rebuildTargets();
}

const QualityPreset& Renderer::getQualityPreset() const
{
	return PRESETS[mPreset];
}

void Renderer::rebuildTargets()
{
	const QualityPreset& q = PRESETS[mPreset];

	// image HDR à l'échelle de rendu demandée, étirée ensuite sur la fenêtre
	int w = std::max(16, (int)(mFbWidth * q.renderScale));
	int h = std::max(16, (int)(mFbHeight * q.renderScale));
	mTarget.create(w, h, q.msaa);
	mPost.resize(w, h);

	mShadowsOn = q.shadowSize > 0;
	if (mShadowsOn)
		mShadowMap.create(q.shadowSize);
	else
		mShadowMap.destroy();
}

void Renderer::beginShadowPass(const glm::vec3& toLight, const glm::vec3& focus)
{
	mInShadowPass = true;
	mShadowFocus = focus;
	mShadowMap.begin(toLight, focus, shadowExtent);
	mShadowShader.use();
	mShadowShader.setUniform("lightSpace", mShadowMap.getLightSpaceMatrix());
	mShadowShader.setUniform("alphaCutoff", -1.0f);
	mShadowShader.setUniform("instanced", (GLint)0);
}

void Renderer::endShadowPass()
{
	mShadowMap.end();
	mInShadowPass = false;
}

void Renderer::beginScene(const Camera& camera, float aspectRatio)
{
	mTarget.bind();
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// glClear(depth) n'efface rien si l'écriture de profondeur est coupée (après les objets transparents)
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// plan lointain à 700 m pour voir l'autre bout du terrain
	mView = camera.getViewMatrix();
	mProjection = glm::perspective(glm::radians(camera.getFOV()), aspectRatio, 0.12f, 2500.0f);
	mFrustum.set(mProjection * mView);
	mEye = camera.getPosition();

	mShader.use();
	mShader.setUniform("view", mView);
	mShader.setUniform("projection", mProjection);
	mShader.setUniform("viewPos", camera.getPosition());

	mShader.setUniform("material.ambient", glm::vec3(1.0f));
	mShader.setUniformSampler("material.albedoMap", 0);
	mShader.setUniformSampler("material.normalMap", 1);
	mShader.setUniformSampler("material.ormMap", 2);
	mShader.setUniformSampler("material.overlayMap", UNIT_OVERLAY);
	const char* terrainSamplers[9] = { "tControl", "tForestA", "tForestN", "tGravelA", "tGravelN", "tMeadowA", "tRockA", "tRockN", "tRockO" };
	for (int i = 0; i < 9; i++)
		mShader.setUniformSampler(terrainSamplers[i], UNIT_TERRAIN + i);
	mShader.setUniform("terrainMode", (GLint)0);
	mShader.setUniformSampler("envMap", UNIT_ENV);
	mShader.setUniformSampler("shadowMap", UNIT_SHADOW);

	// valeurs par défaut sur les unités des matériaux, pour éviter un avertissement du pilote sur les samplers vides
	mFlatNormal.bind(1); mWhite.bind(2); mClear.bind(UNIT_OVERLAY);
	mEnvironment.bind(UNIT_ENV);
	mShader.setUniform("skyIntensity", mEnvironment.getSkyIntensity());
	mShader.setUniform("envMaxLod", mEnvironment.getMaxLod());
	mShader.setUniform("fogDensity", fogDensity);

	mShader.setUniform("shadowsOn", (GLint)mShadowsOn);
	if (mShadowsOn)
	{
		mShadowMap.bindTexture(UNIT_SHADOW);
		mShader.setUniform("lightSpace", mShadowMap.getLightSpaceMatrix());
	}
}

void Renderer::drawSky()
{
	mEnvironment.drawSky(mView, mProjection);
	if (mWireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	mShader.use();
	mEnvironment.bind(UNIT_ENV);
}

void Renderer::setLights(const DirLight& dirLight,
						 const std::vector<PointLight>& pointLights,
						 const std::vector<SpotLight>& spotLights)
{
	mShader.use();
	sendLights(mShader, dirLight, pointLights, spotLights);
}

void Renderer::setMaterial(const Material& m)
{
	if (mInShadowPass)
	{
		mSkipDraw = m.blend;

		// feuillage découpé : le shader d'ombres lit la texture pour ignorer les pixels transparents
		mShadowShader.setUniform("alphaCutoff", m.alphaCutoff);
		if (m.alphaCutoff >= 0.0f && m.albedo)
		{
			m.albedo->bind(0);
			mShadowShader.setUniformSampler("albedoMap", 0);
		}
		return;
	}

	// sans albédo, texture blanche (neutre pour la multiplication par baseColor)
	if (m.albedo) m.albedo->bind(0); else mWhite.bind(0);
	if (m.normal) m.normal->bind(1);
	if (m.orm)    m.orm->bind(2);
	mShader.setUniform("material.hasNormalMap", (GLint)(m.normal != nullptr));
	mShader.setUniform("material.hasOrmMap", (GLint)(m.orm != nullptr));

	mShader.setUniform("material.baseColor", m.baseColor);
	mShader.setUniform("material.roughness", m.roughness);
	mShader.setUniform("material.metallic", m.metallic);
	mShader.setUniform("material.specularF0", m.specularF0);
	mShader.setUniform("material.emissive", m.emissive);
	mShader.setUniform("material.alphaCutoff", m.alphaCutoff);
	mShader.setUniform("material.uvScale", m.uvScale);
	mShader.setUniform("terrainMode", (GLint)0);
	mShader.setUniform("darkBackfaces", (GLint)darkBackfaces);
	mShader.setUniform("material.hasOverlay", (GLint)(m.overlay != nullptr));
	if (m.overlay)
	{
		m.overlay->bind(UNIT_OVERLAY);
		mShader.setUniform("material.overlayPeriod", m.overlayPeriod);
	}

	// faces arrière éliminées sauf matériau double face
	if (m.doubleSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);

	// transparence : mélange alpha sans écriture dans le depth buffer
	if (m.blend)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
	}
	else
	{
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}
}

void Renderer::setTerrainMaterial(const TerrainMaterial& t)
{
	if (mInShadowPass)
	{
		mSkipDraw = false;
		mShadowShader.setUniform("alphaCutoff", -1.0f);
		return;
	}

	const Texture2D* tex[9] = { t.control, t.forestAlbedo, t.forestNormal, t.gravelAlbedo, t.gravelNormal,
								t.meadowAlbedo, t.rockAlbedo, t.rockNormal, t.rockOrm };
	for (int i = 0; i < 9; i++)
		if (tex[i]) const_cast<Texture2D*>(tex[i])->bind(UNIT_TERRAIN + i);

	mShader.setUniform("terrainMode", (GLint)1);
	mShader.setUniform("tWorldSize", t.worldSize);
	mShader.setUniform("material.hasOverlay", (GLint)0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

void Renderer::drawTerrain(Terrain& terrain)
{
	glm::mat4 identity(1.0f);
	if (mInShadowPass)
	{
		mShadowShader.setUniform("model", identity);
		terrain.drawShadow(mShadowFocus, shadowExtent * 1.7f);
		return;
	}
	mShader.setUniform("model", identity);
	mShader.setUniform("normalMatrix", glm::mat3(1.0f));
	terrain.draw(mFrustum, mEye, getQualityPreset().lodScale);
}

void Renderer::draw(Mesh& mesh, const glm::mat4& model)
{
	if (mInShadowPass)
	{
		if (!mSkipDraw)
		{
			mShadowShader.setUniform("model", model);
			mesh.draw();
		}
		return;
	}

	mShader.setUniform("model", model);

	// matrice normale = transposée de l'inverse du 3x3 du modèle
	mShader.setUniform("normalMatrix", glm::inverseTranspose(glm::mat3(model)));

	mesh.draw();
}

void Renderer::drawInstanced(Mesh& mesh, GLuint instanceVbo, size_t byteOffset, int count, float treeHeight)
{
	ShaderProgram& sh = mInShadowPass ? mShadowShader : mShader;
	if (mInShadowPass && mSkipDraw)
		return;

	sh.setUniform("instanced", (GLint)1);
	sh.setUniform("treeHeight", treeHeight);
	sh.setUniform("time", time);
	sh.setUniform("windStrength", windStrength);
	if (!mInShadowPass)
	{
		mShader.setUniform("model", glm::mat4(1.0f));
		mShader.setUniform("normalMatrix", glm::mat3(1.0f));
	}
	mesh.drawInstanced(instanceVbo, byteOffset, count);
	sh.setUniform("instanced", (GLint)0);
}

void Renderer::endScene()
{
	mDebug.flush(mProjection * mView);
	glDepthMask(GL_TRUE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	mTarget.resolve();

	const QualityPreset& q = PRESETS[mPreset];
	mPost.run(mTarget.getTexture(), mFbWidth, mFbHeight, q.bloom, bloomStrength, exposure);
}

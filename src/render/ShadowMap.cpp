#include "ShadowMap.h"
#include <cmath>
#include <iostream>
#include "glm/gtc/matrix_transform.hpp"

ShadowMap::ShadowMap() : mFbo(0), mTexture(0), mSize(0), mLightSpace(1.0f)
{
}

ShadowMap::~ShadowMap()
{
	destroy();
}

void ShadowMap::destroy()
{
	if (mFbo) glDeleteFramebuffers(1, &mFbo);
	if (mTexture) glDeleteTextures(1, &mTexture);
	mFbo = mTexture = 0;
}

bool ShadowMap::create(int size)
{
	destroy();
	mSize = size;

	glGenTextures(1, &mTexture);
	glBindTexture(GL_TEXTURE_2D, mTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	// comparaison de profondeur matérielle (PCF 2x2) pour adoucir les bords
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &mFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mTexture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	if (!ok) std::cerr << "ShadowMap : framebuffer incomplet" << std::endl;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return ok;
}

void ShadowMap::begin(const glm::vec3& toLight, const glm::vec3& focus, float halfExtent)
{
	glm::vec3 up(0.0f, 1.0f, 0.0f);
	glm::mat4 view = glm::lookAt(toLight * 1000.0f, glm::vec3(0.0f), up);

	// ancrage sur la grille de texels pour éviter le scintillement des ombres
	float texel = 2.0f * halfExtent / (float)mSize;
	glm::vec3 f = glm::vec3(view * glm::vec4(focus, 1.0f));
	float cx = floorf(f.x / texel) * texel;
	float cy = floorf(f.y / texel) * texel;

	float nearPlane = glm::max(0.1f, -f.z - 140.0f);
	float farPlane = -f.z + 140.0f;
	glm::mat4 proj = glm::ortho(cx - halfExtent, cx + halfExtent, cy - halfExtent, cy + halfExtent, nearPlane, farPlane);
	mLightSpace = proj * view;

	glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
	glViewport(0, 0, mSize, mSize);
	glClear(GL_DEPTH_BUFFER_BIT);

	// décalage de polygone contre l'auto-ombrage
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);
}

void ShadowMap::end()
{
	glDisable(GL_POLYGON_OFFSET_FILL);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::bindTexture(GLuint unit)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, mTexture);
}

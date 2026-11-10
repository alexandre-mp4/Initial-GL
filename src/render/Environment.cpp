#include "Environment.h"
#include <cmath>
#include <iostream>
#include <vector>
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/constants.hpp"

Environment::Environment()
	: mTexture(0), mVao(0), mMoonDir(0.0f, 1.0f, 0.0f), mMoonSize(0.022f), mMaxLod(0.0f)
{
	setMoon(glm::radians(215.0f), glm::radians(38.0f));
}

void Environment::setMoon(float azimuth, float elevation)
{
	// coordonnées sphériques vers vecteur
	mMoonDir = glm::vec3(cosf(elevation) * sinf(azimuth), sinf(elevation), cosf(elevation) * cosf(azimuth));
	if (mTexture)
		bake();
}

Environment::~Environment()
{
	if (mTexture) glDeleteTextures(1, &mTexture);
	if (mVao) glDeleteVertexArrays(1, &mVao);
}

bool Environment::create()
{
	const int W = 512, H = 256;
	glGenTextures(1, &mTexture);
	glBindTexture(GL_TEXTURE_2D, mTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, W, H, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	mMaxLod = floorf(log2f((float)W));

	glGenVertexArrays(1, &mVao);

	if (!mSkyShader.loadShaders("shaders/fullscreen.vert", "shaders/sky.frag"))
		return false;
	bake();
	return true;
}

void Environment::bake()
{
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
	glViewport(0, 0, 512, 256);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	mSkyShader.use();
	mSkyShader.setUniform("bakeMode", (GLint)1);
	mSkyShader.setUniform("moonDir", mMoonDir);
	mSkyShader.setUniform("moonSize", mMoonSize);
	glBindVertexArray(mVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	glEnable(GL_DEPTH_TEST);

	glBindTexture(GL_TEXTURE_2D, mTexture);
	glGenerateMipmap(GL_TEXTURE_2D);
}

void Environment::bind(GLuint unit)
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, mTexture);
}

void Environment::drawSky(const glm::mat4& view, const glm::mat4& projection)
{
	// la translation de la vue est retirée : le ciel est à l'infini
	glm::mat4 rotationOnly = glm::mat4(glm::mat3(view));
	glm::mat4 invViewProj = glm::inverse(projection * rotationOnly);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	mSkyShader.use();
	mSkyShader.setUniform("invViewProj", invViewProj);
	mSkyShader.setUniform("bakeMode", (GLint)0);
	mSkyShader.setUniform("moonDir", mMoonDir);
	mSkyShader.setUniform("moonSize", mMoonSize);

	glBindVertexArray(mVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);
}

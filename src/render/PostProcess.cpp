#include "PostProcess.h"
#include <algorithm>

PostProcess::PostProcess() : mVao(0), mDummy(0), mLevels(0), mSrcW(0), mSrcH(0)
{
	for (int i = 0; i < MAX_LEVELS; i++) { mTex[i] = mFbo[i] = 0; mW[i] = mH[i] = 0; }
}

PostProcess::~PostProcess()
{
	destroyLevels();
	if (mVao) glDeleteVertexArrays(1, &mVao);
	if (mDummy) glDeleteTextures(1, &mDummy);
}

bool PostProcess::init()
{
	glGenVertexArrays(1, &mVao);

	glGenTextures(1, &mDummy);
	glBindTexture(GL_TEXTURE_2D, mDummy);
	float black[4] = { 0, 0, 0, 1 };
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_FLOAT, black);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	return mDown.loadShaders("shaders/fullscreen.vert", "shaders/bloom_down.frag")
		&& mUp.loadShaders("shaders/fullscreen.vert", "shaders/bloom_up.frag")
		&& mPost.loadShaders("shaders/fullscreen.vert", "shaders/post.frag");
}

void PostProcess::destroyLevels()
{
	for (int i = 0; i < MAX_LEVELS; i++)
	{
		if (mFbo[i]) glDeleteFramebuffers(1, &mFbo[i]);
		if (mTex[i]) glDeleteTextures(1, &mTex[i]);
		mFbo[i] = mTex[i] = 0;
	}
	mLevels = 0;
}

void PostProcess::resize(int width, int height)
{
	destroyLevels();
	mSrcW = width;
	mSrcH = height;

	// niveau 0 = moitié de l'image, chaque niveau est la moitié du précédent
	int w = std::max(1, width / 2), h = std::max(1, height / 2);
	for (int i = 0; i < MAX_LEVELS && w >= 2 && h >= 2; i++)
	{
		mW[i] = w; mH[i] = h;
		glGenTextures(1, &mTex[i]);
		glBindTexture(GL_TEXTURE_2D, mTex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &mFbo[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, mFbo[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTex[i], 0);
		mLevels = i + 1;
		w /= 2; h /= 2;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcess::run(GLuint hdrTexture, int screenW, int screenH, bool bloom, float bloomStrength, float exposure)
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glBindVertexArray(mVao);

	if (bloom && mLevels > 1)
	{
		// descente : image HDR vers des niveaux de plus en plus petits (seuil au premier)
		mDown.use();
		mDown.setUniformSampler("source", 0);
		mDown.setUniform("threshold", 1.2f);
		mDown.setUniform("knee", 0.8f);
		for (int i = 0; i < mLevels; i++)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, mFbo[i]);
			glViewport(0, 0, mW[i], mH[i]);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, i == 0 ? hdrTexture : mTex[i - 1]);
			int srcW = (i == 0) ? mSrcW : mW[i - 1];
			int srcH = (i == 0) ? mSrcH : mH[i - 1];
			mDown.setUniform("texelSize", glm::vec2(1.0f / srcW, 1.0f / srcH));
			mDown.setUniform("firstPass", (GLint)(i == 0));
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		// remontée : chaque niveau est ajouté au plus grand
		mUp.use();
		mUp.setUniformSampler("source", 0);
		mUp.setUniform("radius", 1.0f);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		for (int i = mLevels - 1; i > 0; i--)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, mFbo[i - 1]);
			glViewport(0, 0, mW[i - 1], mH[i - 1]);
			glBindTexture(GL_TEXTURE_2D, mTex[i]);
			mUp.setUniform("texelSize", glm::vec2(1.0f / mW[i], 1.0f / mH[i]));
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
		glDisable(GL_BLEND);
	}

	// image finale à l'écran
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screenW, screenH);
	mPost.use();
	mPost.setUniformSampler("hdrTex", 0);
	mPost.setUniformSampler("bloomTex", 1);
	mPost.setUniform("bloomOn", (GLint)(bloom && mLevels > 1));
	mPost.setUniform("bloomStrength", bloomStrength);
	mPost.setUniform("exposure", exposure);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mLevels > 0 ? mTex[0] : 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
}

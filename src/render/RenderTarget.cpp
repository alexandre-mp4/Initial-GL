#include "RenderTarget.h"
#include <iostream>

RenderTarget::RenderTarget()
	: mFbo(0), mTexture(0), mMsaaFbo(0), mMsaaColor(0), mDepth(0), mWidth(0), mHeight(0), mSamples(0)
{
}

RenderTarget::~RenderTarget()
{
	destroy();
}

void RenderTarget::destroy()
{
	if (mFbo)      glDeleteFramebuffers(1, &mFbo);
	if (mMsaaFbo)  glDeleteFramebuffers(1, &mMsaaFbo);
	if (mTexture)  glDeleteTextures(1, &mTexture);
	if (mMsaaColor) glDeleteRenderbuffers(1, &mMsaaColor);
	if (mDepth)    glDeleteRenderbuffers(1, &mDepth);
	mFbo = mMsaaFbo = mTexture = mMsaaColor = mDepth = 0;
}

bool RenderTarget::create(int width, int height, int samples)
{
	destroy();
	mWidth = width;
	mHeight = height;
	mSamples = samples > 1 ? samples : 0;

	glGenTextures(1, &mTexture);
	glBindTexture(GL_TEXTURE_2D, mTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenRenderbuffers(1, &mDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
	if (mSamples)
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, mSamples, GL_DEPTH_COMPONENT24, width, height);
	else
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

	glGenFramebuffers(1, &mFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mFbo);

	if (mSamples)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);

		glGenRenderbuffers(1, &mMsaaColor);
		glBindRenderbuffer(GL_RENDERBUFFER, mMsaaColor);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, mSamples, GL_RGBA16F, width, height);

		glGenFramebuffers(1, &mMsaaFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, mMsaaFbo);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mMsaaColor);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepth);
	}
	else
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepth);
	}

	bool ok = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	if (!ok)
		std::cerr << "RenderTarget : framebuffer incomplet (" << width << "x" << height << ", MSAA " << mSamples << ")" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return ok;
}

void RenderTarget::bind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, mSamples ? mMsaaFbo : mFbo);
	glViewport(0, 0, mWidth, mHeight);
}

void RenderTarget::resolve()
{
	if (!mSamples)
		return;

	// résolution du MSAA : moyenne des échantillons vers la texture
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mMsaaFbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFbo);
	glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

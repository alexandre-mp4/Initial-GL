// image HDR hors écran (FBO) en RGBA16F, avec MSAA optionnel
#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#define GLEW_STATIC
#include "GL/glew.h"

class RenderTarget
{
public:

	RenderTarget();
	~RenderTarget();
	RenderTarget(const RenderTarget&) = delete;
	RenderTarget& operator=(const RenderTarget&) = delete;

	// samples : 0 ou 1 = pas de MSAA
	bool create(int width, int height, int samples);
	void destroy();

	void bind();
	void resolve();

	GLuint getTexture() const { return mTexture; }
	int getWidth() const  { return mWidth; }
	int getHeight() const { return mHeight; }

private:

	GLuint mFbo, mTexture;
	GLuint mMsaaFbo, mMsaaColor;
	GLuint mDepth;
	int mWidth, mHeight, mSamples;
};
#endif

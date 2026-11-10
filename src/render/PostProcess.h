// bloom, tone mapping, gamma et vignettage
#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#define GLEW_STATIC
#include "GL/glew.h"
#include <vector>
#include "ShaderProgram.h"

class PostProcess
{
public:

	PostProcess();
	~PostProcess();
	PostProcess(const PostProcess&) = delete;
	PostProcess& operator=(const PostProcess&) = delete;

	bool init();

	void resize(int width, int height);

	void run(GLuint hdrTexture, int screenW, int screenH, bool bloom, float bloomStrength, float exposure);

private:

	void destroyLevels();

	static const int MAX_LEVELS = 6;

	ShaderProgram mDown, mUp, mPost;
	GLuint mVao, mDummy;
	int mLevels;
	GLuint mTex[MAX_LEVELS], mFbo[MAX_LEVELS];
	int mW[MAX_LEVELS], mH[MAX_LEVELS];
	int mSrcW, mSrcH;
};
#endif

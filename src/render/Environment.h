// ciel de nuit procédural dessiné une fois dans une texture panoramique
// elle sert au ciel, à l'éclairage ambiant (mipmaps floues) et aux reflets
#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <string>
#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"
#include "ShaderProgram.h"

class Environment
{
public:

	Environment();
	~Environment();
	Environment(const Environment&) = delete;
	Environment& operator=(const Environment&) = delete;

	bool create();

	void bind(GLuint unit);

	void drawSky(const glm::mat4& view, const glm::mat4& projection);

	const glm::vec3& getMoonDirection() const { return mMoonDir; }

	// azimut (0 = vers +Z) et hauteur de la lune
	void setMoon(float azimuth, float elevation);

	float getSkyIntensity() const { return 1.0f; }
	float getMaxLod() const { return mMaxLod; }

private:

	void bake();

	GLuint mTexture, mVao;
	ShaderProgram mSkyShader;
	glm::vec3 mMoonDir;
	float mMoonSize;
	float mMaxLod;
};
#endif

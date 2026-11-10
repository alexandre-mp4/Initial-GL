// carte d'ombres de la lune : la scène vue depuis la lune (profondeur seule) avec une projection orthographique qui suit la voiture
#ifndef SHADOWMAP_H
#define SHADOWMAP_H

#define GLEW_STATIC
#include "GL/glew.h"
#include "glm/glm.hpp"

class ShadowMap
{
public:

	ShadowMap();
	~ShadowMap();
	ShadowMap(const ShadowMap&) = delete;
	ShadowMap& operator=(const ShadowMap&) = delete;

	bool create(int size);
	void destroy();

	void begin(const glm::vec3& toLight, const glm::vec3& focus, float halfExtent);
	void end();

	void bindTexture(GLuint unit);
	const glm::mat4& getLightSpaceMatrix() const { return mLightSpace; }
	int getSize() const { return mSize; }

private:

	GLuint mFbo, mTexture;
	int mSize;
	glm::mat4 mLightSpace;
};
#endif

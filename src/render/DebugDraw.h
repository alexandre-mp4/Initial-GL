// lignes de debug (F3) : vert = suspension, jaune à rouge = force du pneu, cyan = vitesse
#ifndef DEBUGDRAW_H
#define DEBUGDRAW_H

#define GLEW_STATIC
#include "GL/glew.h"
#include <vector>
#include "glm/glm.hpp"
#include "ShaderProgram.h"

class DebugDraw
{
public:

	DebugDraw();
	~DebugDraw();
	DebugDraw(const DebugDraw&) = delete;
	DebugDraw& operator=(const DebugDraw&) = delete;

	bool init();
	void line(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color);
	void flush(const glm::mat4& viewProj);

private:

	ShaderProgram mShader;
	GLuint mVao, mVbo;
	std::vector<float> mVerts;
};
#endif

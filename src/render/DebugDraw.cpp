#include "DebugDraw.h"

DebugDraw::DebugDraw() : mVao(0), mVbo(0) {}

DebugDraw::~DebugDraw()
{
	if (mVbo) glDeleteBuffers(1, &mVbo);
	if (mVao) glDeleteVertexArrays(1, &mVao);
}

bool DebugDraw::init()
{
	glGenVertexArrays(1, &mVao);
	glGenBuffers(1, &mVbo);
	glBindVertexArray(mVao);
	glBindBuffer(GL_ARRAY_BUFFER, mVbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
	return mShader.loadShaders("shaders/debug.vert", "shaders/debug.frag");
}

void DebugDraw::line(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color)
{
	mVerts.insert(mVerts.end(), { a.x, a.y, a.z, color.x, color.y, color.z, b.x, b.y, b.z, color.x, color.y, color.z });
}

void DebugDraw::flush(const glm::mat4& viewProj)
{
	if (mVerts.empty()) return;
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	mShader.use();
	mShader.setUniform("viewProj", viewProj);
	glBindVertexArray(mVao);
	glBindBuffer(GL_ARRAY_BUFFER, mVbo);
	glBufferData(GL_ARRAY_BUFFER, mVerts.size() * sizeof(float), mVerts.data(), GL_STREAM_DRAW);
	glDrawArrays(GL_LINES, 0, (GLsizei)(mVerts.size() / 6));
	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
	mVerts.clear();
}

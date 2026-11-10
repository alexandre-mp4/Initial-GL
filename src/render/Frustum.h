// pyramide de vue de la caméra, plans extraits de projection * vue (Gribb-Hartmann)
#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "glm/glm.hpp"

struct Frustum
{
	glm::vec4 planes[6];

	void set(const glm::mat4& viewProjection)
	{
		// GLM est column-major : m[colonne][ligne]
		const glm::mat4& m = viewProjection;
		for (int i = 0; i < 3; i++)
		{
			planes[2 * i]     = glm::vec4(m[0][3] + m[0][i], m[1][3] + m[1][i], m[2][3] + m[2][i], m[3][3] + m[3][i]);
			planes[2 * i + 1] = glm::vec4(m[0][3] - m[0][i], m[1][3] - m[1][i], m[2][3] - m[2][i], m[3][3] - m[3][i]);
		}
		for (int i = 0; i < 6; i++)
			planes[i] /= glm::length(glm::vec3(planes[i]));
	}

	// vrai si la boîte touche le volume de vue
	bool intersectsBox(const glm::vec3& mn, const glm::vec3& mx) const
	{
		for (int i = 0; i < 6; i++)
		{
			glm::vec3 p(planes[i].x >= 0 ? mx.x : mn.x, planes[i].y >= 0 ? mx.y : mn.y, planes[i].z >= 0 ? mx.z : mn.z);
			if (glm::dot(glm::vec3(planes[i]), p) + planes[i].w < 0)
				return false;
		}
		return true;
	}

	bool intersectsSphere(const glm::vec3& c, float r) const
	{
		for (int i = 0; i < 6; i++)
			if (glm::dot(glm::vec3(planes[i]), c) + planes[i].w < -r)
				return false;
		return true;
	}
};
#endif

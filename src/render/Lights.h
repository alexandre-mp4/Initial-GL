// structures de lumières, miroir des structs GLSL de shaders/scene.frag
#ifndef LIGHTS_H
#define LIGHTS_H

#include <vector>
#include "glm/glm.hpp"
#include "ShaderProgram.h"

// doit rester identique aux #define de shaders/scene.frag
const int MAX_POINT_LIGHTS = 32;
const int MAX_SPOT_LIGHTS = 8;

struct DirLight
{
	glm::vec3 direction;
	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;
};

struct PointLight
{
	glm::vec3 position;
	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;
	float constant;
	float linear;
	float exponent;
};

struct SpotLight
{
	glm::vec3 position;
	glm::vec3 direction;
	float cosInnerCone;
	float cosOuterCone;
	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;
	float constant;
	float linear;
	float exponent;
};

void sendLights(ShaderProgram& shader,
				const DirLight& dirLight,
				const std::vector<PointLight>& pointLights,
				const std::vector<SpotLight>& spotLights);

#endif

#include "Lights.h"
#include <string>
#include <algorithm>
#include <iostream>

// les éléments de tableau de struct GLSL sont des uniforms au nom composé ("pointLights[3].position")
void sendLights(ShaderProgram& shader,
				const DirLight& dirLight,
				const std::vector<PointLight>& pointLights,
				const std::vector<SpotLight>& spotLights)
{
	shader.setUniform("dirLight.direction", dirLight.direction);
	shader.setUniform("dirLight.ambient", dirLight.ambient);
	shader.setUniform("dirLight.diffuse", dirLight.diffuse);
	shader.setUniform("dirLight.specular", dirLight.specular);

	int nbPointLights = std::min((int)pointLights.size(), MAX_POINT_LIGHTS);
	if ((int)pointLights.size() > MAX_POINT_LIGHTS)
		std::cerr << "Trop de point lights : " << pointLights.size() << " > " << MAX_POINT_LIGHTS << std::endl;

	shader.setUniform("nbPointLights", nbPointLights);
	for (int i = 0; i < nbPointLights; i++)
	{
		const PointLight& light = pointLights[i];
		std::string base = "pointLights[" + std::to_string(i) + "].";

		shader.setUniform((base + "position").c_str(), light.position);
		shader.setUniform((base + "ambient").c_str(), light.ambient);
		shader.setUniform((base + "diffuse").c_str(), light.diffuse);
		shader.setUniform((base + "specular").c_str(), light.specular);
		shader.setUniform((base + "constant").c_str(), light.constant);
		shader.setUniform((base + "linear").c_str(), light.linear);
		shader.setUniform((base + "exponent").c_str(), light.exponent);
	}

	int nbSpotLights = std::min((int)spotLights.size(), MAX_SPOT_LIGHTS);
	if ((int)spotLights.size() > MAX_SPOT_LIGHTS)
		std::cerr << "Trop de spots : " << spotLights.size() << " > " << MAX_SPOT_LIGHTS << std::endl;

	shader.setUniform("nbSpotLights", nbSpotLights);
	for (int i = 0; i < nbSpotLights; i++)
	{
		const SpotLight& light = spotLights[i];
		std::string base = "spotLights[" + std::to_string(i) + "].";

		shader.setUniform((base + "position").c_str(), light.position);
		shader.setUniform((base + "direction").c_str(), light.direction);
		shader.setUniform((base + "cosInnerCone").c_str(), light.cosInnerCone);
		shader.setUniform((base + "cosOuterCone").c_str(), light.cosOuterCone);
		shader.setUniform((base + "ambient").c_str(), light.ambient);
		shader.setUniform((base + "diffuse").c_str(), light.diffuse);
		shader.setUniform((base + "specular").c_str(), light.specular);
		shader.setUniform((base + "constant").c_str(), light.constant);
		shader.setUniform((base + "linear").c_str(), light.linear);
		shader.setUniform((base + "exponent").c_str(), light.exponent);
	}
}

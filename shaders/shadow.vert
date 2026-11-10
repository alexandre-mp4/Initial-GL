// passe d'ombres : position vue depuis la lune ; les arbres gardent vent et UV pour que l'ombre suive leur silhouette
#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 2) in vec2 texCoord;
layout (location = 4) in vec4 instPosYaw;
layout (location = 5) in vec4 instParams;

uniform mat4 model;
uniform mat4 lightSpace;
uniform bool instanced;
uniform float time;
uniform float windStrength;
uniform float treeHeight;

out vec2 uv;

void main()
{
	uv = texCoord;
	vec3 worldPos;
	if (instanced)
	{
		float c = cos(instPosYaw.w), s = sin(instPosYaw.w);
		mat3 R = mat3(c, 0.0, -s,  0.0, 1.0, 0.0,  s, 0.0, c);
		vec3 local = pos * instParams.x;
		float h = (treeHeight > 0.0) ? clamp(local.y / (treeHeight * instParams.x), 0.0, 1.0) : 0.0;
		float bend = h * h * windStrength;
		local.x += bend * sin(time * 1.35 + instParams.y);
		local.z += bend * 0.7 * cos(time * 1.10 + instParams.y * 1.7);
		worldPos = instPosYaw.xyz + R * local;
	}
	else
		worldPos = vec3(model * vec4(pos, 1.0));
	gl_Position = lightSpace * vec4(worldPos, 1.0);
}

// position monde et repère tangent (TBN), avec instanciation des arbres (32 octets par instance) et vent
#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;
layout (location = 3) in vec4 tangent;
layout (location = 4) in vec4 instPosYaw;
layout (location = 5) in vec4 instParams;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

uniform bool instanced;
uniform float time;
uniform float windStrength;
uniform float treeHeight;

out vec3 FragPos;
out vec2 TexCoord;
out vec3 Normal;
out vec3 Tangent;
out vec3 Bitangent;
out float Tint;

void main()
{
	vec3 N, T;
	vec3 worldPos;

	if (instanced)
	{
		// rotation autour de Y puis échelle et translation
		float c = cos(instPosYaw.w), s = sin(instPosYaw.w);
		mat3 R = mat3(c, 0.0, -s,  0.0, 1.0, 0.0,  s, 0.0, c);
		vec3 local = pos * instParams.x;

		// vent : déplacement horizontal proportionnel à (hauteur relative)^2, décalé par la phase de chaque arbre
		float h = (treeHeight > 0.0) ? clamp(local.y / (treeHeight * instParams.x), 0.0, 1.0) : 0.0;
		float bend = h * h * windStrength;
		local.x += bend * sin(time * 1.35 + instParams.y);
		local.z += bend * 0.7 * cos(time * 1.10 + instParams.y * 1.7);

		worldPos = instPosYaw.xyz + R * local;
		N = normalize(R * normal);
		T = R * tangent.xyz;
		Tint = instParams.z;
	}
	else
	{
		worldPos = vec3(model * vec4(pos, 1.0));

		// la normale utilise la matrice normale, la tangente la matrice du modèle
		N = normalize(normalMatrix * normal);
		T = mat3(model) * tangent.xyz;
		Tint = 1.0;
	}

	FragPos = worldPos;
	TexCoord = texCoord;

	// sans tangente (terrain), on évite normalize(0) ; T est ré-orthogonalisé par rapport à N
	T = (dot(T, T) > 1e-8) ? normalize(T) : vec3(1.0, 0.0, 0.0);
	T = normalize(T - dot(T, N) * N + 1e-6);
	vec3 B = cross(N, T) * tangent.w;

	Normal = N;
	Tangent = T;
	Bitangent = B;

	gl_Position = projection * view * vec4(worldPos, 1.0);
}

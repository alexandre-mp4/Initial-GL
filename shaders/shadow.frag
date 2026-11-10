#version 330 core

in vec2 uv;
uniform sampler2D albedoMap;
uniform float alphaCutoff;

void main()
{
	if (alphaCutoff >= 0.0 && texture(albedoMap, uv).a < alphaCutoff)
		discard;
}

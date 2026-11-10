// bloom, agrandissement avec un filtre tente 3x3, ajouté au niveau plus grand (blending additif)
#version 330 core

in vec2 uv;
out vec4 frag_color;

uniform sampler2D source;
uniform vec2 texelSize;
uniform float radius;

void main()
{
	vec2 t = texelSize * radius;
	vec3 s  = texture(source, uv + t * vec2(-1, -1)).rgb + texture(source, uv + t * vec2( 1, -1)).rgb
			+ texture(source, uv + t * vec2(-1,  1)).rgb + texture(source, uv + t * vec2( 1,  1)).rgb;
	vec3 e  = texture(source, uv + t * vec2( 0, -1)).rgb + texture(source, uv + t * vec2( 0,  1)).rgb
			+ texture(source, uv + t * vec2(-1,  0)).rgb + texture(source, uv + t * vec2( 1,  0)).rgb;
	vec3 c  = texture(source, uv).rgb;
	frag_color = vec4((s + 2.0 * e + 4.0 * c) * (1.0 / 16.0), 1.0);
}

// passage final : bloom, exposition, tone mapping ACES, gamma sRGB, vignettage et dithering
#version 330 core

in vec2 uv;
out vec4 frag_color;

uniform sampler2D hdrTex;
uniform sampler2D bloomTex;
uniform bool bloomOn;
uniform float bloomStrength;
uniform float exposure;

// approximation d'ACES (Narkowicz)
vec3 aces(vec3 x)
{
	return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 toSRGB(vec3 c)
{
	return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

void main()
{
	vec3 color = texture(hdrTex, uv).rgb;
	if (bloomOn)
		color += texture(bloomTex, uv).rgb * bloomStrength;

	color = aces(color * exposure);

	// vignettage
	vec2 q = uv - 0.5;
	color *= 1.0 - 0.35 * dot(q, q);

	color = toSRGB(color);

	// dithering triangulaire de ±1 niveau 8 bits contre les bandes dans les sombres
	float n = hash12(gl_FragCoord.xy) + hash12(gl_FragCoord.xy + 17.0) - 1.0;
	color += n / 255.0;

	frag_color = vec4(color, 1.0);
}

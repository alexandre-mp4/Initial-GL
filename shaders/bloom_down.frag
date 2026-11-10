// bloom, réduction de moitié avec un filtre à 13 échantillons ; le premier niveau applique un seuil
#version 330 core

in vec2 uv;
out vec4 frag_color;

uniform sampler2D source;
uniform vec2 texelSize;
uniform bool firstPass;
uniform float threshold;
uniform float knee;

float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

// poids de Karis : évite qu'un pixel isolé très lumineux scintille
vec3 karis(vec3 c) { return c / (1.0 + luma(c)); }

vec3 sampleSrc(vec2 p) { return texture(source, p).rgb; }

void main()
{
	vec2 t = texelSize;
	vec3 a = sampleSrc(uv + t * vec2(-2, -2)), b = sampleSrc(uv + t * vec2( 0, -2)), c = sampleSrc(uv + t * vec2( 2, -2));
	vec3 d = sampleSrc(uv + t * vec2(-2,  0)), e = sampleSrc(uv),                    f = sampleSrc(uv + t * vec2( 2,  0));
	vec3 g = sampleSrc(uv + t * vec2(-2,  2)), h = sampleSrc(uv + t * vec2( 0,  2)), i = sampleSrc(uv + t * vec2( 2,  2));
	vec3 j = sampleSrc(uv + t * vec2(-1, -1)), k = sampleSrc(uv + t * vec2( 1, -1));
	vec3 l = sampleSrc(uv + t * vec2(-1,  1)), m = sampleSrc(uv + t * vec2( 1,  1));

	vec3 result;
	if (firstPass)
	{
		vec3 g0 = (a + b + d + e) * 0.25, g1 = (b + c + e + f) * 0.25, g2 = (d + e + g + h) * 0.25, g3 = (e + f + h + i) * 0.25, g4 = (j + k + l + m) * 0.25;
		float w0 = 1.0 / (1.0 + luma(g0)), w1 = 1.0 / (1.0 + luma(g1)), w2 = 1.0 / (1.0 + luma(g2)), w3 = 1.0 / (1.0 + luma(g3)), w4 = 1.0 / (1.0 + luma(g4));

		result = (g0 * w0 + g1 * w1 + g2 * w2 + g3 * w3) * 0.125 + g4 * w4 * 0.5;
		result /= (w0 + w1 + w2 + w3) * 0.125 + w4 * 0.5;

		// seuil doux avec transition quadratique
		float br = max(result.r, max(result.g, result.b));
		float soft = clamp(br - threshold + knee, 0.0, 2.0 * knee);
		soft = soft * soft / (4.0 * knee + 1e-5);
		result *= max(soft, br - threshold) / max(br, 1e-5);
	}
	else
	{
		result = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
	}

	frag_color = vec4(result, 1.0);
}

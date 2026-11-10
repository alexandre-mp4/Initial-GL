// ciel de nuit procédural, utilisé de deux façons :
// bakeMode : dégradé dessiné dans la texture panoramique (éclairage ambiant, reflets, brouillard)
// sinon : ciel à l'écran avec étoiles et disque de lune
#version 330 core

in vec2 uv;
out vec4 frag_color;

uniform mat4 invViewProj;
uniform bool bakeMode;
uniform vec3 moonDir;
uniform float moonSize;

const float PI = 3.14159265;

// dégradé du ciel avec une faible lueur orangée vers +z (ville dans la vallée)
vec3 skyGradient(vec3 d)
{
	const vec3 ZENITH  = vec3(0.004, 0.010, 0.034);
	const vec3 HORIZON = vec3(0.026, 0.040, 0.088);
	const vec3 GROUND  = vec3(0.005, 0.007, 0.015);
	float h = d.y;
	vec3 sky = mix(HORIZON, ZENITH, pow(clamp(h, 0.0, 1.0), 0.45));
	sky = mix(sky, GROUND, smoothstep(0.0, -0.30, h));

	float town = exp(-pow(h / 0.10, 2.0)) * (0.5 + 0.5 * dot(normalize(vec3(d.x, 0.0, d.z) + 1e-5), vec3(0.0, 0.0, 1.0)));
	sky += vec3(0.30, 0.15, 0.05) * 0.06 * town;

	float mc = max(dot(d, moonDir), 0.0);
	sky += vec3(0.40, 0.52, 0.85) * (0.10 * pow(mc, 30.0) + 0.035 * pow(mc, 5.0));
	return sky;
}

float hash13(vec3 p)
{
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}

// étoiles : l'espace de directions est découpé en cellules contenant au plus une étoile (hash)
float stars(vec3 d)
{
	vec3 p = d * 220.0;
	vec3 cell = floor(p);
	float h = hash13(cell);
	if (h < 0.965) return 0.0;
	vec3 center = cell + 0.3 + 0.4 * vec3(hash13(cell + 7.1), hash13(cell + 13.7), hash13(cell + 29.3));
	float dist = length(p - center);
	float size = 0.10 + 0.12 * hash13(cell + 3.3);
	float brightness = 0.3 + 2.2 * pow(hash13(cell + 91.7), 3.0);
	return smoothstep(size, 0.0, dist) * brightness;
}

void main()
{
	if (bakeMode)
	{
		// uv vers direction (inverse de dirToEquirect dans scene.frag)
		float lon = (uv.x - 0.5) * 2.0 * PI;
		float theta = uv.y * PI;
		vec3 d = vec3(sin(theta) * sin(lon), cos(theta), -sin(theta) * cos(lon));
		frag_color = vec4(skyGradient(d), 1.0);
		return;
	}

	vec4 ndc = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
	vec4 w = invViewProj * ndc;
	vec3 dir = normalize(w.xyz / w.w);

	vec3 color = skyGradient(dir);

	float horizonFade = smoothstep(0.02, 0.35, dir.y);
	float moonCos = dot(dir, moonDir);
	float glare = 1.0 - smoothstep(0.85, 0.995, moonCos);
	color += vec3(0.8, 0.9, 1.0) * stars(dir) * horizonFade * glare;

	// lune : valeurs HDR très élevées, le bloom les étale en halo
	float angle = acos(clamp(moonCos, -1.0, 1.0));
	float disc = 1.0 - smoothstep(moonSize * 0.92, moonSize, angle);
	float maria = 0.82 + 0.18 * hash13(floor(dir * 90.0));
	color += vec3(1.0, 0.97, 0.9) * 22.0 * disc * maria;

	frag_color = vec4(color, 1.0);
}

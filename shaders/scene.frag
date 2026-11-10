// éclairage Blinn-Phong multi-lumières (une fonction par type, contributions additionnées)
// matériaux de type PBR : normal map, roughness, metallic et AO lus dans des textures
#version 330 core

struct Material
{
	vec3 ambient;
	sampler2D albedoMap;
	sampler2D normalMap;
	sampler2D ormMap;
	bool hasNormalMap;
	bool hasOrmMap;
	vec4 baseColor;
	float roughness;
	float metallic;
	float specularF0;
	vec3 emissive;
	float alphaCutoff;
	vec2 uvScale;
	sampler2D overlayMap;
	bool hasOverlay;
	float overlayPeriod;
};

struct DirLight
{
	vec3 direction;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

struct PointLight
{
	vec3 position;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;

	float constant;
	float linear;
	float exponent;
};

struct SpotLight
{
	vec3 position;
	vec3 direction;
	float cosInnerCone;
	float cosOuterCone;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;

	float constant;
	float linear;
	float exponent;
};

in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;
in vec3 Bitangent;
in float Tint;

// GLSL 3.3 exige une taille connue pour les tableaux d'uniforms, doit rester identique à Lights.h
#define MAX_POINT_LIGHTS 32
#define MAX_SPOT_LIGHTS 8

uniform DirLight dirLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform int nbPointLights;
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];
uniform int nbSpotLights;

uniform Material material;
uniform vec3 viewPos;

uniform sampler2D envMap;
uniform float skyIntensity;
uniform float envMaxLod;
uniform sampler2DShadow shadowMap;
uniform mat4 lightSpace;
uniform bool shadowsOn;
uniform float fogDensity;

out vec4 frag_color;

// données du point éclairé, calculées une fois par pixel et réutilisées par chaque lumière
struct Surface
{
	vec3 diffuseColor;
	vec3 F0;
	float shininess;
	float ao;
	float roughness;
};

const float PI = 3.14159265;

// direction vers coordonnées de l'image équirectangulaire
vec2 dirToEquirect(vec3 d)
{
	return vec2(atan(d.x, -d.z) / (2.0 * PI) + 0.5, acos(clamp(d.y, -1.0, 1.0)) / PI);
}

// ciel flouté par le niveau de mipmap ; textureLod évite un mipmap faux à la couture de l'image
vec3 sampleSky(vec3 dir, float lod)
{
	return textureLod(envMap, dirToEquirect(dir), lod).rgb * skyIntensity;
}

// part de lumière de la lune qui arrive (1 = éclairé, 0 = ombre)
float shadowFactor(vec3 fragPos, vec3 N, vec3 L)
{
	if (!shadowsOn)
		return 1.0;

	// normal offset : le point est décalé le long de la normale pour éviter l'auto-ombrage
	float cosTheta = clamp(dot(N, L), 0.0, 1.0);
	vec3 p = fragPos + N * (0.03 + 0.12 * (1.0 - cosTheta));

	vec4 ls = lightSpace * vec4(p, 1.0);
	vec3 c = ls.xyz / ls.w * 0.5 + 0.5;
	if (c.z > 1.0 || c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0)
		return 1.0;

	// PCF 3x3 : 9 comparaisons déjà filtrées 2x2 par le GPU
	float bias = 0.0006;
	vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
	float lit = 0.0;
	for (int x = -1; x <= 1; x++)
		for (int y = -1; y <= 1; y++)
			lit += texture(shadowMap, vec3(c.xy + vec2(x, y) * texel, c.z - bias));
	lit /= 9.0;

	// fondu aux bords de la zone d'ombres
	vec2 edge = abs(c.xy * 2.0 - 1.0);
	float fade = 1.0 - smoothstep(0.8, 1.0, max(edge.x, edge.y));
	return mix(1.0, lit, fade);
}

// terrain : mélange de matériaux selon la pente, une carte de contrôle et du bruit, sans coordonnées de texture (projection depuis l'espace monde)
// sol : projection verticale ; roche : projection triplanaire pour éviter l'étirement sur les parois
uniform bool terrainMode;
uniform bool darkBackfaces;
uniform sampler2D tControl;
uniform sampler2D tForestA, tForestN, tGravelA, tGravelN, tMeadowA, tRockA, tRockN, tRockO;
uniform float tWorldSize;

float hash21(vec2 p)
{
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

// bruit de valeurs lissé, casse la régularité des transitions et des textures
float vnoise(vec2 p)
{
	vec2 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x), mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
}

void terrainSurface(vec3 N0, out vec3 albedo, out vec3 normal, out float roughness, out float ao)
{
	vec4 ctl = texture(tControl, (FragPos.xz + 0.5 * tWorldSize) / tWorldSize);
	vec2 p = FragPos.xz;

	// poids de la roche : pente, carte de contrôle et bruit
	float rockSlope = 1.0 - smoothstep(0.58, 0.82, N0.y);
	float wRock = clamp(max(ctl.b * 0.9, rockSlope), 0.0, 1.0);
	wRock = smoothstep(0.35, 0.65, wRock + (vnoise(p * 0.30) - 0.5) * 0.5);
	float wGravel = clamp(ctl.r * (1.0 - wRock), 0.0, 1.0);

	vec2 uvF = vec2(p.x, -p.y);
	vec3 forestA = mix(texture(tForestA, uvF / 3.6).rgb, texture(tForestA, uvF / 19.0 + 0.37).rgb, 0.45);
	vec3 gravelA = texture(tGravelA, uvF / 2.2).rgb;
	vec3 meadowA = texture(tMeadowA, uvF / 4.2).rgb;
	vec3 ground = mix(meadowA, forestA, ctl.g);
	ground = mix(ground, ground * vec3(0.78, 1.06, 0.72), ctl.a * 0.5);
	ground = mix(ground, gravelA, wGravel);
	vec3 nF = texture(tForestN, uvF / 3.6).xyz * 2.0 - 1.0;
	vec3 nG = texture(tGravelN, uvF / 2.2).xyz * 2.0 - 1.0;
	vec3 nGround = mix(nF, nG, wGravel);

	// udn : la pente de la normal map est ajoutée à la normale géométrique
	vec3 groundNormal = normalize(N0 + vec3(nGround.x, 0.0, -nGround.y));
	float groundRough = mix(0.93, 0.85, wGravel);

	albedo = ground;
	normal = groundNormal;
	roughness = groundRough;
	ao = 1.0;

	// roche triplanaire : 3 lectures par texture, seulement où il y a de la roche
	if (wRock > 0.01)
	{
		vec3 w = pow(abs(N0), vec3(4.0));
		w /= (w.x + w.y + w.z);
		const float RS = 1.0 / 5.5;
		vec2 uvX = vec2(-FragPos.z, FragPos.y) * RS, uvY = vec2(FragPos.x, -FragPos.z) * RS, uvZ = vec2(FragPos.x, FragPos.y) * RS;
		vec3 rock = texture(tRockA, uvX).rgb * w.x + texture(tRockA, uvY).rgb * w.y + texture(tRockA, uvZ).rgb * w.z;
		vec3 orm = texture(tRockO, uvX).rgb * w.x + texture(tRockO, uvY).rgb * w.y + texture(tRockO, uvZ).rgb * w.z;

		// whiteout blend : mélange des normales dans l'espace monde avec les poids de la projection
		vec3 tx = texture(tRockN, uvX).xyz * 2.0 - 1.0;
		vec3 ty = texture(tRockN, uvY).xyz * 2.0 - 1.0;
		vec3 tz = texture(tRockN, uvZ).xyz * 2.0 - 1.0;
		tx = vec3(tx.xy + N0.zy, abs(tx.z) * N0.x);
		ty = vec3(ty.xy + N0.xz, abs(ty.z) * N0.y);
		tz = vec3(tz.xy + N0.xy, abs(tz.z) * N0.z);
		vec3 rockNormal = normalize(tx.zyx * w.x + ty.xzy * w.y + tz.xyz * w.z);

		rock *= 0.8 + 0.4 * vnoise(p * 0.07 + FragPos.y * 0.05);

		albedo = mix(albedo, rock, wRock);
		normal = normalize(mix(normal, rockNormal, wRock));
		roughness = mix(roughness, clamp(orm.g * 1.0, 0.3, 1.0), wRock);
		ao = mix(1.0, orm.r, wRock);
	}

	albedo *= 0.82 + 0.36 * vnoise(p * 0.045);
}

vec3 calcBlinnPhong(vec3 lightDir, vec3 lightDiffuse, vec3 lightSpecular, vec3 normal, vec3 viewDir, Surface s);
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir, Surface s, float shadow);
vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, Surface s);
vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, Surface s);

void main()
{
	vec2 uv = TexCoord * material.uvScale;
	vec3 albedo, normal;
	float roughness, metallic, ao;
	float paint = 0.0;
	float outAlpha = 1.0;

	if (terrainMode)
	{
		terrainSurface(normalize(Normal), albedo, normal, roughness, ao);
		metallic = 0.0;
	}
	else
	{
		vec4 albedoSample = texture(material.albedoMap, uv) * material.baseColor;
		if (albedoSample.a < material.alphaCutoff)
			discard;
		albedo = albedoSample.rgb * Tint;
		outAlpha = albedoSample.a;

		// vu de l'intérieur, le dessous du toit montrait l'autocollant extérieur : garnissage gris sombre à la place
		if (darkBackfaces && !gl_FrontFacing && albedoSample.a > 0.99)
			albedo = vec3(0.035, 0.035, 0.04);

		// calque peint (lignes de la route) par-dessus l'asphalte
		if (material.hasOverlay)
		{
			vec4 o = texture(material.overlayMap, vec2(TexCoord.x, TexCoord.y / material.overlayPeriod));
			albedo = mix(albedo, o.rgb, o.a);
			paint = o.a;
		}

		// normale du mesh, ou normal map ([0,1] vers [-1,1]) passée dans le repère monde avec TBN = [T B N]
		normal = normalize(Normal) * (gl_FrontFacing ? 1.0 : -1.0);
		if (material.hasNormalMap)
		{
			vec3 n = texture(material.normalMap, uv).xyz * 2.0 - 1.0;
			mat3 TBN = mat3(normalize(Tangent), normalize(Bitangent), normal);
			normal = normalize(TBN * n);
		}

		roughness = material.roughness;
		metallic = material.metallic;
		ao = 1.0;
		if (material.hasOrmMap)
		{
			vec3 orm = texture(material.ormMap, uv).rgb;
			ao = orm.r;
			roughness *= orm.g;
			metallic *= orm.b;
		}
		roughness = mix(roughness, 0.55, paint);
	}
	roughness = clamp(roughness, 0.05, 1.0);

	// peinture de voiture : métal sous vernis, on plafonne le métal pour garder la couleur sous peu de lumière
	metallic = min(metallic, 0.7);

	// du matériau PBR aux paramètres de Blinn-Phong : un métal n'a pas de diffuse et son reflet est teinté par sa couleur
	// exposant de Blinn-Phong n = 2 / r^4 - 2
	Surface s;
	s.diffuseColor = albedo * (1.0 - metallic);
	s.F0 = mix(vec3(material.specularF0), albedo, metallic);
	s.shininess = clamp(2.0 / (roughness * roughness * roughness * roughness) - 2.0, 1.0, 4096.0);
	s.ao = ao;
	s.roughness = roughness;

	vec3 viewDir = normalize(viewPos - FragPos);

	// lune avec son ombre
	vec3 moonL = normalize(-dirLight.direction);
	float shadow = shadowFactor(FragPos, normalize(Normal), moonL);

	vec3 outColor = calcDirLight(dirLight, normal, viewDir, s, shadow);

	for (int i = 0; i < min(nbPointLights, MAX_POINT_LIGHTS); i++)
		outColor += calcPointLight(pointLights[i], normal, FragPos, viewDir, s);

	for (int i = 0; i < min(nbSpotLights, MAX_SPOT_LIGHTS); i++)
		outColor += calcSpotLight(spotLights[i], normal, FragPos, viewDir, s);

	// lumière du ciel : diffuse (mipmap très floue dans la direction de la normale) et reflet (flou selon la rugosité, Fresnel, atténué par l'AO)
	vec3 skyDiffuse = sampleSky(normal, envMaxLod - 1.0);

	vec3 R = reflect(-viewDir, normal);
	float NdotV = max(dot(normal, viewDir), 0.0);
	vec3 Fenv = s.F0 + (max(vec3(1.0 - s.roughness), s.F0) - s.F0) * pow(1.0 - NdotV, 5.0);
	vec3 skySpec = sampleSky(R, s.roughness * (envMaxLod - 3.0));
	outColor += (skyDiffuse * s.diffuseColor + skySpec * Fenv) * s.ao * material.ambient;

	// brouillard exp(-(k*d)^2) vers la couleur du ciel
	float dist = length(FragPos - viewPos);
	float fog = exp(-pow(fogDensity * dist, 2.0));
	vec3 fogColor = sampleSky(normalize(FragPos - viewPos), envMaxLod - 4.0);
	outColor = mix(fogColor, outColor, fog);

	frag_color = vec4(outColor + material.emissive, outAlpha);
}

// Blinn-Phong pour une lumière, commun aux 3 types ; lightDir va du fragment vers la lumière
vec3 calcBlinnPhong(vec3 lightDir, vec3 lightDiffuse, vec3 lightSpecular, vec3 normal, vec3 viewDir, Surface s)
{
	float NdotL = max(dot(normal, lightDir), 0.0);
	vec3 diffuse = lightDiffuse * NdotL * s.diffuseColor;

	vec3 halfDir = normalize(lightDir + viewDir);
	float NdotH = max(dot(normal, halfDir), 0.0);
	float VdotH = max(dot(viewDir, halfDir), 0.0);
	vec3 fresnel = s.F0 + (1.0 - s.F0) * pow(1.0 - VdotH, 5.0);
	vec3 specular = lightSpecular * fresnel * pow(NdotH, s.shininess) * (NdotL > 0.0 ? 1.0 : 0.0);

	return diffuse + specular;
}

// lumière directionnelle : pas de position ni d'atténuation
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir, Surface s, float shadow)
{
	vec3 lightDir = normalize(-light.direction);

	// seule la lumière directe de la lune subit l'ombre
	vec3 ambient = light.ambient * material.ambient * s.diffuseColor * s.ao;
	return ambient + shadow * calcBlinnPhong(lightDir, light.diffuse, light.specular, normal, viewDir, s);
}

// point light : atténuation 1 / (Kc + Kl*d + Kq*d^2)
vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, Surface s)
{
	vec3 toLight = light.position - fragPos;
	vec3 lightDir = normalize(toLight);

	float d = length(toLight);
	float attenuation = 1.0f / (light.constant + light.linear * d + light.exponent * (d * d));

	vec3 ambient = light.ambient * material.ambient * s.diffuseColor * s.ao;
	vec3 color = ambient + calcBlinnPhong(lightDir, light.diffuse, light.specular, normal, viewDir, s);

	return color * attenuation;
}

// spot : point light limitée à un cône
vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, Surface s)
{
	vec3 toLight = light.position - fragPos;
	vec3 lightDir = normalize(toLight);
	vec3 spotDir = normalize(light.direction);

	// comparaison de cosinus pour éviter un acos, smoothstep entre le cône intérieur et extérieur
	float cosDir = dot(-lightDir, spotDir);
	float spotIntensity = smoothstep(light.cosOuterCone, light.cosInnerCone, cosDir);

	float d = length(toLight);
	float attenuation = 1.0f / (light.constant + light.linear * d + light.exponent * (d * d));

	vec3 ambient = light.ambient * material.ambient * s.diffuseColor * s.ao;
	vec3 color = ambient + calcBlinnPhong(lightDir, light.diffuse, light.specular, normal, viewDir, s) * spotIntensity;

	return color * attenuation;
}

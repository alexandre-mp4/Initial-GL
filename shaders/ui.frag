// l'atlas de police a un seul canal ; un carré blanc de l'atlas sert aux formes pleines
#version 330 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D atlas;
out vec4 frag_color;
void main()
{
	float a = texture(atlas, vUV).r;
	frag_color = vec4(vColor.rgb, vColor.a * a);
}

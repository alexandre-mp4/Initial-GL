// lignes de debug : position monde et couleur par sommet
#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 color;
uniform mat4 viewProj;
out vec3 vColor;
void main()
{
	vColor = color;
	gl_Position = viewProj * vec4(pos, 1.0);
}

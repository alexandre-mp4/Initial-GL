// triangle plein écran sans buffer : gl_VertexID 0, 1, 2 donne (-1,-1), (3,-1), (-1,3)
#version 330 core

out vec2 uv;

void main()
{
	vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
	uv = p;
	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}

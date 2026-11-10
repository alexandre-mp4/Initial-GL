#version 330 core
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec4 color;
uniform mat4 ortho;
out vec2 vUV;
out vec4 vColor;
void main()
{
	vUV = uv;
	vColor = color;
	gl_Position = ortho * vec4(pos, 0.0, 1.0);
}

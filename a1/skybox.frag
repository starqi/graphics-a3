#version 430 core

in vec3 direction;
out vec4 color;

uniform samplerCube skyTex;

void main() {
	color = texture(skyTex, direction);
}
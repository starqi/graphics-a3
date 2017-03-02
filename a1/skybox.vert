#version 430 core

layout (location = 0) in vec3 position;
out vec3 direction;

uniform mat4 proj, view;

void main() {
	gl_Position = proj * view * vec4(position, 1.0f);
	direction = position;
}
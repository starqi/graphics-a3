#version 430 core

uniform mat4 model, normalModel, view, proj, shadowMatrix;

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;

out vec3 vNormal;
out vec3 vPosition;
out vec3 vColor;
out vec4 vShadowC;

void main() {
	gl_Position = proj * view * model * vec4(position, 1.0f);
	vPosition = vec3(model * vec4(position, 1.0f));
	vNormal = vec3(normalModel * vec4(normal, 1.0f));
	vShadowC = shadowMatrix * model * vec4(position, 1.0f);
	vColor = color;
}
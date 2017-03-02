#version 430 core

struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;
}; 

in vec3 vPosition, vNormal, vColor;
out vec4 c;

uniform uint isColor;
uniform vec3 viewerPos;
uniform Material material;

void main()
{          
	//float a = -vPosition.z / 500.0f;
	//c = vec4(a, a, a, 1.0f);
	//gl_FragDepth = a;
} 
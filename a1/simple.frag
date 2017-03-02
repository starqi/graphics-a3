#version 430 core

struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;
}; 

struct PointLight {
    vec3 position;
	vec3 ambient, diffuse, specular;
};

in vec3 gPosition, gNormal, gColor;
in vec4 gShadowC;
flat in int gIsEdge;
out vec4 color;

uniform uint isColor, nonsenseOff;
uniform PointLight light;
uniform vec3 viewerPos;
uniform Material material;
uniform vec3 vegetaLoc, gokuLoc;
uniform sampler2D shadowMap;

void main() {
	if (gIsEdge == 1) {
		if (nonsenseOff == 1) return;
		color = texture(material.texture_diffuse1, vec2(gColor));
		color.x *= 0.21f;
		color.y *= 0.21f;
		color.z *= 0.21f;
		return;
	}

	vec3 norm = normalize(gNormal);
	vec3 lightDir = normalize(light.position - gPosition);
	vec3 viewerDir = normalize(viewerPos - gPosition);
	vec3 reflectDir = reflect(-lightDir, norm);
	vec3 refractDir = refract(-lightDir, norm, 1.33); // Water index
	float diffuse = max(dot(norm, lightDir), 0.0f);
	float specular = pow(max(dot(viewerDir, reflectDir), 0.0f), 5);
	float distance = length(light.position - gPosition);
	vec3 ambientC, diffuseC, specularC;
	if (isColor > 0) {
		ambientC = gColor;
		diffuseC = gColor;
		specularC = gColor;
	} else {
		ambientC = vec3(texture(material.texture_diffuse1, vec2(gColor)));
		diffuseC = vec3(texture(material.texture_diffuse1, vec2(gColor)));
		specularC = vec3(texture(material.texture_diffuse1, vec2(gColor)));
	}
	ambientC *= light.ambient;
	diffuseC *= light.diffuse * diffuse;
	specularC *= light.specular * specular;
	vec3 ndc = gShadowC.xyz / gShadowC.w;
	vec3 bndc = ndc / 2.0f + 0.5f;
	float s = texture(shadowMap, bndc.xy).r;
	float t = bndc.z > 1.0f ? 1.0f : bndc.z;
	float c = t - s > (1.0 / 100000.0) ? 0.0f : 1.0f;
	color = vec4(ambientC + diffuseC + specularC, 1.0f);
	
	float factor = 1.02f;
	float gokuContrib = 1.0f / pow(factor, length(gPosition - gokuLoc));
	float vegContrib = 1.0f / pow(factor, length(gPosition - vegetaLoc));
	float total = min(gokuContrib + vegContrib, 1.0f);
	color = total * color + (1.0f - total) * vec4(0.76470f, 0.84705f, 0.44313f, 1.0f);

	if (total > 0.4f)
		color = vec4(ceil(vec3(color) * 7.0f) / 7.0f, 1.0f);
	if (c == 0.0f)
		color /= 5.0f;
} 
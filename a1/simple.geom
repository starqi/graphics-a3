#version 430 core

layout (triangles_adjacency) in;
layout (triangle_strip, max_vertices = 15) out;

out vec3 gNormal, gPosition, gColor;
out vec4 gShadowC;
flat out int gIsEdge;

in vec3 vNormal[], vPosition[], vColor[];
in vec4 vShadowC[];

uniform float edgeWidth, extend;
uniform uint nonsenseOff;

bool isFrontFacing(vec3 a, vec3 b, vec3 c) {
	return ((a.x * b.y - b.x * a.y) + (b.x * c.y - c.x * b.y) + (c.x * a.y - a.x * c.y)) > 0; 
}

void emitEdgeQuad(vec3 e0, vec3 e1, vec3 c1, vec3 c2) {
	vec2 ext = extend * (e1.xy - e0.xy);
	vec2 v = normalize(e1.xy - e0.xy);
	vec2 n = vec2(-v.y, v.x) * edgeWidth;
	gIsEdge = 1;
	gl_Position = vec4(e0.xy - ext, e0.z, 1.0f);
	gColor = c1;
	EmitVertex();
	gl_Position = vec4(e0.xy - ext - n, e0.z, 1.0f);
	gColor = c1;
	EmitVertex();
	gl_Position = vec4(e1.xy + ext, e1.z, 1.0f);
	gColor = c2;
	EmitVertex();
	gl_Position = vec4(e1.xy + ext - n, e1.z, 1.0f);
	gColor = c2;
	EmitVertex();
	EndPrimitive();
}

void main() {

	if (nonsenseOff == 0) {
		vec3 p[6];
		for (uint i = 0; i < 5; ++i) {
			p[i] = gl_in[i].gl_Position.xyz / gl_in[i].gl_Position.w;
		}
		if (isFrontFacing(p[0], p[2], p[4])) {
			if (p[0] == p[1] || !isFrontFacing(p[0], p[1], p[2])) 
				emitEdgeQuad(p[0], p[2], vColor[0], vColor[2]);
			if (p[2] == p[3] || !isFrontFacing(p[2], p[3], p[4])) 
				emitEdgeQuad(p[2], p[4], vColor[2], vColor[4]);
			if (p[4] == p[5] || !isFrontFacing(p[4], p[5], p[0])) 
				emitEdgeQuad(p[4], p[0], vColor[4], vColor[0]);
		}
	}

	gIsEdge = 0;

	gNormal = vNormal[0];
	gPosition = vPosition[0];
	gColor = vColor[0];
	gShadowC = vShadowC[0];
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();

	gNormal = vNormal[2];
	gPosition = vPosition[2];
	gColor = vColor[2];
	gShadowC = vShadowC[2];
	gl_Position = gl_in[2].gl_Position;
	EmitVertex();

	gNormal = vNormal[4];
	gPosition = vPosition[4];
	gColor = vColor[4];
	gShadowC = vShadowC[4];
	gl_Position = gl_in[4].gl_Position;
	EmitVertex();

	EndPrimitive();
}
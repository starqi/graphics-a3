#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <string>
#include "SOIL.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <sstream>
#include <cstdarg>
#include <unordered_map>

#define WIDTH 1024
#define HEIGHT 768
#define CAMERA_DIST 75.0f

struct Shader
{
	Shader(int n, ...) {
		va_list vl;
		va_start(vl, n);
		char *path;
		GLenum shaderType;
		std::vector<GLuint> shaderIds;

		GLuint sp = glCreateProgram();

		for (int i = 0; i < n; ++i) {
			path = va_arg(vl, char*);
			shaderType = va_arg(vl, GLenum);
			GLuint shaderId = loadShader(path, shaderType);
			glAttachShader(sp, shaderId);
			shaderIds.push_back(shaderId);
		}

		GLint success;
		char infoLog[512];
		glLinkProgram(sp);
		glGetProgramiv(sp, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(sp, 512, nullptr, infoLog);
			std::cerr << "link: " << infoLog << std::endl;
			throw false;
		}

		for (GLuint i = 0; i < shaderIds.size(); ++i) {
			glDeleteShader(shaderIds[i]);
		}

		progId = sp;
	}

  	void use() {
		glUseProgram(progId);
	}

	GLuint getProgId() {
		return progId;
	}

private:
	GLuint progId;

	GLuint loadShader(const char *path, GLenum shaderType) {
		std::ifstream sFile(path);
		if (!sFile.is_open()) {
			std::cerr << path << " not found" << std::endl;
			throw false;
		}
		std::string sStr(
			(std::istreambuf_iterator<char>(sFile)),
			std::istreambuf_iterator<char>()
			);
		const char* sChar = sStr.c_str();
	
		GLuint shaderId;
		shaderId = glCreateShader(shaderType);
		glShaderSource(shaderId, 1, &sChar, nullptr);
		glCompileShader(shaderId);
		
		GLint success;
		char infoLog[512];
		glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shaderId, 512, nullptr, infoLog);
			std::cerr << path << " " << infoLog << std::endl;
			throw false;
		}

		return shaderId;
	}
};

struct Camera {

	glm::vec3 lookFrom, lookAt, lookUp;

	Camera() : lookFrom(0.0f, 0.0f, CAMERA_DIST), 
			   lookAt(0.0f, 0.0f, 0.0f), 
			   lookUp(0.0f, 1.0f, 0.0f),
			   persp(glm::perspective(glm::radians(55.0f), (float)WIDTH/(float)HEIGHT, 0.1f, 500.0f)) {}

	void preDraw(Shader shader, bool removeTranslate) {
		glUniform3fv(glGetUniformLocation(shader.getProgId(), "viewerPos"),
			1, glm::value_ptr(lookFrom));
		glUniformMatrix4fv(glGetUniformLocation(shader.getProgId(), "view"),
			1, GL_FALSE, glm::value_ptr(getViewMatrix(removeTranslate)));
		glUniformMatrix4fv(glGetUniformLocation(shader.getProgId(), "proj"),
			1, GL_FALSE, glm::value_ptr(persp));
	}
	
	glm::mat4 getViewMatrix(bool removeTranslate) {
		glm::mat4 m = glm::lookAt(lookFrom, lookAt, lookUp);
		if (removeTranslate) {
			m[3][0] = 0.0f;
			m[3][1] = 0.0f;
			m[3][2] = 0.0f;
		}
		return m;
	}

	const glm::mat4 persp;
};

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////
// Modified Assimp wrappers "Vertex, Texture, Mesh, Model" from
// http://learnopengl.com/
// For loading models (eg. obj files)

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 TexCoords;
};

struct Texture {
    GLuint id;
    std::string type;
	aiString path;
}; 

struct EdgeKeyValue {

	EdgeKeyValue() : state(0), a((GLuint)-1), b((GLuint)-1) {}
	EdgeKeyValue(GLuint a, GLuint b) : state(0), a(a), b(b) {}

	void insert(GLuint val) {
		if (state == 0) {
			a = val;
			state++;
		} else if (state == 1) {
			b = val;
			state++;
		}
	}

	inline GLuint getA() const { return a; }
	inline GLuint getB() const { return b; }

private:
	long a, b;
	GLuint state;
};

template<> struct std::hash<EdgeKeyValue> {
	std::size_t operator()(EdgeKeyValue const& e) const {
		GLuint a, b;
		if (e.getA() < e.getB()) {
			a = e.getA();
			b = e.getB();
		} else {
			a = e.getB();
			b = e.getA();
		}
		std::size_t h1 = std::hash<GLuint>()(a);
		std::size_t h2 = std::hash<GLuint>()(b);
		return h1 ^ (h2 << 1);
	}
};

template<> struct std::equal_to<EdgeKeyValue> {
	bool operator()(const EdgeKeyValue &x, const EdgeKeyValue &y) {
		return (x.getA() == y.getA() && x.getB() == y.getB()) || (x.getA() == y.getB() && x.getB() == y.getA());
	}
};
	
class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::vector<Texture> textures;

	Mesh() {}

    Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures) {
		this->vertices = vertices;
		this->computeAdjacency(indices);
		this->textures = textures;
		this->setupMesh();
	}
	
    void Draw(Shader shader, Camera &camera, glm::mat4 &modelMatrix, bool isColor) {
		if (vertices.size() == 0) return;
		GLuint diffuseNr = 1;
		GLuint specularNr = 1;
		glUniform1ui(glGetUniformLocation(shader.getProgId(), "isColor"), isColor ? 1 : 0);
		glUniformMatrix4fv(glGetUniformLocation(shader.getProgId(), "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
		glUniformMatrix4fv(glGetUniformLocation(shader.getProgId(), "normalModel"), 
			1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(modelMatrix))));
		for (GLuint i = 0; i < this->textures.size(); i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			std::stringstream ss;
			std::string number;
			std::string name = this->textures[i].type;
			if(name == "texture_diffuse")
				ss << diffuseNr++;
			else if(name == "texture_specular")
				ss << specularNr++;
			number = ss.str(); 
			glUniform1i(glGetUniformLocation(shader.getProgId(), ("material." + name + number).c_str()), i);
 			glBindTexture(GL_TEXTURE_2D, this->textures[i].id);
		}
		glActiveTexture(GL_TEXTURE0);

		glBindVertexArray(this->VAO);
		camera.preDraw(shader, false);
		glDrawElements(GL_TRIANGLES_ADJACENCY, this->indices.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	void computeAdjacency(std::vector<GLuint> indices) {
		std::unordered_map<EdgeKeyValue, EdgeKeyValue> dict;

		for (GLuint i = 0; i < indices.size(); i += 3) {
			EdgeKeyValue ek1(indices[i], indices[i + 1]);
			EdgeKeyValue ek2(indices[i + 1], indices[i + 2]);
			EdgeKeyValue ek3(indices[i + 2], indices[i]);

			dict[ek1].insert(indices[i + 2]);
			dict[ek2].insert(indices[i]);
			dict[ek3].insert(indices[i + 1]);
		}

		for (GLuint i = 0; i < indices.size(); i += 3) {
			GLuint places[3][3] = {
				i, i + 1, i + 2,
				i + 1, i + 2, i,
				i + 2, i, i + 1
			};
			for (GLuint j = 0; j < 3; ++j) {
				EdgeKeyValue ek1(
					indices[places[j][0]],
					indices[places[j][1]]
				);
				EdgeKeyValue result = dict[ek1];
				this->indices.push_back(indices[places[j][0]]);
				GLuint middle = indices[places[j][2]] == result.getA() ? result.getB() : result.getA();
				this->indices.push_back(middle == (GLuint)-1 ? indices[places[j][0]] : middle);
			}
		}
	}

    void setupMesh() {
		glGenVertexArrays(1, &this->VAO);
		glGenBuffers(1, &this->VBO);
		glGenBuffers(1, &this->EBO);
  
		glBindVertexArray(this->VAO);
		glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
		glBufferData(GL_ARRAY_BUFFER, this->vertices.size() * sizeof(Vertex), 
						&this->vertices[0], GL_STATIC_DRAW);  

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices.size() * sizeof(GLuint), 
						&this->indices[0], GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);	
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
								(GLvoid*)0);

		glEnableVertexAttribArray(1);	
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
								(GLvoid*)offsetof(Vertex, Normal));

		glEnableVertexAttribArray(2);	
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
								(GLvoid*)offsetof(Vertex, TexCoords));

		glBindVertexArray(0);
	}
	
private:
    GLuint VAO, VBO, EBO;

}; 

GLint TextureFromFile(const char* path, std::string directory)
{
    std::string filename = std::string(path);
    filename = directory + '/' + filename;
    GLuint textureID;
    glGenTextures(1, &textureID);
    int width, height;
    unsigned char* image = SOIL_load_image(filename.c_str(), &width, &height, 0, SOIL_LOAD_RGB);
 
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);	

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    SOIL_free_image_data(image);

    return textureID;
}

class Model 
{
public:
	glm::mat4 modelMatrix;

    Model(GLchar* path, bool flipWinding)
		: modelMatrix(1.0f), flipWinding(flipWinding)
    {
        this->loadModel(path);
    }

    void Draw(Shader shader, Camera &camera) {
		for (GLuint i = 0; i < this->meshes.size(); i++)
			this->meshes[i].Draw(shader, camera, modelMatrix, false);
	}

private:
	std::vector<Texture> textures_loaded; 
    std::vector<Mesh> meshes;
    std::string directory;
	bool flipWinding;
       
	void loadModel(std::string path) {
		Assimp::Importer import;
		const aiScene* scene = import.ReadFile(path,
			aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices |
			(flipWinding ? aiProcess_FlipWindingOrder : 0));	
		if(!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
		{
			std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
			throw false;
		}
		this->directory = path.substr(0, path.find_last_of('/'));
		this->processNode(scene->mRootNode, scene);
	}

    void processNode(aiNode* node, const aiScene* scene) {
		for (GLuint i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]]; 
			this->meshes.push_back(this->processMesh(mesh, scene));			
		}	
		for(GLuint i = 0; i < node->mNumChildren; i++)
		{
			this->processNode(node->mChildren[i], scene);
		}
	}

    Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
		std::vector<Vertex> vertices;
		std::vector<GLuint> indices;
		std::vector<Texture> textures;

		for(GLuint i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;

			glm::vec3 vector; 
			vector.x = mesh->mVertices[i].x;
			vector.y = mesh->mVertices[i].y;
			vector.z = mesh->mVertices[i].z; 
			vertex.Position = vector;

			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
			vertex.Normal = vector;  

			if (mesh->mTextureCoords[0])
			{
				glm::vec3 vec;
				vec.x = mesh->mTextureCoords[0][i].x; 
				vec.y = mesh->mTextureCoords[0][i].y;
				vec.z = 0.0f;
				vertex.TexCoords = vec;
			}
			else
				vertex.TexCoords = glm::vec3(0.0f, 0.0f, 0.0f);  

			vertices.push_back(vertex);
		}
			
		for (GLuint i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for(GLuint j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}  

		if (mesh->mMaterialIndex >= 0)
		{
			aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
			std::vector<Texture> diffuseMaps = this->loadMaterialTextures(material, 
												aiTextureType_DIFFUSE, "texture_diffuse");
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
			std::vector<Texture> specularMaps = this->loadMaterialTextures(material, 
												aiTextureType_SPECULAR, "texture_specular");
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		}

		return Mesh(vertices, indices, textures);
	}

    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, 
                                        std::string typeName) {
		std::vector<Texture> textures;
		for(GLuint i = 0; i < mat->GetTextureCount(type); i++)
		{
			aiString str;
			mat->GetTexture(type, i, &str);
			GLboolean skip = false;
			for(GLuint j = 0; j < textures_loaded.size(); j++)
			{
				if (textures_loaded[j].path == str)
				{
					textures.push_back(textures_loaded[j]);
					skip = true; 
					break;
				}
			}
			if(!skip)
			{  
				Texture texture;
				texture.id = TextureFromFile(str.C_Str(), this->directory);
				texture.type = typeName;
				texture.path = str;
				textures.push_back(texture);
				this->textures_loaded.push_back(texture);
			}
		}
		return textures;
	}
};

//////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////

struct CubeMap {
	CubeMap(char **list, GLenum active) {
		glGenTextures(1, &tid);
		int width, height;
		unsigned char* image;
		glActiveTexture(active);
		glBindTexture(GL_TEXTURE_CUBE_MAP, tid);
		for (int i = 0; i < 6; ++i) {
			image = SOIL_load_image(list[i], &width, &height, 0, SOIL_LOAD_RGB);
			if (image == NULL) {
				std::cerr << "Load cube map failed" << std::endl;
				throw false;
			}
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height,
				0, GL_RGB, GL_UNSIGNED_BYTE, image);
			SOIL_free_image_data(image);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);  
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}

	GLuint getTid() {
		return tid;
	}

private:
	GLuint tid;
};

struct Light {
	glm::vec3 position, ambient, diffuse, specular;

	Light() : position(0.0f),
		ambient(0.45f), diffuse(1.0f), specular(0.6f) {
	}

	void preDraw(Shader shader) {
		glUniform3fv(glGetUniformLocation(shader.getProgId(), "light.position"), 1, glm::value_ptr(position));
		glUniform3fv(glGetUniformLocation(shader.getProgId(), "light.ambient"), 1, glm::value_ptr(ambient));
		glUniform3fv(glGetUniformLocation(shader.getProgId(), "light.diffuse"), 1, glm::value_ptr(diffuse));
		glUniform3fv(glGetUniformLocation(shader.getProgId(), "light.specular"), 1, glm::value_ptr(specular));
	}
};

struct SkyBox {

	SkyBox(char **list) 
		: skyShader(2, 
			"../a1/skybox.vert", GL_VERTEX_SHADER,
			"../a1/skybox.frag", GL_FRAGMENT_SHADER),
		cubeMap(list, GL_TEXTURE0) {

		texId = glGetUniformLocation(skyShader.getProgId(), "skyTex");

		glGenVertexArrays(1, &vao);
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);
  
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(GLfloat), &SkyBox::skyCubeVertices[0], GL_STATIC_DRAW);  

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, 36 * sizeof(GLuint), &SkyBox::skyCubeIndices[0], GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);	
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);

		glBindVertexArray(0);
	}

	void draw(Camera &camera) {
		camera.preDraw(skyShader, true);
		glBindVertexArray(vao);
		glActiveTexture(GL_TEXTURE0);
		glUniform1i(texId, 0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubeMap.getTid());
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, NULL);
		glBindVertexArray(0);
	}

	Shader skyShader;

private:
	static GLfloat skyCubeVertices[];
	static GLuint skyCubeIndices[];
	
	CubeMap cubeMap;
	GLint texId;
	GLuint vao, vbo, ebo;
};

GLfloat SkyBox::skyCubeVertices[] = {
	-1.0f, -1.0f, -1.0f,
	-1.0f, 1.0f, -1.0f,
	1.0f, 1.0f, -1.0f,
	1.0f, -1.0f, -1.0f,

	-1.0f, -1.0f, 1.0f,
	-1.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f,
	1.0f, -1.0f, 1.0f,
};

GLuint SkyBox::skyCubeIndices[] = {
	0, 1, 2, 0, 2, 3,
	4, 5, 6, 4, 6, 7,
	0, 1, 5, 0, 5, 4,
	2, 3, 7, 2, 7, 6,
	1, 2, 6, 1, 6, 5,
	0, 3, 7, 0, 7, 4
};

class Program {
	static char *skyBoxList[];
	Shader defaultShader, shadowShader;
	Light light;
	Camera camera;
	SkyBox skyBox;
	Mesh floor;
	Model goku, vegeta, portrait;
	float rotation;
	GLint edgeWidthId, extendId, nonsenseId, vId, gId, sMapId, sMatId;
	glm::mat4 floorModel;
	GLuint depthMapId, depthMapFbo;

public:
	
	Program() : 
		skyBox(Program::skyBoxList),
		shadowShader(2,
			"../a1/shadow.vert", GL_VERTEX_SHADER,
			"../a1/shadow.frag", GL_FRAGMENT_SHADER),
		defaultShader(3,
			"../a1/simple.vert", GL_VERTEX_SHADER,
			"../a1/simple.frag", GL_FRAGMENT_SHADER,
			"../a1/simple.geom", GL_GEOMETRY_SHADER),
		rotation(0.0f),
		goku("../Debug/Goku.obj", false),
		vegeta("../Debug/Vegeta.obj", true),
		portrait("../Debug/model.obj", false) {

		Vertex floorVertices[] = {
			{
				glm::vec3(-10000.0f, -0.0f, -10000.0f),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec3(0.0f, 0.0f, 0.0f)
			},
			{
				glm::vec3(-10000.0f, -0.0f, 10000.0f),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec3(0.0f, 600.0f, 0.0f)
			},
						{
				glm::vec3(10000.0f, -0.0f, 10000.0f),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec3(600.0f, 600.0f, 0.0f)
			},
						{
				glm::vec3(10000.0f, -0.0f, -10000.0f),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec3(600.0f, 0.0f, 0.0f)
			}
		};

		GLuint floorIndices[] = {
			0, 0, 1, 1, 2, 2,
			0, 0, 2, 2, 3, 3
		};

		floor.vertices = std::vector<Vertex>(floorVertices, floorVertices + sizeof(floorVertices) / sizeof(Vertex));
		floor.indices = std::vector<GLuint>(floorIndices, floorIndices + sizeof(floorIndices) / sizeof(GLuint));
		floor.textures = std::vector<Texture>();
		floorModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -40.0f, 0.0f));

		GLuint a = TextureFromFile("../Debug/floor.bmp", ".");
		Texture aa = {
			a, "texture_diffuse", aiString()
		};
		floor.textures.push_back(aa);
		floor.setupMesh();

		goku.modelMatrix = 
			glm::rotate(
			glm::scale(
			glm::translate(
			glm::mat4(1.0f), glm::vec3(-20.0f, -40.0f, 0.0f)), 
			glm::vec3(1.6f)),
			glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		vegeta.modelMatrix = 
			glm::rotate(
			glm::scale(
			glm::translate(
			glm::mat4(1.0f), glm::vec3(20.0f, -40.0f, 0.0f)), 
			glm::vec3(1.0f)),
			glm::radians(270.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		portrait.modelMatrix = 
			glm::rotate(
			glm::scale(
			glm::translate(
			glm::mat4(1.0f), glm::vec3(-45.0f, -35.0f, -45.0f)), 
			glm::vec3(20.0f)),
			glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		light.position = glm::vec3(-CAMERA_DIST, CAMERA_DIST, -CAMERA_DIST);
		edgeWidthId = glGetUniformLocation(defaultShader.getProgId(), "edgeWidth");
		extendId = glGetUniformLocation(defaultShader.getProgId(), "extend");
		nonsenseId = glGetUniformLocation(defaultShader.getProgId(), "nonsenseOff");
		vId = glGetUniformLocation(defaultShader.getProgId(), "vegetaLoc");
		gId = glGetUniformLocation(defaultShader.getProgId(), "gokuLoc");
		sMapId = glGetUniformLocation(defaultShader.getProgId(), "shadowMap");
		sMatId = glGetUniformLocation(defaultShader.getProgId(), "shadowMatrix");
		glUniform3fv(vId, 1, glm::value_ptr(glm::vec3(20.0f, -40.0f, 0.0f)));
		glUniform3fv(gId, 1, glm::value_ptr(glm::vec3(-20.0f, -40.0f, 0.0f)));

		glGenFramebuffers(1, &depthMapFbo);  

		GLfloat border[] = {1.0f, 0.0f, 0.0f, 0.0f};
		glGenTextures(1, &depthMapId);
		glBindTexture(GL_TEXTURE_2D, depthMapId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
					 WIDTH, HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);  
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapId, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);  
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
	}

	void update(bool isAnimating, double diff) {
		rotation += isAnimating ? 0.005f : 0.0f;
		rotation = std::fmod(rotation, 3.14159f * 2.0f);
		camera.lookFrom.x = CAMERA_DIST * std::sin(rotation);
		camera.lookFrom.z = CAMERA_DIST * std::cos(rotation);
		camera.lookFrom.y = std::max(CAMERA_DIST * std::sin(rotation), 0.0f);

		shadowShader.use();
		glViewport(0, 0, WIDTH, HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFbo);
		glClear(GL_DEPTH_BUFFER_BIT);
		Camera shadowCamera;
		shadowCamera.lookAt = glm::vec3(0.0f);
		shadowCamera.lookFrom = light.position;
		goku.Draw(shadowShader, shadowCamera);
		vegeta.Draw(shadowShader, shadowCamera);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		skyBox.skyShader.use();
		glDisable(GL_DEPTH_TEST);
		skyBox.draw(camera);
		glEnable(GL_DEPTH_TEST);

		defaultShader.use();
		light.specular = glm::vec3(0.5f);
		light.preDraw(defaultShader);
		glUniformMatrix4fv(sMatId, 1, false,
			glm::value_ptr(shadowCamera.persp * shadowCamera.getViewMatrix(false)));
		glActiveTexture(GL_TEXTURE0 + 5);
		glUniform1i(sMapId, 5);
		glBindTexture(GL_TEXTURE_2D, depthMapId);
		glUniform1f(edgeWidthId, 0.005f);
		glUniform1f(extendId, 0.00f);
		glUniform1ui(nonsenseId, 0);
		goku.Draw(defaultShader, camera);
		vegeta.Draw(defaultShader, camera);
		
		floor.Draw(defaultShader, camera, floorModel, false);
		glUniform1ui(nonsenseId, 1);
		light.specular = glm::vec3(1.0f);
		light.preDraw(defaultShader);
		portrait.Draw(defaultShader, camera);
	}
};

char *Program::skyBoxList[] = {
	"../Debug/side.bmp", "../Debug/side.bmp", "../Debug/up.bmp", 
	"../Debug/down.bmp", "../Debug/side.bmp", "../Debug/side.bmp"
};

////////////////////////////////////////////////////////////////////
// Window code
////////////////////////////////////////////////////////////////////

void error_callback(int error, const char* description) {
	std::cerr << description << std::endl;
}
bool animating = false;
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == 'A' && action == GLFW_RELEASE) {
		animating = !animating;
	}
}

int main() {
	if (!glfwInit()) {
		exit(1);
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
	glfwSetErrorCallback(error_callback);
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "a1", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		exit(1);
	}
	glfwSetKeyCallback(window, key_callback);
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		glfwTerminate();
		exit(1);
	}
	glViewport(0, 0, WIDTH, HEIGHT);

	Program prog;

	double lastTime = 0.0;
	unsigned counter = 0;
	
	while (!glfwWindowShouldClose(window)) {
		double x = glfwGetTime();
		double diff = x - lastTime;

		glfwPollEvents();
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		prog.update(animating, diff);
		glfwSwapBuffers(window);

		if (animating && diff > 2.0) {
			glfwSetWindowTitle(window, std::to_string(counter).c_str());
			lastTime = x;
			counter = 0;
		}
		counter++;
	}
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

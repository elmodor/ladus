#pragma once

#include <vector>
#include <glm/glm.hpp>



class Model {
public:

	bool load(const char* name);

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
	};

//private:
	struct Mesh {
		glm::vec3 color;
		std::vector<Vertex>	vertices;
		std::vector<int>	indices;

//		btBvhTriangleMeshShape*		shape;
//		btTriangleIndexVertexArray* interface;
	};
	std::vector<Mesh> meshes;

};

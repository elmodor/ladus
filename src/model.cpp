#include "model.h"

#include <cstdio>
#include <string>
#include <cassert>



bool Model::load(const char* name) {
	FILE* f = fopen(name, "r");
	if (!f) return false;
	char line[256];


	Mesh* mesh = nullptr;
	while (fgets(line, sizeof(line), f)) {

		char* p = line;
		auto next_word = [&]() -> char* {
			char* q = p;
			while (*p && !isspace(*p)) p++;
			for (; isspace(*p); ++p) *p = '\0';
			return q;
		};

		std::string cmd = next_word();
		if (cmd == "mesh") {
			meshes.emplace_back();
			mesh = &meshes.back();
		}
		else if (cmd == "color") {
			mesh->color.x = std::stof(next_word()) / 255.0;
			mesh->color.y = std::stof(next_word()) / 255.0;
			mesh->color.z = std::stof(next_word()) / 255.0;
		}
		else if (cmd == "vertex") {
			mesh->vertices.emplace_back();
			auto& v = mesh->vertices.back();
			v.pos.x = std::stof(next_word());
			v.pos.y = std::stof(next_word());
			v.pos.z = std::stof(next_word());
		}
		else if (cmd == "normal") {
			auto& v = mesh->vertices.back();
			v.normal.x = std::stof(next_word());
			v.normal.y = std::stof(next_word());
			v.normal.z = std::stof(next_word());
		}
		else if (cmd == "triangle") {
			mesh->indices.push_back(std::stod(next_word()));
			mesh->indices.push_back(std::stod(next_word()));
			mesh->indices.push_back(std::stod(next_word()));
		}
		else {
			assert(false);
		}
	}
	fclose(f);


//		for (auto& mesh : meshes) {
//			mesh.interface = new btTriangleIndexVertexArray(
//				mesh.indices.size() / 3, mesh.indices.data(), sizeof(int) * 3,
//				mesh.vertices.size(), (float*) mesh.vertices.data(), sizeof(glm::vec3));
//			mesh.shape = new btBvhTriangleMeshShape(mesh.interface, true);
//		}


	return true;
}

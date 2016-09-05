#include "rmw.h"
#include "model.h"

#include <cstdlib>
#include <cstdio>
#include <vector>

#include <glm/gtx/transform.hpp>

#include <btBulletDynamicsCommon.h>


rmw::Context ctx;


class Scene {
public:


	void init() {


		// gfx

		shader = ctx.create_shader(
			VERTEX_SHADER(
				layout(location = 0) in vec3 in_pos;
				layout(location = 1) in vec3 in_normal;
				uniform mat4 u_mvp;
				out vec3 v_normal;
				out float v_z;
				void main() {
					gl_Position = u_mvp * vec4(in_pos, 1.0);
					v_normal = in_normal;
					v_z = in_pos.z;
				}
			), FRAGMENT_SHADER(
				in vec3 v_normal;
				in float v_z;
				out vec4 out_color;
				void main() {
					out_color = vec4(normalize(v_normal) * 0.25 + vec3(0.5), 1.0);
					//out_color = vec4(
					//	vec3(0.5) * (0.1 + 0.9 * max(0, dot(normalize(vec3(0.3, 0.6, 2.0)), normalize(v_normal)))),
					//	1.0);
					out_color *= 0.6 + v_z * 0.05;
				}
			));



		model.load("media/map.model");

		vertex_buffer = ctx.create_vertex_buffer(rmw::BufferHint::StreamDraw);
		vertex_buffer->init_data(model.meshes[0].vertices);

		vertex_array = ctx.create_vertex_array();
		vertex_array->set_count(model.meshes[0].vertices.size());
		vertex_array->set_attribute(0, *vertex_buffer, rmw::ComponentType::Float, 3, false,  0, sizeof(Model::Vertex));
		vertex_array->set_attribute(1, *vertex_buffer, rmw::ComponentType::Float, 3, false, 12, sizeof(Model::Vertex));




		// bullet



		collision_conf = std::make_unique<btDefaultCollisionConfiguration>();
		dispatcher = std::make_unique<btCollisionDispatcher>(collision_conf.get());

		broadphase = std::make_unique<btDbvtBroadphase>();
		solver = std::make_unique<btSequentialImpulseConstraintSolver>();
		world = std::make_unique<btDiscreteDynamicsWorld>(
				dispatcher.get(),
				broadphase.get(),
				solver.get(),
				collision_conf.get());

		world->setGravity(btVector3(0, -10, 0));
		world->setInternalTickCallback(Scene::update, static_cast<void*>(this), false);





		// load mesh into mesh interface
		// the vertices must be indexed over their position (like in obj files)

		// FIXME
		static std::vector<int> indices;
		for (size_t i = 0; i < model.meshes[0].vertices.size(); ++i) {
			indices.push_back(i);
		}
		interface = std::make_unique<btTriangleIndexVertexArray>(
				indices.size() / 3,
				indices.data(),
				sizeof(int) * 3,
				model.meshes[0].vertices.size(),
				(float*) model.meshes[0].vertices.data(),
				sizeof(Model::Vertex));


		shape = std::make_unique<btBvhTriangleMeshShape>(interface.get(), true);


		motion_state = std::make_unique<btDefaultMotionState>(
				btTransform(btQuaternion(0, 0, 0, 1),
				btVector3(0, 0, 0)));

		auto construction_info = btRigidBody::btRigidBodyConstructionInfo(
				0,
				motion_state.get(),
				shape.get(),
				btVector3(0, 0, 0));

		rigid_body = std::make_unique<btRigidBody>(construction_info);

		world->addRigidBody(rigid_body.get());

	}

	void update(float dt) {
		world->stepSimulation(dt, 5);
	}



	void draw(rmw::RenderState rs, const glm::vec2& camera) {

		glm::mat4 mvp = glm::perspectiveFov<float>(1.0, 800, 600, 1, 100)
					  * glm::translate(glm::vec3(camera.x, camera.y, -25));

		shader->set_uniform("u_mvp", mvp);

		ctx.draw(rs, *shader, *vertex_array);
	}


private:

	void tick() {



	}


	static void update(btDynamicsWorld *world, float dt) {
		static_cast<Scene*>(world->getWorldUserInfo())->tick();
	}


	Model						model;

	rmw::Shader::Ptr			shader;
	rmw::VertexBuffer::Ptr		vertex_buffer;
	rmw::VertexArray::Ptr		vertex_array;


	// bullet stuff

	std::unique_ptr<btDefaultCollisionConfiguration>		collision_conf;
	std::unique_ptr<btCollisionDispatcher>					dispatcher;
	std::unique_ptr<btDbvtBroadphase>						broadphase;
	std::unique_ptr<btSequentialImpulseConstraintSolver>	solver;
	std::unique_ptr<btDiscreteDynamicsWorld>				world;


	// ~per body stuff
	std::unique_ptr<btBvhTriangleMeshShape>					shape;
	std::unique_ptr<btTriangleIndexVertexArray> 			interface;
	std::unique_ptr<btDefaultMotionState>					motion_state;
	std::unique_ptr<btRigidBody>							rigid_body;

};





int main(int argc, char** argv) {


	ctx.init(800, 600);


	Scene scene;
	scene.init();



	rmw::ClearState cs;
	cs.color = glm::vec4(0.2, 0.3, 0.5, 1.0);


	rmw::RenderState rs;
	rs.depth_test_func = rmw::DepthTestFunc::LEqual;
	rs.depth_test_enabled = true;


	SDL_ShowCursor(SDL_DISABLE);
	SDL_SetRelativeMouseMode(SDL_TRUE);


	glm::vec2 camera;

	uint32_t time = SDL_GetTicks();

	bool running = true;
	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {

			case SDL_QUIT:
				running = false;
				break;

			case SDL_KEYDOWN:
				if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = false;
				break;

			case SDL_WINDOWEVENT:
				switch (e.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
					rs.viewport.w = e.window.data1;
					rs.viewport.h = e.window.data2;
					break;
				default: break;
				}
				break;


			case SDL_MOUSEMOTION:
				camera.x -= e.motion.xrel * 0.1;
				camera.y += e.motion.yrel * 0.1;


			default: break;
			}
		}



		// update

		uint32_t now = SDL_GetTicks();
		scene.update((now - time) * 0.001);
		time = now;



		// render

		ctx.clear(cs);

		scene.draw(rs, camera);

		ctx.flip_buffers();
	}

	return 0;
}

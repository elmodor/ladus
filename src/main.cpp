#include "rmw.h"
#include "model.h"

#include <cstdlib>
#include <cstdio>
#include <vector>

#include <glm/gtx/transform.hpp>

#include <btBulletDynamicsCommon.h>


rmw::Context ctx;


class DebugDrawer : public btIDebugDraw {
public:

	struct Vertex {
		glm::vec3 pos;
		glm::vec3 color;
	};

	void init() {

		shader = ctx.create_shader(
			VERTEX_SHADER(
				layout(location = 0) in vec3 in_pos;
				layout(location = 1) in vec3 in_color;
				uniform mat4 u_mvp;
				out vec3 v_color;
				void main() {
					gl_Position = u_mvp * vec4(in_pos, 1.0);
					v_color = in_color;
				}
			), FRAGMENT_SHADER(
				in vec3 v_color;
				out vec4 out_color;
				void main() {
					out_color = vec4(v_color, 1.0);
				}
			));


		vertex_buffer = ctx.create_vertex_buffer(rmw::BufferHint::StreamDraw);
		vertex_array = ctx.create_vertex_array();
		vertex_array->set_primitive_type(rmw::PrimitiveType::Lines);
		vertex_array->set_attribute(0, *vertex_buffer, rmw::ComponentType::Float, 3, false,  0, sizeof(Model::Vertex));
		vertex_array->set_attribute(1, *vertex_buffer, rmw::ComponentType::Float, 3, false, 12, sizeof(Model::Vertex));

	}

	void draw(rmw::RenderState rs, const glm::mat4& mvp) {

		shader->set_uniform("u_mvp", mvp);

		vertex_buffer->init_data(vertices);
		vertex_array->set_count(vertices.size());
		ctx.draw(rs, *shader, *vertex_array);

		vertices.clear();
	}


	void drawLine(const btVector3& a, const btVector3& b, const btVector3& color) override {
		vertices.push_back({
			* (glm::vec3*) & a,
			* (glm::vec3*) & color,
		});
		vertices.push_back({
			* (glm::vec3*) & b,
			* (glm::vec3*) & color,
		});

	}

	void drawContactPoint (
			const btVector3 &PointOnB,
			const btVector3 &normalOnB,
			btScalar distance,
			int lifeTime,
			const btVector3 &color)
	{
	}

	void reportErrorWarning(const char* warningString) override {
	}
	void draw3dText(const btVector3& location, const char* textString) override {
	}

	void setDebugMode(int debugMode) override {
		mode = debugMode;
	}

	int getDebugMode() const override {
		return mode;
	}


private:

	rmw::Shader::Ptr		shader;
	rmw::VertexBuffer::Ptr	vertex_buffer;
	rmw::VertexArray::Ptr	vertex_array;
	std::vector<Vertex>		vertices;
	int						mode;
};









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
		dynamics_world = std::make_unique<btDiscreteDynamicsWorld>(
				dispatcher.get(),
				broadphase.get(),
				solver.get(),
				collision_conf.get());

		dynamics_world->setGravity(btVector3(0, -10, 0));
		dynamics_world->setInternalTickCallback(Scene::update, static_cast<void*>(this), false);





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

		dynamics_world->addRigidBody(rigid_body.get());


		// debug drawing
		drawer.init();
		drawer.setDebugMode(btIDebugDraw::DBG_DrawWireframe);
		dynamics_world->setDebugDrawer(&drawer);
	}

	void update(float dt) {
		dynamics_world->stepSimulation(dt, 5);
	}



	void draw(rmw::RenderState rs, const glm::mat4& mvp) {

		shader->set_uniform("u_mvp", mvp);

		ctx.draw(rs, *shader, *vertex_array);


		// debug drawing
		dynamics_world->debugDrawWorld();
		drawer.draw(rs, mvp);
	}


private:
	DebugDrawer drawer;


	void tick() {



	}


	static void update(btDynamicsWorld* dynamics_world, float dt) {
		static_cast<Scene*>(dynamics_world->getWorldUserInfo())->tick();
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
	std::unique_ptr<btDiscreteDynamicsWorld>				dynamics_world;


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

		glm::mat4 mvp = glm::perspectiveFov<float>(1.0, 800, 600, 1, 100)
					  * glm::translate(glm::vec3(camera.x, camera.y, -25));

		scene.draw(rs, mvp);


		ctx.flip_buffers();
	}

	return 0;
}

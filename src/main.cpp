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

	void clear() {
		vertices.clear();
	}

	void draw(rmw::RenderState rs, const glm::mat4& mvp) {

		shader->set_uniform("u_mvp", mvp);

		vertex_buffer->init_data(vertices);
		vertex_array->set_count(vertices.size());

		rs.depth_test_enabled = false;
		ctx.draw(rs, *shader, *vertex_array);

		clear();
	}


	// the line method i use myself
	void line(const glm::vec2& a, const glm::vec2& b, const glm::vec3& color) {
		vertices.push_back({ glm::vec3(a, 0), color });
		vertices.push_back({ glm::vec3(b, 0), color });
	}
	void circle(const glm::vec2& a, float r, const glm::vec3& color) {
		glm::vec2 p = a + glm::vec2(0, r);
		for (int i = 1; i <= 8; i++) {
			glm::vec2 q = a + glm::vec2(sin(i / 4.0f * M_PI), cos(i / 4.0f * M_PI)) * r;
			line(p, q, color);
			p = q;
		}
	}



	void drawLine(const btVector3& a, const btVector3& b, const btVector3& color) override {
		vertices.push_back({
			*(glm::vec3*) & a,
			*(glm::vec3*) & color,
		});
		vertices.push_back({
			*(glm::vec3*) & b,
			*(glm::vec3*) & color,
		});
	}

	void drawContactPoint (
			const btVector3 &PointOnB,
			const btVector3 &normalOnB,
			btScalar distance,
			int lifeTime,
			const btVector3 &color)
	{}

	void reportErrorWarning(const char* str) override {
		printf("DebugDrawer: %s\n", str);
	}
	void draw3dText(const btVector3& location, const char* textString) override {}

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
	std::vector<Vertex>		vertices; // line list
	int						mode;
};





struct Player {

//private:
	std::unique_ptr<btBoxShape>				shape;
	std::unique_ptr<btDefaultMotionState>	motion_state;
	std::unique_ptr<btRigidBody>			rigid_body;

};



struct Player2 {
	glm::vec2	size { 2, 3 };

	glm::vec2	pos { 0, 0 };
	glm::vec2	vel;

	bool		in_air;

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

		dynamics_world->setGravity(btVector3(0, -30, 0));
		dynamics_world->setInternalTickCallback(Scene::update, static_cast<void*>(this), false);


		// debug drawing
		drawer.init();
		drawer.setDebugMode(btIDebugDraw::DBG_DrawWireframe);
		dynamics_world->setDebugDrawer(&drawer);



		// map
		{
			// load mesh into mesh interface
			// the vertices must be indexed over their position (like in obj files)

			// FIXME: this is bad
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
					0, // mass of 0 means static world
					motion_state.get(), shape.get());

			rigid_body = std::make_unique<btRigidBody>(construction_info);



			// disable debug drawer for map
			int f = rigid_body->getCollisionFlags();
			rigid_body->setCollisionFlags(f | btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT);

			dynamics_world->addRigidBody(rigid_body.get());
		}




		// player
		{
			player.shape = std::make_unique<btBoxShape>(btVector3(1, 1.5, 1));


			player.motion_state = std::make_unique<btDefaultMotionState>(
					btTransform(btQuaternion(0, 0, 0, 1),
					btVector3(5, 0, 0)));

			float		mass = 100;
			btVector3	inertia(0, 0, 0);
//			player.shape->calculateLocalInertia(mass, inertia);

			auto construction_info = btRigidBody::btRigidBodyConstructionInfo(
					mass,
					player.motion_state.get(),
					player.shape.get(),
					inertia);

			player.rigid_body = std::make_unique<btRigidBody>(construction_info);

			dynamics_world->addRigidBody(player.rigid_body.get());
		}
	}

	void update(float dt) {
		dynamics_world->stepSimulation(dt, 5);
	}



	void draw(rmw::RenderState rs, const glm::mat4& mvp) {

		shader->set_uniform("u_mvp", mvp);

		ctx.draw(rs, *shader, *vertex_array);



		// debug drawing
		dynamics_world->debugDrawWorld();

		auto a = player2.size * 0.5f;
		auto b = glm::vec2(a.x, -a.y);

		auto col = player2.in_air ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, 1);

		drawer.line(player2.pos + a, player2.pos + b, col);
		drawer.line(player2.pos + b, player2.pos - a, col);
		drawer.line(player2.pos - a, player2.pos - b, col);
		drawer.line(player2.pos - b, player2.pos + a, col);




		drawer.draw(rs, mvp);
	}


private:
	static void update(btDynamicsWorld* dynamics_world, float dt) {
		static_cast<Scene*>(dynamics_world->getWorldUserInfo())->tick();
	}


	void tick() {

		const Uint8* ks = SDL_GetKeyboardState(nullptr);
		int dx = !!ks[SDL_SCANCODE_RIGHT] - !!ks[SDL_SCANCODE_LEFT];
		int dy = !!ks[SDL_SCANCODE_UP] - !!ks[SDL_SCANCODE_DOWN];


		// player
//		{
//			btVector3 vel = player.rigid_body->getLinearVelocity();
//
//			// walking
//			vel.setX(std::min(10.0f, std::max(-10.0f, vel.x() + dx)));
//
//			// z correction
//			vel.setZ(std::min(5.0f, std::max(-5.0f, vel.z() - dy)));
//
//			// jumping
//			if (ks[SDL_SCANCODE_X]) vel.setY(20);
//			else if (vel.y() > 10) vel.setY(10);
//
//			player.rigid_body->setLinearVelocity(vel);
//
//			if (!vel.isZero()) player.rigid_body->activate();
//		}


		// player2
		{

			player2.vel.x = dx * 0.2;
//			player2.vel.y = dy * 0.1;



			if (player2.in_air) {
				// gravity
				player2.vel.y -= 0.02;

			}
			else {

				if (dy == 1) {
					player2.vel.y = 0.7;
					player2.pos.y += 0.5;
				}
			}


			glm::vec2 dp(
				std::max( -0.5f, std::min( 0.5f, player2.vel.x)),
				std::max( -0.5f, std::min( 0.5f, player2.vel.y)));

			player2.pos += dp;


			glm::vec2 va = player2.pos;
			glm::vec2 vb = va - glm::vec2(0, player2.size.y * 0.5);

			// scan a bit further down
			if (!player2.in_air) vb.y -= 0.3;

			btCollisionWorld::ClosestRayResultCallback cb(
					btVector3(va.x, va.y, 0),
					btVector3(vb.x, vb.y, 0));
			dynamics_world->rayTest(cb.m_rayFromWorld, cb.m_rayToWorld, cb);


			if (cb.hasHit()) {
				player2.in_air = false;

				player2.vel.y = 0;
				player2.pos.y = cb.m_hitPointWorld.y() + player2.size.y * 0.5;
			}
			else {
				player2.in_air = true;
			}


			// sensor lines
			drawer.clear();
			drawer.line(va, vb, glm::vec3(1, 1, 0));
			if (cb.hasHit()) drawer.circle(glm::vec2(cb.m_hitPointWorld.x(), cb.m_hitPointWorld.y()), 0.2, glm::vec3(1, 0, 0));

		}


	}


	DebugDrawer					drawer;

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



public:

	Player		player;
	Player2		player2;


};





int main(int argc, char** argv) {


	ctx.init(800, 600);


	Scene scene;
	scene.init();



	rmw::ClearState cs;
	cs.color = glm::vec4(0.2, 0.3, 0.5, 1.0);


	rmw::RenderState rs;
	rs.line_width = 2.0;


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
				break;


			default: break;
			}
		}



		// update

		uint32_t now = SDL_GetTicks();
		scene.update((now - time) * 0.001);
		time = now;



		// render

		ctx.clear(cs);

//		glm::mat4 mvp = glm::perspectiveFov<float>(1.0, 800, 600, 1, 100)
//					  * glm::translate(glm::vec3(camera.x, camera.y, -25));

		glm::mat4 mvp = glm::perspectiveFov<float>(1.0, 800, 600, 1, 100)
					  * glm::translate(-glm::vec3(scene.player2.pos.x, scene.player2.pos.y, 25));
		scene.draw(rs, mvp);


		ctx.flip_buffers();
	}

	return 0;
}

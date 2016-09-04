#include "rmw.h"
#include "model.h"

#include <cstdlib>
#include <cstdio>
#include <vector>

#include <glm/gtx/transform.hpp>





int main(int argc, char** argv) {


	rmw::Context ctx;
	ctx.init(800, 600);


	// model
	rmw::Shader::Ptr model_shader = ctx.create_shader(
		VERTEX_SHADER(
			layout(location = 0) in vec3 in_pos;
			layout(location = 1) in vec3 in_normal;
			uniform mat4 u_mvp;
			out vec3 v_normal;
			void main() {
				gl_Position = u_mvp * vec4(in_pos, 1.0);
				v_normal = in_normal;
			}
		), FRAGMENT_SHADER(
			in vec3 v_normal;
			out vec4 out_color;
			void main() {
//				out_color = vec4(normalize(v_normal) * 0.5 + vec3(0.5), 1.0);
				out_color = vec4(
					vec3(0.5) * (0.1 + 0.9 * max(0, dot(normalize(vec3(0.3, 0.6, 2.0)), normalize(v_normal)))),
					1.0);
			}
		));


	Model model;
	model.load("media/map.model");

	rmw::VertexBuffer::Ptr model_vb = ctx.create_vertex_buffer(rmw::BufferHint::StreamDraw);
	model_vb->init_data(model.meshes[0].vertices);

	rmw::VertexArray::Ptr model_va = ctx.create_vertex_array();
	model_va->set_count(model.meshes[0].vertices.size());
	model_va->set_attribute(0, *model_vb, rmw::ComponentType::Float, 3, false,  0, sizeof(Model::Vertex));
	model_va->set_attribute(1, *model_vb, rmw::ComponentType::Float, 3, false, 12, sizeof(Model::Vertex));




	rmw::ClearState cs;
	cs.color = glm::vec4(0.2, 0.3, 0.5, 1.0);


	rmw::RenderState rs;
	rs.depth_test_func = rmw::DepthTestFunc::LEqual;
	rs.depth_test_enabled = true;


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

			default: break;
			}
		}



		ctx.clear(cs);




		static float ang = 0;
		ang += 0.005;
		glm::mat4 mvp = glm::perspectiveFov<float>(1.0, 800, 600, 1, 100)
					  * glm::translate(glm::vec3(-12 + sin(ang) * 25, -5, -20));
		model_shader->set_uniform("u_mvp", mvp);

		ctx.draw(rs, *model_shader, *model_va);


		ctx.flip_buffers();
	}

	return 0;
}

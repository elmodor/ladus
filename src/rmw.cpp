#include "rmw.h"
#include <glm/glm.hpp>
#include <GL/glew.h>


namespace rmw {


constexpr uint32_t map_to_gl(ComponentType t) {
	const uint32_t lut[] = {
		GL_BYTE, GL_UNSIGNED_BYTE,
		GL_SHORT, GL_UNSIGNED_SHORT,
		GL_INT, GL_UNSIGNED_INT,
		GL_FLOAT, GL_HALF_FLOAT,
	};
	return lut[static_cast<int>(t)];
}
constexpr uint32_t map_to_gl(BufferHint h) {
	const uint32_t lut[] = { GL_STREAM_DRAW, GL_DYNAMIC_DRAW, GL_STATIC_DRAW };
	return lut[static_cast<int>(h)];
}
constexpr uint32_t map_to_gl(DepthTestFunc dtf) {
	const uint32_t lut[] = { GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, };
	return lut[static_cast<int>(dtf)];
}
constexpr uint32_t map_to_gl(PrimitiveType pt) {
	const uint32_t lut[] = {
		GL_POINTS, GL_LINE_STRIP, GL_LINE_LOOP, GL_LINES,
		GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_TRIANGLES
	};
	return lut[static_cast<int>(pt)];
}




VertexArray::VertexArray() {
	_first = 0;
	_count = 0;
	_indexed = false;
	_primitive_type = PrimitiveType::Triangles;
	glGenVertexArrays(1, &_va);
}
VertexArray::~VertexArray() {
	glDeleteVertexArrays(1, &_va);
}


void VertexArray::set_attribute(int i, const VertexBuffer& vb, ComponentType ct, int cc, bool normalized, int offset, int stride) {
	glBindVertexArray(_va);
	vb.bind();
	glEnableVertexAttribArray(i);
	glVertexAttribPointer(i, cc, map_to_gl(ct), normalized, stride, reinterpret_cast<void*>(offset));
}
void VertexArray::set_attribute(int i, const glm::vec3& v) {
	glBindVertexArray(_va);
	glDisableVertexAttribArray(i);
	glVertexAttrib2fv(i, &v.x);
}
void VertexArray::set_attribute(int i, const glm::vec4& v) {
	glBindVertexArray(_va);
	glDisableVertexAttribArray(i);
	glVertexAttrib3fv(i, &v.x);
}

void VertexArray::set_index_buffer(const IndexBuffer& ib) {
	_indexed = true;
	glBindVertexArray(_va);
	ib.bind();
}


GpuBuffer::GpuBuffer(uint32_t target, BufferHint hint) : _target(target), _hint(hint), _size(0) {
	glGenBuffers(1, &_b);
}
GpuBuffer::~GpuBuffer() {
	glDeleteBuffers(1, &_b);
}
void GpuBuffer::bind() const {
	glBindBuffer(_target, _b);
}
void GpuBuffer::init_data(const void* data, int size) {
	_size = size;
	glBindVertexArray(0);
	bind();
	glBufferData(_target, _size, data, map_to_gl(_hint));
}

VertexBuffer::VertexBuffer(BufferHint hint) : GpuBuffer(GL_ARRAY_BUFFER, hint) {}
IndexBuffer::IndexBuffer(BufferHint hint) : GpuBuffer(GL_ELEMENT_ARRAY_BUFFER, hint) {}










// shader


static GLuint compile_shader(uint32_t type, const char* src) {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint e = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &e);
	if (e) return s;
	int len = 0;
	glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
	char log[len];
	glGetShaderInfoLog(s, len, &len, log);
	printf("error compiling s:\n%s\n", log);
	glDeleteShader(s);
	return 0;
}


bool Shader::init(const std::string& vs, const std::string& fs) {
	GLint v = compile_shader(GL_VERTEX_SHADER, vs.c_str());
	if (v == 0) return false;
	GLint f = compile_shader(GL_FRAGMENT_SHADER, fs.c_str());
	if (f == 0) {
		glDeleteShader(v);
		return false;
	}
	_program = glCreateProgram();

	glAttachShader(_program, v);
	glAttachShader(_program, f);
	glDeleteShader(v);
	glDeleteShader(f);
	glLinkProgram(_program);


	// attributes
	int count;
	glGetProgramiv(_program, GL_ACTIVE_ATTRIBUTES, &count);
	for (int i = 0; i < count; ++i) {
		char name[128];
		int size;
		uint32_t type;
		glGetActiveAttrib(_program, i, sizeof(name), nullptr, &size, &type, name);
		_attributes.push_back({ name, type, glGetAttribLocation(_program, name) });
	}

	// uniforms
	glGetProgramiv(_program, GL_ACTIVE_UNIFORMS, &count);
	for (int i = 0; i < count; ++i) {
		char name[128];
		int size; // > 1 for arrays
		uint32_t type;
		glGetActiveUniform(_program, i, sizeof(name), nullptr, &size, &type, name);
		int location = glGetUniformLocation(_program, name);
		Uniform::Ptr u;
		switch (type) {
		case GL_FLOAT:		u = std::make_unique<UniformExtend<float    >>(name, type, location); break;
		case GL_FLOAT_VEC2: u = std::make_unique<UniformExtend<glm::vec2>>(name, type, location); break;
		case GL_FLOAT_VEC3: u = std::make_unique<UniformExtend<glm::vec3>>(name, type, location); break;
		case GL_FLOAT_VEC4: u = std::make_unique<UniformExtend<glm::vec4>>(name, type, location); break;

		case GL_FLOAT_MAT4: u = std::make_unique<UniformExtend<glm::mat4>>(name, type, location); break;
		default: assert(false);
		}
		_uniforms.push_back(std::move(u));
	}

	return true;
}


Shader::~Shader() {
	glDeleteProgram(_program);
}


// glUniform* wrapper
void gl_uniform(int l, float v) { glUniform1f(l, v); }
void gl_uniform(int l, const glm::vec2& v) { glUniform2fv(l, 1, &v.x); }
void gl_uniform(int l, const glm::vec3& v) { glUniform3fv(l, 1, &v.x); }
void gl_uniform(int l, const glm::vec4& v) { glUniform4fv(l, 1, &v.x); }

void gl_uniform(int l, const glm::mat4& v) { glUniformMatrix4fv(l, 1, false, &v[0].x); }


template <class T>
void Shader::UniformExtend<T>::update() const {
	if (!dirty) return;
	dirty = false;
	gl_uniform(location, value);
}







// Context


bool Context::init(int width, int height)
{
	_width = width;
	_height = height;

	SDL_Init(SDL_INIT_VIDEO);
	IMG_Init(IMG_INIT_PNG);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	_window = SDL_CreateWindow("portals",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			_width, _height,
			SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE*/);

	SDL_GL_SetSwapInterval(1); // v-sync
	_gl_context = SDL_GL_CreateContext(_window);

	glewExperimental = true;
	glewInit();



	// initialize the reder state according to opengl's initial state
	_render_state.depth_test_enabled = 0;
	_render_state.depth_test_func = DepthTestFunc::Less;

	return true;
}

Context::~Context()
{
	SDL_GL_DeleteContext(_gl_context);
	SDL_DestroyWindow(_window);
	SDL_Quit();
	IMG_Quit();
}


void Context::clear(const ClearState& cs) {

	if (_clear_state.color != cs.color) {
		_clear_state.color = cs.color;
		glClearColor(_clear_state.color.x, _clear_state.color.y, _clear_state.color.z, _clear_state.color.w);
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}







void Context::draw(const RenderState& rs, const Shader& shader, const VertexArray& va) {

	// sync render state
	if (_render_state.depth_test_enabled != rs.depth_test_enabled) {
		_render_state.depth_test_enabled = rs.depth_test_enabled;
		if (_render_state.depth_test_enabled) {
			glEnable(GL_DEPTH_TEST);
			if (_render_state.depth_test_func != rs.depth_test_func) {
				_render_state.depth_test_func = rs.depth_test_func;
				glDepthFunc(map_to_gl(_render_state.depth_test_func));
			}
		}
		else {
			glDisable(GL_DEPTH_TEST);
		}
	}
	if (memcmp(&_render_state.viewport, &rs.viewport, sizeof(Viewport)) != 0) {
		_render_state.viewport = rs.viewport;
		glViewport( _render_state.viewport.x,
					_render_state.viewport.y,
					_render_state.viewport.w,
					_render_state.viewport.h);
	}
	if (_render_state.line_width != rs.line_width) {
		_render_state.line_width = rs.line_width;
		glLineWidth(_render_state.line_width);
	}


	// sync shader
	if (_shader != &shader) {
		_shader = &shader;
		glUseProgram(_shader->_program);
	}
	_shader->update_uniforms();


	glBindVertexArray(va._va);

	if (va._indexed) {
		glDrawElements(map_to_gl(va._primitive_type), va._count, GL_UNSIGNED_INT, reinterpret_cast<void*>(va._first));
	}
	else {
		glDrawArrays(map_to_gl(va._primitive_type), va._first, va._count);
	}

}



void Context::flip_buffers() const {
	SDL_GL_SwapWindow(_window);
}


}

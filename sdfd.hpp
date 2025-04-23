#ifndef SDFD_H_
#define SDFD_H_
#include <vector>
#include <optional>
#include <stdint.h>
#include <math.h>

#define SDFD_VERSION 0

#ifndef SDFD_DEF
#define SDFD_DEF extern
#endif

namespace sdfd {


inline float sign(float x) { return (x<0)?-1:1; }
inline float sign0(float x) { return x==0?0:(x<0)?-1:1; }


struct Vector2 {
	float x, y;

	Vector2 yx() const { return {y,x}; }

	Vector2 operator+(Vector2 b) const { return {x+b.x, y+b.y}; }
	Vector2 operator-(Vector2 b) const { return {x-b.x, y-b.y}; }
	Vector2 operator*(Vector2 b) const { return {x*b.x, y*b.y}; }
	Vector2 operator/(Vector2 b) const { return {x/b.x, y/b.y}; }
	Vector2 operator+(float b) const { return {x+b, y+b}; }
	Vector2 operator-(float b) const { return {x-b, y-b}; }
	Vector2 operator*(float b) const { return {x*b, y*b}; }
	Vector2 operator/(float b) const { return {x/b, y/b}; }
	Vector2 &operator+=(Vector2 b) { return x+=b.x, y+=b.y, *this; }
	Vector2 &operator-=(Vector2 b) { return x-=b.x, y-=b.y, *this; }
	Vector2 &operator*=(Vector2 b) { return x*=b.x, y*=b.y, *this; }
	Vector2 &operator/=(Vector2 b) { return x/=b.x, y/=b.y, *this; }
};
inline Vector2 abs(Vector2 a) { return {fabsf(a.x),fabsf(a.y)}; }
inline float dot(Vector2 a, Vector2 b) { return a.x*b.x + a.y*b.y; }
inline float length(Vector2 a) { return sqrtf(a.x*a.x + a.y*a.y); }
// rotate 90deg ccw
inline Vector2 perp(Vector2 a) { return {-a.y, a.x}; }


struct Plane {
	// This is more storage than needed as
	// only 2 floats (angle and offset along normal) are needed.
	// But the benefit is no sin/cos as they introduce
	// big floating point errors. Could use "turns" instead
	// of radians here,
	
	// Going along normal increases the distance to the plane.
	Vector2 normal;

	// How much the plane is moved along the normal.
	float offset;
};

// Constructs a plane that goes through a and b.
// If you orient yourself so that vector from a to b points up, to the right is solid and empty to the left.
SDFD_DEF Plane plane_from_points(Vector2 a, Vector2 b);

// Resulting plane goes through point and along normal is away from the plane.
SDFD_DEF Plane plane_from_point_and_normal(Vector2 point, Vector2 normal);

// Resulting plane goes through point. Normal is {cos(angle), sin(angle)}.
SDFD_DEF Plane plane_from_point_and_angle(Vector2 point, float angle);

// Normal is {cos(angle), sin(angle)}. Plane is moved by offset along the normal.
SDFD_DEF Plane plane_from_angle_and_offset(float angle, float offset);


struct Circle {
	Vector2 center;
	float radius;
};

SDFD_DEF float distance(Circle c, Vector2 p);

struct Ellipse {
	Vector2 center;
	Vector2 radius;
};

SDFD_DEF float distance(Ellipse e, Vector2 p);


/*
#define x(type, name, kind_value)
SDFD_ENUMERATE_PRIMITIVE(x)
#undef x
*/
#define SDFD_ENUMERATE_PRIMITIVE(x) \
	x(float,  float1, 0) \
	x(Plane,  plane,  4) \
	x(Circle, circle, 5) \

struct Primitive {
	enum class Kind : uint16_t {
		#define x(type, name, value) name = value,
		SDFD_ENUMERATE_PRIMITIVE(x)
		#undef x
	};

	Kind kind;
	union {
		#define x(type, name, value) type name;
		SDFD_ENUMERATE_PRIMITIVE(x)
		#undef x
	};

	Primitive() = default;
	#define x(type, name, value) Primitive(type name) : kind(Kind::name), name(name) {}
	SDFD_ENUMERATE_PRIMITIVE(x)
	#undef x
};

struct ArgumentIndex {
	struct Kind {
		inline static constexpr uint32_t object_primitive = 0;
		inline static constexpr uint32_t object_operation = 1;
	};

	uint32_t kind  : 1;
	uint32_t value : 31;
};

inline ArgumentIndex object_primitive_index(uint32_t i) { return {.kind = ArgumentIndex::Kind::object_primitive, .value = i}; }
inline ArgumentIndex object_operation_index(uint32_t i) { return {.kind = ArgumentIndex::Kind::object_operation, .value = i}; }

/*
#define x(name, value, arity)
SDFD_ENUMERATE_OPERATION(x)
#undef x
*/
#define SDFD_ENUMERATE_OPERATION(x) \
	x(min, 0, 2) /* minimum */ \
	x(max, 1, 2) /* maximum */ \
	x(neg, 2, 1) /* negate  */ \

struct Operation {
	enum class Kind : uint16_t {
		#define x(name, value, arity) name = value,
		SDFD_ENUMERATE_OPERATION(x)
		#undef x
	};

	Kind kind = {};

	// Indices into Object::primitives, Scene::primitives or Object::operations.
	// If kind is object_operation, index must be of a previous operation, otherwise
	// it will evaluate to NaN.
	ArgumentIndex args[2] = {};
};

SDFD_DEF uint32_t get_arity(Operation::Kind operation_kind);

struct Object {
	std::vector<Primitive> primitives;
	std::vector<Operation> operations;
};

struct Scene {
	std::vector<Object> objects;
	std::vector<Primitive> primitives;
	Vector2 scale = {1, 1};
};

SDFD_DEF bool store_to_file(Scene const &scene, char const *path);
SDFD_DEF std::optional<Scene> load_from_file(char const *path);

// Evaluates distance to primitive at point.
SDFD_DEF float evaluate(Scene const &scene, Primitive const &primitive, Vector2 point);

// Evaluates distance to primitives at point.
// If object contains no operations, returns distance to last object, otherwise
// if object contains no primitives, returns infinity.
SDFD_DEF float evaluate(Scene const &scene, Object const &object, Vector2 point);

}

#ifdef SDFD_IMPLEMENTATION

#include <stdio.h>
#include <assert.h>
#include <string>

namespace sdfd {

// Adapted from https://github.com/twixuss/defer
// Used only for this file in case it is used outside.
template <class Fn>
struct Defer {
    Fn fn;
    inline ~Defer() { fn(); }
};

template <class Fn>
inline static constexpr Defer<Fn> make_defer(Fn fn) { return {fn}; }

#define sdfd_defer_concat_(a, b) a ## b
#define sdfd_defer_concat(a, b) sdfd_defer_concat_(a, b)

#pragma push_macro("defer")
#undef defer
#define defer(...) auto sdfd_defer_concat(_d, __COUNTER__) = ::sdfd::make_defer([&](){__VA_ARGS__;})


float distance(Circle c, Vector2 p) {
	return length(p - c.center) - c.radius;
}
float distance(Ellipse e, Vector2 in_p) {
	// Modified
	// https://www.shadertoy.com/view/4sS3zz
	// Copyright © 2013 Inigo Quilez
	auto ab = e.radius;
	auto p = in_p - e.center;
  //if( ab.x==ab.y ) return length(p)-ab.x;

	p = abs( p ); 
    if( p.x>p.y ){ p=p.yx(); ab=ab.yx(); }
	
	float l = ab.y*ab.y - ab.x*ab.x;
	
	if (fabsf(l) < 1e-9f) {
		return distance(Circle{e.center, ab.y}, in_p);
	}

    float m = ab.x*p.x/l; 
	float n = ab.y*p.y/l; 
	float m2 = m*m;
	float n2 = n*n;
	
    float c = (m2+n2-1.0f)/3.0f; 
	float c3 = c*c*c;

    float d = c3 + m2*n2;
    float q = d  + m2*n2;
    float g = m  + m *n2;

    float co;

    if( d<0.0f )
    {
        float h = acosf(q/c3)/3.0f;
        float s = cosf(h) + 2.0f;
        float t = sinf(h) * sqrtf(3.0f);
        float rx = sqrtf( m2-c*(s+t) );
        float ry = sqrtf( m2-c*(s-t) );
        co = ry + sign0(l)*rx + fabsf(g)/(rx*ry);
    }
    else
    {
        float h = 2.0f*m*n*sqrtf(d);
        float s = sign(q+h)*powf( fabsf(q+h), 1.0f/3.0f );
        float t = sign(q-h)*powf( fabsf(q-h), 1.0f/3.0f );
        float rx = -(s+t) - c*4.0f + 2.0f*m2;
        float ry =  (s-t)*sqrtf(3.0f);
        float rm = sqrtf( rx*rx + ry*ry );
        co = ry/sqrtf(rm-rx) + 2.0f*g/rm;
    }
    co = (co-m)/2.0f;

    float si = sqrtf( std::max(1.0f-co*co,0.0f) );
 
	Vector2 r = ab * Vector2{co,si};
	
    return length(r-p) * sign(p.y-r.y);
}

Plane plane_from_point_and_normal(Vector2 point, Vector2 normal) {
	return Plane {
		.normal = normal,
		.offset = dot(point, normal),
	};
}

uint32_t get_arity(Operation::Kind operation_kind) {
	switch (operation_kind) {
		#define x(name, value, arity) case Operation::Kind::name: return arity;
		SDFD_ENUMERATE_OPERATION(x)
		#undef x
	}
	assert(!"invalid Operation::Kind in get_arity");
	return 0;
}

static std::optional<std::string> read_entire_file(char const *path) {
	std::optional<std::string> result;

	FILE *file = fopen(path, "rb");
	if (!file) {
		return result;
	}

	defer(fclose(file));

	if (fseek(file, 0, SEEK_END) != 0)
		return result;

	auto size = ftell(file);

	if (fseek(file, 0, SEEK_SET) != 0)
		return result;

	result.emplace();
	result.value().resize(size);

	if (!fread(result.value().data(), size, 1, file)) {
		result.reset();
		return result;
	}

	return result;
}

bool serialize(bool reading, Scene &scene, char const *path) {
	FILE *file = 0;
	std::string content = {};
	char *cursor = {};
	char *end = {};

	if (reading) {
		auto maybe_content = read_entire_file(path);
		if (!maybe_content)
			return {};
		content = std::move(maybe_content).value();
		cursor = content.data();
		end = content.data() + content.size();
	} else {
		file = fopen(path, "wb");
		if (!file) {
			return false;
		}
	}
	
	defer (
		if (!reading) {
			fclose(file);
		}
	);

	auto serialize_buffer = [&](void *data, uint32_t size) -> bool {
		if (reading) {
			if (cursor + size > end) {
				return false;
			}
			memcpy(data, cursor, size);
			cursor += size;
			return true;
		} else {
			return fwrite(data, size, 1, file);
		}
	};

	auto serialize_value = [&](auto &value) -> bool {
		return serialize_buffer(&value, sizeof(value));
	};

	auto serialize_primitive = [&](Primitive &primitive) {
		if (!serialize_value(primitive.kind))
			return false;
		switch (primitive.kind) {
			#define x(type, name, value)                  \
				case Primitive::Kind::name: {             \
					if (!serialize_value(primitive.name)) \
						return false;                     \
					break;                                \
				}
			SDFD_ENUMERATE_PRIMITIVE(x)
			#undef x
		}
		return true;
	};

	auto serialize_operation = [&](Operation &operation) {
		if (!serialize_value(operation.kind))
			return false;
		if (!serialize_buffer(operation.args, sizeof(operation.args[0]) * get_arity(operation.kind)))
			return false;
		return true;
	};

	#define SERIALIZE_VECTOR(object, vector) \
		{                                    \
			uint32_t size = vector.size();   \
			if (!serialize_value(size))      \
				return false;                \
			vector.resize(size);             \
		}                                    \
		for (auto &object : vector)

	std::string header_id = "sdfd";
	if (!serialize_buffer(header_id.data(), header_id.size()))
		return false;
	if (header_id != "sdfd")
		return false;

	uint16_t version = SDFD_VERSION;
	if (!serialize_value(version))
		return false;
	if (version > SDFD_VERSION)
		return false;

	SERIALIZE_VECTOR(object, scene.objects) {
		SERIALIZE_VECTOR(primitive, object.primitives) {
			if (!serialize_primitive(primitive))
				return false;
		}
		SERIALIZE_VECTOR(operation, object.operations) {
			if (!serialize_operation(operation))
				return false;
		}
	}
	
	SERIALIZE_VECTOR(primitive, scene.primitives) {
		if (!serialize_primitive(primitive))
			return false;
	}

	#undef SERIALIZE_VECTOR

	return true;
}

bool store_to_file(Scene const &scene, char const *path) {
	return serialize(false, const_cast<Scene&>(scene), path); // I promise
}
std::optional<Scene> load_from_file(char const *path) {
	std::optional<Scene> result;
	result.emplace();
	if (!serialize(true, result.value(), path)) {
		result.reset();
	}
	return result;
}

float evaluate(Scene const &scene, Primitive const &primitive, Vector2 point) {
	switch (primitive.kind) {
		case Primitive::Kind::float1: {
			return primitive.float1;
		}
		case Primitive::Kind::plane: {
			Vector2 a = primitive.plane.normal * primitive.plane.offset;
			Vector2 b = a + perp(primitive.plane.normal);

			a *= scene.scale;
			b *= scene.scale;

			Vector2 normal = perp(a - b);
			float offset = dot(a, normal);

			return dot(normal, point) - offset;
		}
		case Primitive::Kind::circle: {
			return distance(Ellipse{.center = scene.scale * primitive.circle.center, .radius = scene.scale * primitive.circle.radius}, point);
		}
		default:
			assert(!"invalid Primitive::Kind");
	}
}

float evaluate(Scene const &scene, Object const &object, Vector2 point) {
	if (object.operations.size() == 0) {
		if (object.primitives.size() == 0) {
			return std::numeric_limits<float>::infinity();
		}

		return evaluate(scene, object.primitives.back(), point);
	}

	std::vector<float> operation_results;
	operation_results.resize(object.operations.size(), std::numeric_limits<float>::quiet_NaN());

	auto evaluate_argument = [&](ArgumentIndex index) -> float {
		switch (index.kind) {
			default:
			case ArgumentIndex::Kind::object_primitive: {
				return evaluate(scene, object.primitives[index.value], point);
			}
			case ArgumentIndex::Kind::object_operation: {
				return operation_results[index.value];
			}
		}
	};

	for (std::size_t operation_index = 0; operation_index < object.operations.size(); ++operation_index) {
		auto &operation = object.operations[operation_index];

		auto evaluate_min = [&] {
			return std::min(
				evaluate_argument(operation.args[0]),
				evaluate_argument(operation.args[1])
			);
		};
		auto evaluate_max = [&] {
			return std::max(
				evaluate_argument(operation.args[0]),
				evaluate_argument(operation.args[1])
			);
		};
		auto evaluate_neg = [&] {
			return -evaluate_argument(operation.args[0]);
		};

		switch (operation.kind) {
			#define x(name, value, arity) case Operation::Kind::name: operation_results[operation_index] = evaluate_##name(); break;
			SDFD_ENUMERATE_OPERATION(x)
			#undef x

			default:
				assert(!"invalid Operation::Kind");
		}
	}
	return operation_results.back();
}


#pragma pop_macro("defer")

}

#endif

#endif

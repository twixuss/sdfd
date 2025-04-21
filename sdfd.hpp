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

struct Vector2 {
	float x, y;
	Vector2 operator-(Vector2 b) const { return {x-b.x, y-b.y}; }
};
float dot(Vector2 a, Vector2 b) { return a.x*b.x + a.y*b.y; }
float length(Vector2 a) { return sqrtf(a.x*a.x + a.y*a.y); }


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
		inline static constexpr uint32_t scene_primitive  = 1;
		inline static constexpr uint32_t object_operation = 2;
	};

	uint32_t kind  : 2;
	uint32_t value : 30;
};

inline ArgumentIndex object_primitive_index(uint32_t i) { return {.kind = ArgumentIndex::Kind::object_primitive, .value = i}; }
inline ArgumentIndex  scene_primitive_index(uint32_t i) { return {.kind = ArgumentIndex::Kind:: scene_primitive, .value = i}; }
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
};

SDFD_DEF bool store_to_file(Scene const &scene, char const *path);
SDFD_DEF std::optional<Scene> load_from_file(char const *path);

// Evaluates distance to primitive at point.
SDFD_DEF float evaluate(Primitive const &primitive, Vector2 point);

// Evaluates distance to primitives at point.
// If object contains no primitives, returns infinity.
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

float evaluate(Primitive const &primitive, Vector2 point) {
	switch (primitive.kind) {
		case Primitive::Kind::float1: {
			return primitive.float1;
		}
		case Primitive::Kind::plane: {
			return dot(primitive.plane.normal, point) - primitive.plane.offset;
		}
		case Primitive::Kind::circle: {
			return length(point - primitive.circle.center) - primitive.circle.radius;
		}
		default:
			assert(!"invalid Primitive::Kind");
	}
}

float evaluate(Scene const &scene, Object const &object, Vector2 point) {
	if (object.operations.size() == 0) {
		return std::numeric_limits<float>::infinity();
	}

	std::vector<float> operation_results;
	operation_results.resize(object.operations.size(), std::numeric_limits<float>::quiet_NaN());

	auto evaluate_argument = [&](ArgumentIndex index) -> float {
		switch (index.kind) {
			default:
			case ArgumentIndex::Kind::object_primitive: {
				return evaluate(object.primitives[index.value], point);
			}
			case ArgumentIndex::Kind::scene_primitive: {
				return evaluate(scene.primitives[index.value], point);
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

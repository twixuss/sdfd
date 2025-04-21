#define SDFD_IMPLEMENTATION
#include "../sdfd.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../dep/stb/stb_image_write.h"

#include <float.h>
#include <algorithm>

#define WIDTH  64
#define HEIGHT 64

int main(int argc, char **argv) {
	// This example shows how to create, load, serialize, and render shapes.

	sdfd::Scene scene = {};

	scene.objects.emplace_back();
	sdfd::Object *object = &scene.objects.back();

	if (argc < 2) {
		// Create shapes programmatically

		// This represents a square with a carved out circle.

		// Add primitives
	
		// Define square lines using planes
		object->primitives.push_back(sdfd::plane_from_point_and_normal({16,16}, {-1, 0}));
		object->primitives.push_back(sdfd::plane_from_point_and_normal({16,16}, {0, -1}));
		object->primitives.push_back(sdfd::plane_from_point_and_normal({48,48}, { 1, 0}));
		object->primitives.push_back(sdfd::plane_from_point_and_normal({48,48}, { 0, 1}));

		// Circle to carve out
		object->primitives.push_back(sdfd::Circle{.center={32,32}, .radius=12});
	
		// Add operations

		// Compute intersection of four planes
		object->operations.push_back({sdfd::Operation::Kind::max, {
			sdfd::object_primitive_index(0),
			sdfd::object_primitive_index(1),
		}});
		object->operations.push_back({sdfd::Operation::Kind::max, {
			sdfd::object_primitive_index(2),
			sdfd::object_primitive_index(3),
		}});
		object->operations.push_back({sdfd::Operation::Kind::max, {
			sdfd::object_operation_index(0),
			sdfd::object_operation_index(1),
		}});

		// Intersect the square with the negation of the circle.
		object->operations.push_back({sdfd::Operation::Kind::neg, {
			sdfd::object_primitive_index(4),
		}});
		object->operations.push_back({sdfd::Operation::Kind::max, {
			sdfd::object_operation_index(2),
			sdfd::object_operation_index(3),
		}});
	} else {
		// Load from file
		scene = sdfd::load_from_file(argv[1]).value();
		object = &scene.objects.at(0);
	}

	// Render the field and write to output.png
	uint32_t pixels[WIDTH*HEIGHT];
	for (int x = 0; x < WIDTH; ++x) {
		for (int y = 0; y < HEIGHT; ++y) {
			float distance = sdfd::evaluate(scene, *object, {x+0.5f, y+0.5f});
			float alpha = std::clamp(0.5f - distance, 0.0f, 1.0f);
			uint8_t c = alpha * nextafterf(256, -1);
			uint32_t pixel = 0xff000000 | c | (c << 8) | (c << 16);
			pixels[y*WIDTH + x] = pixel;
		}
	}

	stbi_write_png("output.png", WIDTH, HEIGHT, 4, pixels, WIDTH*sizeof(pixels[0]));

	// Serialize
	sdfd::store_to_file(scene, "output.sdfd");
}

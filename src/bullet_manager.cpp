#include "bullet_manager.h"

#include <godot_cpp/core/class_db.hpp>

#ifdef _WIN32
#include <malloc.h>
static void *alloc_aligned(int count, size_t element_size) {
	return _aligned_malloc(count * element_size, 16);
}
static void free_aligned(void *ptr) {
	_aligned_free(ptr);
}
#else
#include <cmath>
#include <cstdlib>
static void *alloc_aligned(int count, size_t element_size) {
	// aligned_alloc requires size to be a multiple of alignment (16)
	size_t size = (count * element_size + 15) & ~size_t(15);
	return std::aligned_alloc(16, size);
}
static void free_aligned(void *ptr) {
	std::free(ptr);
}
#endif

template <typename T>
static T *alloc_aligned_array(int count) {
	return static_cast<T *>(alloc_aligned(count, sizeof(T)));
}

template <typename T>
static void free_aligned_array(T *&ptr) {
	free_aligned(ptr);
	ptr = nullptr;
}

static void remove_bullets(int *_delete_indexes, int delete_count, int &_bullet_count,
		float *_position_x, float *_position_y, float *_velocity_x, float *_velocity_y, float *_rotation) {
	for (int j = delete_count - 1; j >= 0; j--) {
		const int idx = _delete_indexes[j];
		_bullet_count--;
		_position_x[idx] = _position_x[_bullet_count];
		_position_y[idx] = _position_y[_bullet_count];
		_velocity_x[idx] = _velocity_x[_bullet_count];
		_velocity_y[idx] = _velocity_y[_bullet_count];
		_rotation[idx] = _rotation[_bullet_count];
	}
}

namespace godot {

BulletManager::BulletManager() {
	_reallocate_arrays(_max_count);
}

BulletManager::~BulletManager() {
	_free_arrays();
}

void BulletManager::_reallocate_arrays(int p_count) {
	_free_arrays();
	if (p_count > 0) {
		_position_x = alloc_aligned_array<float>(p_count);
		_position_y = alloc_aligned_array<float>(p_count);
		_velocity_x = alloc_aligned_array<float>(p_count);
		_velocity_y = alloc_aligned_array<float>(p_count);
		_rotation = alloc_aligned_array<float>(p_count);
		_delete_indexes = alloc_aligned_array<int>(p_count);
	}
	_bullet_count = MIN(_bullet_count, p_count);
}

void BulletManager::_free_arrays() {
	free_aligned_array(_position_x);
	free_aligned_array(_position_y);
	free_aligned_array(_velocity_x);
	free_aligned_array(_velocity_y);
	free_aligned_array(_rotation);
	free_aligned_array(_delete_indexes);
}

void BulletManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &BulletManager::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &BulletManager::get_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0,1000,0.1"), "set_radius", "get_radius");

	ClassDB::bind_method(D_METHOD("set_active_area", "active_area"), &BulletManager::set_active_area);
	ClassDB::bind_method(D_METHOD("get_active_area"), &BulletManager::get_active_area);
	ADD_PROPERTY(PropertyInfo(Variant::RECT2, "active_area"), "set_active_area", "get_active_area");

	ClassDB::bind_method(D_METHOD("set_max_count", "max_count"), &BulletManager::set_max_count);
	ClassDB::bind_method(D_METHOD("get_max_count"), &BulletManager::get_max_count);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_count", PROPERTY_HINT_RANGE, "0,100000"), "set_max_count", "get_max_count");

	ClassDB::bind_method(D_METHOD("step_physics", "delta"), &BulletManager::step_physics);
	ClassDB::bind_method(D_METHOD("check_overlap", "position", "radius", "destroy_overlaps"), &BulletManager::check_overlap);
	ClassDB::bind_method(D_METHOD("check_cast", "delta", "position", "velocity", "radius", "destroy_overlaps"), &BulletManager::check_cast);
	ClassDB::bind_method(D_METHOD("clear_bullets"), &BulletManager::clear_bullets);
	ClassDB::bind_method(D_METHOD("spawn_bullet", "position", "velocity"), &BulletManager::spawn_bullet);
	ClassDB::bind_method(D_METHOD("update_multi_mesh_2d", "multi_mesh", "use_rotation"), &BulletManager::update_multi_mesh_2d);
}

void BulletManager::step_physics(float p_delta) {
	const float center_x = _active_area.position.x + _active_area.size.x * 0.5f;
	const float center_y = _active_area.position.y + _active_area.size.y * 0.5f;
	const float half_x = _active_area.size.x * 0.5f;
	const float half_y = _active_area.size.y * 0.5f;
	int delete_count = 0;
	for (int i = 0; i < _bullet_count; i++) {
		_position_x[i] += _velocity_x[i] * p_delta;
		_position_y[i] += _velocity_y[i] * p_delta;
		if (fabsf(_position_x[i] - center_x) >= half_x || fabsf(_position_y[i] - center_y) >= half_y) {
			_delete_indexes[delete_count++] = i;
		}
	}
	remove_bullets(_delete_indexes, delete_count, _bullet_count, _position_x, _position_y, _velocity_x, _velocity_y, _rotation);
}

bool BulletManager::check_overlap(Vector2 p_position, float p_radius, bool p_destroy_overlaps) {
	const float combined_radius = _radius + p_radius;
	const float combined_radius_sq = combined_radius * combined_radius;
	int delete_count = 0;
	for (int i = 0; i < _bullet_count; i++) {
		const float dx = _position_x[i] - p_position.x;
		const float dy = _position_y[i] - p_position.y;
		if (dx * dx + dy * dy < combined_radius_sq) {
			if (!p_destroy_overlaps) {
				return true;
			}
			_delete_indexes[delete_count++] = i;
		}
	}
	remove_bullets(_delete_indexes, delete_count, _bullet_count, _position_x, _position_y, _velocity_x, _velocity_y, _rotation);
	return delete_count > 0;
}

bool BulletManager::check_cast(float p_delta, Vector2 p_position, Vector2 p_velocity, float p_radius, bool p_destroy_overlaps) {
	const float combined_radius = _radius + p_radius;
	const float combined_radius_sq = combined_radius * combined_radius;
	float closest_t = p_delta;
	int delete_count = 0;
	bool hit = false;
	for (int i = 0; i < _bullet_count; i++) {
		const float dpx = p_position.x - _position_x[i];
		const float dpy = p_position.y - _position_y[i];
		const float c = dpx * dpx + dpy * dpy - combined_radius_sq;
		if (c < 0.0f) {
			if (!p_destroy_overlaps) {
				return true;
			}
			_delete_indexes[delete_count++] = i;
			hit = true;
			continue;
		}
		const float dvx = p_velocity.x - _velocity_x[i];
		const float dvy = p_velocity.y - _velocity_y[i];
		const float a = dvx * dvx + dvy * dvy;
		if (a == 0.0f) {
			continue;
		}
		const float b = 2.0f * (dpx * dvx + dpy * dvy);
		const float disc = b * b - 4.0f * a * c;
		if (disc < 0.0f) {
			continue;
		}
		const float t = (-b - sqrtf(disc)) / (2.0f * a);
		if (t >= 0.0f && t <= p_delta) {
			if (!p_destroy_overlaps) {
				if (t < closest_t) {
					closest_t = t;
					hit = true;
				}
			} else {
				_delete_indexes[delete_count++] = i;
				hit = true;
			}
		}
	}
	remove_bullets(_delete_indexes, delete_count, _bullet_count, _position_x, _position_y, _velocity_x, _velocity_y, _rotation);
	return hit;
}

void BulletManager::update_multi_mesh_2d(Ref<MultiMesh> p_multi_mesh, bool p_use_rotation) {
	p_multi_mesh->set_visible_instance_count(_bullet_count);
	for (int i = 0; i < _bullet_count; i++) {
		p_multi_mesh->set_instance_transform_2d(i, Transform2D(p_use_rotation ? _rotation[i] : 0.0f, Vector2(_position_x[i], _position_y[i])));
	}
}

void BulletManager::set_radius(float p_radius) { _radius = p_radius; }
float BulletManager::get_radius() const { return _radius; }

void BulletManager::set_active_area(const Rect2 &p_area) { _active_area = p_area; }
Rect2 BulletManager::get_active_area() const { return _active_area; }

void BulletManager::clear_bullets() { _bullet_count = 0; }

void BulletManager::spawn_bullet(Vector2 p_position, Vector2 p_velocity) {
	ERR_FAIL_COND(_bullet_count >= _max_count);
	_position_x[_bullet_count] = p_position.x;
	_position_y[_bullet_count] = p_position.y;
	_velocity_x[_bullet_count] = p_velocity.x;
	_velocity_y[_bullet_count] = p_velocity.y;
	_rotation[_bullet_count] = atan2f(p_velocity.y, p_velocity.x);
	_bullet_count++;
}

void BulletManager::set_max_count(int p_max_count) {
	if (p_max_count == _max_count) {
		return;
	}
	_max_count = p_max_count;
	_reallocate_arrays(_max_count);
}

int BulletManager::get_max_count() const { return _max_count; }

} // namespace godot

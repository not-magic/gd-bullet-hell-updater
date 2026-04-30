#pragma once

#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

class BulletManager : public Node {
	GDCLASS(BulletManager, Node)

	float _radius = 8.0f;
	Rect2 _active_area;
	int _max_count = 1000;
	int _bullet_count = 0;

	float *_position_x = nullptr;
	float *_position_y = nullptr;
	float *_velocity_x = nullptr;
	float *_velocity_y = nullptr;
	float *_rotation = nullptr;
	int *_delete_indexes = nullptr;

	void _reallocate_arrays(int p_count);
	void _free_arrays();

protected:
	static void _bind_methods();

public:
	BulletManager();
	~BulletManager();

	void step_physics(float p_delta);
	bool check_overlap(Vector2 p_position, float p_radius, bool p_destroy_overlaps);
	bool check_cast(float p_delta, Vector2 p_position, Vector2 p_velocity, float p_radius, bool p_destroy_overlaps);

	void set_radius(float p_radius);
	float get_radius() const;

	void set_active_area(const Rect2 &p_area);
	Rect2 get_active_area() const;

	void set_max_count(int p_max_count);
	int get_max_count() const;

	void clear_bullets();
	void spawn_bullet(Vector2 p_position, Vector2 p_velocity);
	void update_multi_mesh_2d(Ref<MultiMesh> p_multi_mesh, bool p_use_rotation);
};

} // namespace godot

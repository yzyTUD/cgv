#include <cgv/signal/rebind.h>
#include <cgv/base/node.h>
#include <cgv/base/register.h>
#include <cgv/math/fvec.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/trigger.h>
#include <cgv/render/drawable.h>
#include <cgv_gl/point_renderer.h>
#include <cgv_gl/surfel_renderer.h>
#include <cgv_gl/sphere_renderer.h>
#include <cgv_gl/arrow_renderer.h>
#include <cgv_gl/normal_renderer.h>
#include <cgv_gl/box_renderer.h>
#include <cgv_gl/box_wire_renderer.h>
#include <cgv_gl/gl/gl.h>
#include <random>

class renderer_tests :
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::gui::provider
{
public:
	enum RenderMode { RM_POINTS, RM_SURFELS, RM_BOXES, RM_BOX_WIRES, RM_NORMALS, RM_ARROWS, RM_SPHERES };
	struct vertex {
		vec3 point;
		vec3 normal;
		vec4 color;
	};
protected:
	std::vector<vec3> points;
	std::vector<vec3> transformed_points;
	std::vector<unsigned> group_indices;
	std::vector<vec3> normals;
	std::vector<vec4> colors;

	mat4 T;
	vec3 t;
	float lambda;

	bool sort_points;
	bool disable_depth;
	bool blend;
	bool interleaved_mode;
	std::vector<vertex> vertices;

	std::vector<vec4> group_colors;
	std::vector<vec3> group_translations;
	std::vector<vec4> group_rotations;
	std::vector<vec3> sizes;
	std::vector<unsigned> indices;
	RenderMode mode;
	cgv::render::view* view_ptr;
	bool p_vbos_out_of_date, sl_vbos_out_of_date, b_vbos_out_of_date;

	// declare render styles
	cgv::render::point_render_style point_style;
	cgv::render::surfel_render_style surfel_style;
	cgv::render::box_render_style box_style;
	cgv::render::box_wire_render_style box_wire_style;
	cgv::render::normal_render_style normal_style;
	cgv::render::arrow_render_style arrow_style;
	cgv::render::sphere_render_style sphere_style;

	// declare attribute managers
	cgv::render::attribute_array_manager p_manager;
	cgv::render::attribute_array_manager sl_manager;
	cgv::render::attribute_array_manager b_manager;
public:
	/// define format and texture filters in constructor
	renderer_tests() : cgv::base::node("renderer_test")
	{
		sort_points = false;
		blend = false;

		T.identity();
		t = vec3(-0.5f,0.0f,-0.5f);
		lambda = 1.0f;

		disable_depth = false;
		p_vbos_out_of_date = true;
		sl_vbos_out_of_date = true;
		b_vbos_out_of_date = true;
		interleaved_mode = false;
		normal_style.normal_length = 0.01f;
		// generate random geometry
		std::default_random_engine g;
		std::uniform_real_distribution<float> d(0.0f, 1.0f);
		unsigned i;
		for (i = 0; i < 10000; ++i) {
			points.push_back(vec3(d(g), d(g), d(g)));
			unsigned group_index = 0;
			if (points.back()[0] > 0.5f)
				group_index += 1;
			if (points.back()[1] > 0.5f)
				group_index += 2;
			if (points.back()[2] > 0.5f)
				group_index += 4;
			group_indices.push_back(group_index);
			normals.push_back(points.back() - vec3(0.5f, 0.5f, 0.5f));
			normals.back().normalize();
			colors.push_back(vec4(0.7f*d(g), 0.6f*d(g), 0.3f*d(g), 0.4f*d(g) + 0.6f));
			vertices.push_back(vertex());
			vertices.back().point = points.back();
			vertices.back().normal = normals.back();
			vertices.back().color = colors.back();
			sizes.push_back(vec3(0.03f*d(g) + 0.001f, 0.03f*d(g) + 0.001f, 0.03f*d(g) + 0.001f));
		}
		compute_transformed_points();
		for (i = 0; i < 8; ++i) {
			group_colors.push_back(vec4((i & 1) != 0 ? 1.0f : 0.0f, (i & 2) != 0 ? 1.0f : 0.0f, (i & 4) != 0 ? 1.0f : 0.0f, 1.0f));
			group_translations.push_back(vec3(0, 0, 0));
			group_rotations.push_back(vec4(0, 0, 0, 1));
		}
		point_style.point_size = 8;
		point_style.blend_points = false;
		point_style.blend_width_in_pixel = 0.0f;
		mode = RM_ARROWS;
		point_style.measure_point_size_in_pixel = false;
		surfel_style.point_size = 15;
		surfel_style.measure_point_size_in_pixel = false;
		surfel_style.illumination_mode = cgv::render::IM_TWO_SIDED;
		arrow_style.length_scale = 0.01f;
		sphere_style.radius = 0.01f;
		sphere_style.use_group_color = true;
	}
	std::string get_type_name() const
	{
		return "renderer_tests";
	}
	void compute_transformed_points()
	{
		transformed_points.resize(points.size());
		for (size_t i = 0; i < points.size(); ++i) {
			vec4 hp = vec4(points[i]+t, 1.0f);
			hp = T * hp;
			vec3 p = (1.0f / hp[3])*vec3(hp[0], hp[1], hp[2]);
			transformed_points[i] = (1.0f-lambda)*(points[i]+t) + lambda*p;
		}
	}
	void on_set(void* member_ptr)
	{
		if ((member_ptr >= &T && member_ptr < &T + 1) || (member_ptr >= &t && member_ptr < &t + 1) || (member_ptr == &lambda)) {
			compute_transformed_points();
			p_vbos_out_of_date = true;
		}
		if (member_ptr == &sort_points) {
			disable_depth = sort_points;
			update_member(&disable_depth);
		}
		update_member(member_ptr);
		post_redraw();
	}

	bool init(cgv::render::context& ctx)
	{
		
		if (view_ptr = find_view_as_node()) {
			view_ptr->set_focus(0.5, 0.5, 0.5);
			view_ptr->set_y_extent_at_focus(1.0);
		}
		ctx.set_bg_clr_idx(4);

		// create attribute managers
		if (!p_manager.init(ctx))
			return false;
		if (!sl_manager.init(ctx))
			return false;
		if (!b_manager.init(ctx))
			return false;

		// increase reference counts of used renderer singeltons
		cgv::render::ref_point_renderer   (ctx, 1);
		cgv::render::ref_surfel_renderer  (ctx, 1);
		cgv::render::ref_box_renderer     (ctx, 1);
		cgv::render::ref_box_wire_renderer(ctx, 1);
		cgv::render::ref_normal_renderer(ctx, 1);
		cgv::render::ref_arrow_renderer(ctx, 1);
		cgv::render::ref_sphere_renderer  (ctx, 1);
		return true;
	}

	void set_group_geometry(cgv::render::context& ctx, cgv::render::group_renderer& sr)
	{
		sr.set_group_colors(ctx, group_colors);
		sr.set_group_translations(ctx, group_translations);
		sr.set_group_rotations(ctx, group_rotations);
	}
	void set_geometry(cgv::render::context& ctx, cgv::render::group_renderer& sr)
	{
		if (interleaved_mode) {
			sr.set_position_array(ctx, &vertices.front().point, vertices.size(), sizeof(vertex));
			sr.set_color_array(ctx, &vertices.front().color, vertices.size(), sizeof(vertex));
		}
		else {
			sr.set_position_array(ctx, transformed_points);
			//sr.set_position_array(ctx, points);
			sr.set_color_array(ctx, colors);
		}
		sr.set_group_index_array(ctx, group_indices);
	}
	void draw_points()
	{
		if (sort_points) {
			indices.resize(points.size());
			for (unsigned i = 0; i < indices.size(); ++i)
				indices[i] = i;
			struct sort_pred {
				const std::vector<vec3>& points;
				const vec3& view_dir;
				bool operator () (GLint i, GLint j) const {
					return dot(points[i], view_dir) > dot(points[j], view_dir);
				}
				sort_pred(const std::vector<vec3>& _points, const vec3& _view_dir) : points(_points), view_dir(_view_dir) {}
			};
			vec3 view_dir = view_ptr->get_view_dir();
			std::sort(indices.begin(), indices.end(), sort_pred(points, view_dir));
			glDrawElements(GL_POINTS, GLsizei(points.size()), GL_UNSIGNED_INT, &indices.front());
		}
		else
			glDrawArrays(GL_POINTS, 0, GLsizei(points.size()));
	}
	void draw(cgv::render::context& ctx)
	{
		if (blend) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		if (disable_depth)
			glDepthFunc(GL_ALWAYS);
		switch (mode) {
		case RM_POINTS: {
			cgv::render::point_renderer& p_renderer = cgv::render::ref_point_renderer(ctx);
			p_renderer.set_reference_point_size(0.005f);
			p_renderer.set_y_view_angle(float(view_ptr->get_y_view_angle()));
			p_renderer.set_render_style(point_style);
			p_renderer.set_attribute_array_manager(ctx, &p_manager);
			set_group_geometry(ctx, p_renderer);
			if (p_vbos_out_of_date) {
				set_geometry(ctx, p_renderer);
				p_vbos_out_of_date = false;
			}
			p_renderer.validate_and_enable(ctx);
			draw_points();
			p_renderer.disable(ctx);
		}	break;
		case RM_SURFELS: {
			cgv::render::surfel_renderer& sl_renderer = cgv::render::ref_surfel_renderer(ctx);
			sl_renderer.set_reference_point_size(0.005f);
			sl_renderer.set_y_view_angle(float(view_ptr->get_y_view_angle()));
			sl_renderer.set_render_style(surfel_style);
			sl_renderer.set_attribute_array_manager(ctx, &sl_manager);
			set_group_geometry(ctx, sl_renderer);
			if (sl_vbos_out_of_date) {
				set_geometry(ctx, sl_renderer);
				if (interleaved_mode)
					sl_renderer.set_normal_array(ctx, &vertices.front().normal, vertices.size(), sizeof(vertex));
				else
					sl_renderer.set_normal_array(ctx, normals);
				sl_vbos_out_of_date = false;
			}
			sl_renderer.validate_and_enable(ctx);
			draw_points();
			sl_renderer.disable(ctx);
		}	break;
		case RM_BOXES: {
			cgv::render::box_renderer& b_renderer = cgv::render::ref_box_renderer(ctx);
			b_renderer.set_render_style(box_style);
			b_renderer.set_attribute_array_manager(ctx, &b_manager);
			set_group_geometry(ctx, b_renderer);
			if (b_vbos_out_of_date) {
				set_geometry(ctx, b_renderer);
				b_renderer.set_extent_array(ctx, sizes);
				b_vbos_out_of_date = false;
			}
			b_renderer.validate_and_enable(ctx);
			draw_points();
			b_renderer.disable(ctx);
		}	break;
		case RM_BOX_WIRES: {
			cgv::render::box_wire_renderer& bw_renderer = cgv::render::ref_box_wire_renderer(ctx);
			bw_renderer.set_render_style(box_wire_style);
			set_group_geometry(ctx, bw_renderer);
			set_geometry(ctx, bw_renderer);
			bw_renderer.set_extent_array(ctx, sizes);
			bw_renderer.validate_and_enable(ctx);
			draw_points();
			bw_renderer.disable(ctx);
		} break;
		case RM_NORMALS: {
			cgv::render::normal_renderer& n_renderer = cgv::render::ref_normal_renderer(ctx);
			n_renderer.set_render_style(normal_style);
			set_group_geometry(ctx, n_renderer);
			set_geometry(ctx, n_renderer);
			n_renderer.set_normal_array(ctx, normals);
			n_renderer.validate_and_enable(ctx);
			draw_points();
			n_renderer.disable(ctx);
		}	break;
		case RM_ARROWS: {
			cgv::render::arrow_renderer& a_renderer = cgv::render::ref_arrow_renderer(ctx);
			a_renderer.set_render_style(arrow_style);
			a_renderer.set_position_array(ctx, points);
			a_renderer.set_color_array(ctx, colors);
			a_renderer.set_direction_array(ctx, normals);
			if (a_renderer.validate_and_enable(ctx)) {
				glDrawArraysInstanced(GL_POINTS, 0, GLsizei(points.size()), arrow_style.nr_subdivisions);
				a_renderer.disable(ctx);
			}
		}	break;
		case RM_SPHERES: {
			cgv::render::sphere_renderer& s_renderer = cgv::render::ref_sphere_renderer(ctx);
			s_renderer.set_y_view_angle(float(view_ptr->get_y_view_angle()));
			s_renderer.set_render_style(sphere_style);

			set_group_geometry(ctx, s_renderer);
			set_geometry(ctx, s_renderer);
			s_renderer.set_radius_array(ctx, &sizes[0][0], sizes.size(), sizeof(vec3));
			s_renderer.validate_and_enable(ctx);
			draw_points();
			s_renderer.disable(ctx);
		}	break;
		}
		if (disable_depth)
			glDepthFunc(GL_LESS);
		if (blend) {
			glDisable(GL_BLEND);
		}
	}
	void clear(cgv::render::context& ctx)
	{
		// clear attribute managers
		p_manager.destruct(ctx);
		sl_manager.destruct(ctx);
		b_manager.destruct(ctx);

		// decrease reference counts of used renderer singeltons
		cgv::render::ref_point_renderer(ctx, -1);
		cgv::render::ref_surfel_renderer(ctx, -1);
		cgv::render::ref_box_renderer(ctx, -1);
		cgv::render::ref_box_wire_renderer(ctx, -1);
		cgv::render::ref_normal_renderer  (ctx, -1);
		cgv::render::ref_arrow_renderer   (ctx, -1);
		cgv::render::ref_sphere_renderer  (ctx, -1);
	}
	void create_gui()
	{
		add_decorator("renderer tests", "heading");
		add_member_control(this, "mode", mode, "dropdown", "enums='points,surfels,boxes,box wires,normals,arrows,spheres'");
		if (begin_tree_node("transformation", lambda, true)) {
			align("\a");
			add_member_control(this, "lambda", lambda, "value_slider", "min=0;max=1;ticks=true");
			add_member_control(this, "translation", t[0], "value", "w=50", " ");
			add_member_control(this, "", t[1], "value", "w=50", " ");
			add_member_control(this, "", t[2], "value", "w=50");
			add_member_control(this, "", t[0], "slider", "w=50;min=-2;max=2;ticks=true", " ");
			add_member_control(this, "", t[1], "slider", "w=50;min=-2;max=2;ticks=true", " ");
			add_member_control(this, "", t[2], "slider", "w=50;min=-2;max=2;ticks=true");
			for (unsigned i = 0; i < 4; ++i) {
				for (unsigned j = 0; j < 4; ++j)
					add_member_control(this, (i == 0 && j == 0) ? "T" : "", T(i, j), "value", "w=50", j == 3 ? "\n" : " ");
				for (unsigned j = 0; j < 4; ++j)
					add_member_control(this, "", T(i, j), "slider", "min=-1;max=1;w=50;ticks=true", j == 3 ? "\n" : " ");
			}
			align("\b");
			end_tree_node(lambda);
		}

		if (begin_tree_node("geometry and groups", mode, true)) {
			align("\a");
			add_member_control(this, "interleaved_mode", interleaved_mode, "check");
			if (begin_tree_node("group colors", group_colors, false)) {
				align("\a");
				for (unsigned i = 0; i < group_colors.size(); ++i) {
					add_member_control(this, std::string("C") + cgv::utils::to_string(i), reinterpret_cast<rgba&>(group_colors[i]));
				}
				align("\b");
				end_tree_node(group_colors);
			}
			if (begin_tree_node("group transformations", group_translations, false)) {
				align("\a");
				for (unsigned i = 0; i < group_translations.size(); ++i) {
					add_gui(std::string("T") + cgv::utils::to_string(i), group_translations[i]);
					add_gui(std::string("Q") + cgv::utils::to_string(i), group_rotations[i], "direction");
				}
				align("\b");
				end_tree_node(group_translations);
			}
			align("\b");
			end_tree_node(mode);
		}
		add_member_control(this, "sort_points", sort_points, "toggle");
		add_member_control(this, "disable_depth", disable_depth, "toggle");
		add_member_control(this, "blend", blend, "toggle");
		if (begin_tree_node("Point Rendering", point_style, false)) {
			align("\a");
			add_gui("point_style", point_style);
			align("\b");
			end_tree_node(point_style);
		}
		if (begin_tree_node("Surfel Rendering", surfel_style, false)) {
			align("\a");
			add_gui("surfel_style", surfel_style);
			align("\b");
			end_tree_node(surfel_style);
		}
		if (begin_tree_node("Box Rendering", box_style, false)) {
			align("\a");
			add_gui("box_style", box_style);
			align("\b");
			end_tree_node(box_style);
		}
		if (begin_tree_node("Box Wire Rendering", box_wire_style, false)) {
			align("\a");
			add_gui("box_wire_style", box_wire_style);
			align("\b");
			end_tree_node(box_wire_style);
		}
		if (begin_tree_node("Normal Rendering", normal_style, false)) {
			align("\a");
			add_gui("normal_style", normal_style);
			align("\b");
			end_tree_node(normal_style);
		}
		if (begin_tree_node("Arrow Rendering", arrow_style, false)) {
			align("\a");
			add_gui("arrow_style", arrow_style);
			align("\b");
			end_tree_node(arrow_style);
		}
		if (begin_tree_node("Sphere Rendering", sphere_style, false)) {
			align("\a");
			add_gui("sphere_style", sphere_style);
			align("\b");
			end_tree_node(sphere_style);
		}
	}
};

cgv::base::factory_registration<renderer_tests> fr_renderer_test("new/render/render_test", 'K', true);
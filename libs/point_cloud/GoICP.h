#pragma once
#include <vector>
#include "point_cloud.h"
#include <random>
#include <ctime>
#include <vector>
#include <cgv/math/svd.h>
#include <memory>
#include "3ddt.h"
#include "ICP.h"


#include "lib_begin.h"

namespace cgv {

	namespace pointcloud {

		struct rotation_node
		{
			float a, b, c, w;
			float ub, lb;
			int l;
			friend bool operator < (const struct rotation_node & n1, const struct rotation_node & n2)
			{
				if (n1.lb != n2.lb)
					return n1.lb > n2.lb;
				else
					return n1.w < n2.w;
			}

		};

		struct translation_node
		{
			float x, y, z, w;
			float ub, lb;
			friend bool operator < (const translation_node & n1, const translation_node & n2)
			{
				if (n1.lb != n2.lb)
					return n1.lb > n2.lb;
				else
					return n1.w < n2.w;
			}
		};

		class CGV_API GoICP : public point_cloud_types {
			typedef cgv::math::fvec<float, 3> vec3;
			typedef cgv::math::fmat<float, 4, 4> mat4;
			typedef cgv::math::fmat<float, 3, 3> mat3;
		public:
			static const int max_rot_level = 20;

			GoICP();
			void buildDistanceTransform();

			inline void set_source_cloud(const point_cloud &inputCloud) {
				source_cloud = &inputCloud;
			}
			inline void set_target_cloud(const point_cloud &inputCloud) {
				target_cloud = &inputCloud;
			};

			inline float register_pointcloud()
			{
				initialize();
				outerBnB();
				clear();

				return optimal_error;
			}

		protected:
			float icp(mat3& R_icp, vec3& t_icp);
			void initialize();
			void outerBnB();
			float innerBnB(float* maxRotDisL, translation_node* nodeTransOut);
			void clear();
		private:
			const point_cloud *source_cloud;
			const point_cloud *target_cloud;
			point_cloud temp_icp_cloud; //used in icp
			point_cloud temp_source_cloud;

			Mat rotation;
			Dir translation;
			std::vector<float> norm_data;
			DT3D distance_transform;
			ICP icp_obj;

			//temp data
			float** max_rot_dis;
			std::vector<float> min_dis;
			int icp_inlier_num;
			//vec3* point_data_temp;
			//vec3* point_data_temp_ICP;

			rotation_node init_rot_node,optimal_rot_node;
			translation_node init_trans_node, optimal_trans_node;
			mat3 optimal_rotation;
			vec3 optimal_translation;
			

			float optimal_error;
			//settings
			float mse_threshhold,sse_threshhold;
			float rotMinX,rotMinY,rotMinZ,rotWidth;
			float transMinX, transMinY, transMinZ, transWidth;
			float trim_fraction;
			float distance_transform_size;
			float distance_transform_expand_factor;
			bool do_trim;
		};

	}
}
#include <cgv/config/lib_end.h>
#include "Blupch.h"
#include "Math.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>


namespace Blu
{
	namespace Math
	{
		// Function to decompose a 4x4 transformation matrix into translation, rotation, and scale
		bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
		{
			// Alias the glm namespace to make the code cleaner
			using namespace glm;

			// Alias a float as type T for simplicity
			using T = float;

			// Create a copy of the transformation matrix
			mat4 LocalMatrix(transform);

			// If the homogeneous coordinate is 0, return false because the matrix cannot be decomposed
			if (epsilonEqual(LocalMatrix[3][3], static_cast<float>(0), epsilon<T>()))
				return false;

			// If any of the elements in the rightmost column (not including homogeneous coordinate) are close to zero,
			// set them to zero and set the homogeneous coordinate to 1.
			if (
				epsilonEqual(LocalMatrix[0][3], static_cast<T>(0), epsilon<T>()) ||
				epsilonEqual(LocalMatrix[1][3], static_cast<T>(0), epsilon<T>()) ||
				epsilonEqual(LocalMatrix[2][3], static_cast<T>(0), epsilon<T>())
				)
			{
				LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = static_cast<T>(0);
				LocalMatrix[3][3] = static_cast<T>(1);
			}

			// Extract the translation vector from the matrix
			translation = vec3(LocalMatrix[3]);
			// Set the translation elements in the matrix to zero
			LocalMatrix[3] = vec4(0, 0, 0, LocalMatrix[3].w);

			vec3 Row[3], Pdum3;

			// Fill the Row array with the 3x3 rotation and scale matrix
			for (length_t i = 0; i < 3; i++)
			{
				for (length_t j = 0; j < 3; j++)
				{
					Row[i][j] = LocalMatrix[i][j];

				}
			}

			// Compute the scale and normalize the rows
			scale.x = length(Row[0]);
			Row[0] = detail::scale(Row[0], static_cast<T>(1));
			scale.y = length(Row[1]);
			Row[1] = detail::scale(Row[1], static_cast<T>(1));
			scale.z = length(Row[2]);
			Row[2] = detail::scale(Row[2], static_cast<T>(1));

			// Compute the rotation angles based on the normalized matrix
			rotation.y = asin(-Row[0][2]);
			if (cos(rotation.y) != 0)
			{
				rotation.x = atan2(Row[1][2], Row[2][2]);
				rotation.z = atan2(Row[0][1], Row[0][0]);
			}
			else
			{
				rotation.x = atan2(-Row[2][0], Row[1][1]);
				rotation.z = 0;
			}

			// Return true if the decomposition was successful
			return true;
		}

	}
}
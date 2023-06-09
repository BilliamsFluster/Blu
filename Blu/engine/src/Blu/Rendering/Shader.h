#pragma once

namespace Blu
{
	class Shader
	{
	public:

		virtual ~Shader() = default;

		static Shader* Create(const std::string& vertexSrc, const std::string& fragmentSrc);
		virtual void Bind() const = 0;
		virtual void UnBind() const = 0;

	};

}



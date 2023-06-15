#pragma once
#include "Blu/Core/Core.h"
#include <unordered_map>

namespace Blu
{
	class Shader
	{
	public:

		virtual ~Shader() = default;

		static Shared<Shader> Create(const std::string& filepath);
		static Shared<Shader> Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		virtual void Bind() const = 0;
		virtual void UnBind() const = 0;
		virtual const std::string& GetName() const = 0;

	};
	class ShaderLibrary
	{
	public:
		void Add(const Shared<Shader>& shader);
		void Add(const std::string& name, const Shared<Shader>& shader);
		Shared<Shader>Load(const std::string& filepath);
		Shared<Shader>Load(std::string name, const std::string& filepath);
		Shared<Shader> Get(const std::string& name);


	private:
		std::unordered_map<std::string, Shared<Shader>> m_Shaders;
	};
}



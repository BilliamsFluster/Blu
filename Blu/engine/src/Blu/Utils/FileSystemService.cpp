#include "Blupch.h"
#include "FileSystemService.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Blu
{
	namespace
	{
		std::filesystem::path FindProjectRoot()
		{
			std::error_code error;
			auto current = std::filesystem::weakly_canonical(std::filesystem::current_path(error), error);
			if (error)
				current = std::filesystem::path(".");

			for (auto cursor = current; !cursor.empty(); cursor = cursor.parent_path())
			{
				if (std::filesystem::exists(cursor / "Blu.sln"))
					return cursor;
				if (cursor == cursor.root_path())
					break;
			}
			return current;
		}
	}

	FileSystemService& FileSystemService::Get()
	{
		static FileSystemService instance;
		return instance;
	}

	FileSystemService::FileSystemService()
	{
		MountDefaults(FindProjectRoot());
	}

	bool FileSystemService::Mount(std::string_view name, const std::filesystem::path& root)
	{
		const std::string normalizedName = NormalizeMountName(name);
		if (normalizedName.empty() || root.empty())
			return false;

		m_Mounts[normalizedName] = Canonicalize(root);
		return true;
	}

	void FileSystemService::MountDefaults(const std::filesystem::path& projectRoot)
	{
		Mount("project", projectRoot);
		Mount("engine", projectRoot / "Blu" / "engine");
		Mount("editor", projectRoot / "Blu-Editor");
		Mount("cache", projectRoot / ".cache");
	}

	void FileSystemService::Reset()
	{
		m_Mounts.clear();
	}

	bool FileSystemService::IsVirtualPath(const std::filesystem::path& path) const
	{
		return path.generic_string().find("://") != std::string::npos;
	}

	std::filesystem::path FileSystemService::Resolve(const std::filesystem::path& virtualPath) const
	{
		const std::string value = virtualPath.generic_string();
		const size_t separator = value.find("://");
		if (separator == std::string::npos)
			return {};

		const std::string mountName = NormalizeMountName(value.substr(0, separator));
		auto mount = m_Mounts.find(mountName);
		if (mount == m_Mounts.end())
			return {};

		const std::filesystem::path relativePath = value.substr(separator + 3);
		if (relativePath.is_absolute())
			return {};
		for (const auto& part : relativePath)
		{
			if (part == "..")
				return {};
		}

		const std::filesystem::path resolved = Canonicalize(mount->second / relativePath);
		return IsUnder(resolved, mount->second) ? resolved : std::filesystem::path();
	}

	std::string FileSystemService::ToVirtualPath(const std::filesystem::path& path, std::string_view preferredMount) const
	{
		if (path.empty())
			return {};
		if (IsVirtualPath(path))
			return path.generic_string();

		const std::filesystem::path canonicalPath = Canonicalize(path);
		auto buildVirtualPath = [&](const auto& mount) -> std::string
		{
			if (mount == m_Mounts.end() || !IsUnder(canonicalPath, mount->second))
				return {};
			std::error_code error;
			const auto relative = std::filesystem::relative(canonicalPath, mount->second, error);
			if (error)
				return {};
			return mount->first + "://" + relative.generic_string();
		};

		if (!preferredMount.empty())
		{
			const std::string result = buildVirtualPath(m_Mounts.find(NormalizeMountName(preferredMount)));
			if (!result.empty())
				return result;
		}

		for (auto mount = m_Mounts.begin(); mount != m_Mounts.end(); ++mount)
		{
			const std::string result = buildVirtualPath(mount);
			if (!result.empty())
				return result;
		}
		return {};
	}

	bool FileSystemService::Read(const std::filesystem::path& virtualPath, std::string& outContents) const
	{
		const std::filesystem::path resolved = Resolve(virtualPath);
		if (resolved.empty())
			return false;

		std::ifstream input(resolved, std::ios::binary);
		if (!input)
			return false;
		std::stringstream contents;
		contents << input.rdbuf();
		outContents = contents.str();
		return true;
	}

	bool FileSystemService::Write(const std::filesystem::path& virtualPath, std::string_view contents) const
	{
		const std::filesystem::path resolved = Resolve(virtualPath);
		if (resolved.empty())
			return false;

		std::error_code error;
		std::filesystem::create_directories(resolved.parent_path(), error);
		if (error)
			return false;

		std::ofstream output(resolved, std::ios::binary);
		if (!output)
			return false;
		output.write(contents.data(), (std::streamsize)contents.size());
		return output.good();
	}

	bool FileSystemService::Exists(const std::filesystem::path& virtualPath) const
	{
		std::error_code error;
		const std::filesystem::path resolved = Resolve(virtualPath);
		return !resolved.empty() && std::filesystem::exists(resolved, error);
	}

	std::string FileSystemService::NormalizeMountName(std::string_view name)
	{
		std::string normalized(name);
		const size_t separator = normalized.find("://");
		if (separator != std::string::npos)
			normalized.resize(separator);
		std::transform(normalized.begin(), normalized.end(), normalized.begin(),
			[](unsigned char character) { return (char)std::tolower(character); });
		return normalized;
	}

	std::filesystem::path FileSystemService::Canonicalize(const std::filesystem::path& path)
	{
		std::error_code error;
		auto canonical = std::filesystem::weakly_canonical(path, error);
		if (!error)
			return canonical.lexically_normal();
		return std::filesystem::absolute(path, error).lexically_normal();
	}

	bool FileSystemService::IsUnder(const std::filesystem::path& child, const std::filesystem::path& root)
	{
		const auto canonicalChild = Canonicalize(child);
		const auto canonicalRoot = Canonicalize(root);
		auto childPart = canonicalChild.begin();
		auto rootPart = canonicalRoot.begin();
		for (; rootPart != canonicalRoot.end(); ++rootPart, ++childPart)
		{
			if (childPart == canonicalChild.end())
				return false;
#ifdef BLU_PLATFORM_WINDOWS
			std::string left = childPart->generic_string();
			std::string right = rootPart->generic_string();
			std::transform(left.begin(), left.end(), left.begin(), [](unsigned char character) { return (char)std::tolower(character); });
			std::transform(right.begin(), right.end(), right.begin(), [](unsigned char character) { return (char)std::tolower(character); });
			if (left != right)
				return false;
#else
			if (*childPart != *rootPart)
				return false;
#endif
		}
		return true;
	}
}

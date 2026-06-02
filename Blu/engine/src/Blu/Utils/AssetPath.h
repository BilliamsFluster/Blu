#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace Blu::AssetPath
{
	inline std::string ToForwardSlashes(std::string path)
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		return path;
	}

	inline std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](unsigned char c) { return (char)std::tolower(c); });
		return value;
	}

	inline std::filesystem::path WeaklyCanonical(const std::filesystem::path& path)
	{
		std::error_code ec;
		auto canonical = std::filesystem::weakly_canonical(path, ec);
		if (!ec)
			return canonical.lexically_normal();

		return std::filesystem::absolute(path, ec).lexically_normal();
	}

	inline std::filesystem::path ProjectRoot()
	{
		std::error_code ec;
		auto current = std::filesystem::current_path(ec);
		if (ec)
			current = std::filesystem::path(".");

		current = WeaklyCanonical(current);
		for (auto cursor = current; !cursor.empty(); cursor = cursor.parent_path())
		{
			if (std::filesystem::exists(cursor / "Blu.sln"))
				return cursor;
			if (cursor == cursor.root_path())
				break;
		}

		return current;
	}

	inline std::filesystem::path AssetsRoot()
	{
		return ProjectRoot() / "assets";
	}

	inline std::string NormalizePath(const std::filesystem::path& path)
	{
		return ToForwardSlashes(path.lexically_normal().generic_string());
	}

	inline bool IsUnder(const std::filesystem::path& child, const std::filesystem::path& root)
	{
		auto childString = ToLower(ToForwardSlashes(WeaklyCanonical(child).generic_string()));
		auto rootString = ToLower(ToForwardSlashes(WeaklyCanonical(root).generic_string()));
		if (!rootString.empty() && rootString.back() != '/')
			rootString.push_back('/');

		return childString == rootString.substr(0, rootString.size() - 1)
			|| childString.rfind(rootString, 0) == 0;
	}

	inline std::string ToProjectRelative(const std::filesystem::path& path)
	{
		if (path.empty())
			return {};

		std::error_code ec;
		auto projectRoot = ProjectRoot();
		auto input = path;

		if (input.is_absolute())
		{
			auto canonical = WeaklyCanonical(input);
			if (IsUnder(canonical, projectRoot))
				return NormalizePath(std::filesystem::relative(canonical, projectRoot, ec));

			return NormalizePath(canonical);
		}

		return NormalizePath(input);
	}

	inline std::filesystem::path ResolvePath(const std::filesystem::path& path, const std::filesystem::path& scenePath = {})
	{
		if (path.empty())
			return {};

		std::error_code ec;
		if (path.is_absolute())
			return WeaklyCanonical(path);

		auto projectCandidate = (ProjectRoot() / path).lexically_normal();
		if (std::filesystem::exists(projectCandidate, ec))
			return WeaklyCanonical(projectCandidate);

		if (!scenePath.empty())
		{
			auto sceneCandidate = (scenePath.parent_path() / path).lexically_normal();
			if (std::filesystem::exists(sceneCandidate, ec))
				return WeaklyCanonical(sceneCandidate);
		}

		return WeaklyCanonical(projectCandidate);
	}

	inline bool Exists(const std::filesystem::path& path, const std::filesystem::path& scenePath = {})
	{
		std::error_code ec;
		return !path.empty() && std::filesystem::exists(ResolvePath(path, scenePath), ec);
	}

	inline bool IsExternal(const std::filesystem::path& path)
	{
		if (path.empty())
			return false;

		auto resolved = path.is_absolute() ? WeaklyCanonical(path) : WeaklyCanonical(ProjectRoot() / path);
		return !IsUnder(resolved, ProjectRoot());
	}

	inline bool IsImported(const std::string& projectRelativePath)
	{
		return ToLower(ToForwardSlashes(projectRelativePath)).rfind("assets/imports/", 0) == 0;
	}

	inline std::string SanitizeName(std::string_view value, std::string_view fallback = "Asset")
	{
		std::string result;
		result.reserve(value.size());
		for (char c : value)
		{
			unsigned char uc = (unsigned char)c;
			if (std::isalnum(uc) || c == '-' || c == '_')
				result.push_back(c);
			else if (std::isspace(uc) || c == '.')
				result.push_back('_');
		}

		while (!result.empty() && result.front() == '_')
			result.erase(result.begin());
		while (!result.empty() && result.back() == '_')
			result.pop_back();

		return result.empty() ? std::string(fallback) : result;
	}

	inline std::string CopyExternalAssetToProject(
		const std::filesystem::path& source,
		std::string_view category,
		std::string_view ownerName)
	{
		if (source.empty())
			return {};

		auto resolvedSource = ResolvePath(source);
		if (IsUnder(resolvedSource, AssetsRoot()))
			return ToProjectRelative(resolvedSource);

		auto owner = SanitizeName(ownerName, source.stem().string());
		auto destination = AssetsRoot() / "imports" / category / owner / source.filename();

		std::error_code ec;
		std::filesystem::create_directories(destination.parent_path(), ec);
		std::filesystem::copy_file(resolvedSource, destination, std::filesystem::copy_options::overwrite_existing, ec);

		if (ec)
			return ToProjectRelative(resolvedSource);

		return ToProjectRelative(destination);
	}

	inline std::string ImportModelPath(const std::filesystem::path& source)
	{
		if (source.empty())
			return {};

		auto resolvedSource = ResolvePath(source);
		if (IsUnder(resolvedSource, AssetsRoot()))
			return ToProjectRelative(resolvedSource);

		auto owner = SanitizeName(source.stem().string(), "Model");
		auto destination = AssetsRoot() / "imports" / "models" / owner / source.filename();

		std::error_code ec;
		std::filesystem::create_directories(destination.parent_path(), ec);
		std::filesystem::copy_file(resolvedSource, destination, std::filesystem::copy_options::overwrite_existing, ec);
		if (ec)
			return ToProjectRelative(resolvedSource);

		auto copySidecar = [&](const std::filesystem::path& sidecar)
		{
			std::error_code copyError;
			std::filesystem::copy_file(
				sidecar,
				destination.parent_path() / sidecar.filename(),
				std::filesystem::copy_options::overwrite_existing,
				copyError);
		};

		auto sourceDir = resolvedSource.parent_path();
		for (std::filesystem::directory_iterator it(sourceDir, ec), end; !ec && it != end; it.increment(ec))
		{
			if (!it->is_regular_file(ec))
				continue;

			auto extension = ToLower(it->path().extension().string());
			if (extension != ".mtl" && extension != ".bin")
				continue;

			copySidecar(it->path());
		}

		return ToProjectRelative(destination);
	}

	inline std::string ImportTexturePath(const std::filesystem::path& source, std::string_view ownerName)
	{
		return CopyExternalAssetToProject(source, "textures", ownerName);
	}
}

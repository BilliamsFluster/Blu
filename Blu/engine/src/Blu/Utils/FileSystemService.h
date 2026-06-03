#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace Blu
{
	class FileSystemService
	{
	public:
		static FileSystemService& Get();

		bool Mount(std::string_view name, const std::filesystem::path& root);
		void MountDefaults(const std::filesystem::path& projectRoot);
		void Reset();

		bool IsVirtualPath(const std::filesystem::path& path) const;
		std::filesystem::path Resolve(const std::filesystem::path& virtualPath) const;
		std::string ToVirtualPath(const std::filesystem::path& path, std::string_view preferredMount = {}) const;
		bool Read(const std::filesystem::path& virtualPath, std::string& outContents) const;
		bool Write(const std::filesystem::path& virtualPath, std::string_view contents) const;
		bool Exists(const std::filesystem::path& virtualPath) const;

		const std::map<std::string, std::filesystem::path>& GetMounts() const { return m_Mounts; }

	private:
		FileSystemService();

		static std::string NormalizeMountName(std::string_view name);
		static std::filesystem::path Canonicalize(const std::filesystem::path& path);
		static bool IsUnder(const std::filesystem::path& child, const std::filesystem::path& root);

		std::map<std::string, std::filesystem::path> m_Mounts;
	};
}

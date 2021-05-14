#pragma once

#include "IG_Config.h"

namespace IG {
class SharedLibrary {
public:
	SharedLibrary();
	SharedLibrary(const std::filesystem::path& file);
	~SharedLibrary();

	void* symbol(const std::string& name) const;
	void unload();

	inline operator bool() const { return mInternal != nullptr; }

private:
	std::shared_ptr<struct SharedLibraryInternal> mInternal;
};
} // namespace IG
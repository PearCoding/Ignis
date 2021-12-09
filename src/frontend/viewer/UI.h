#pragma once

#include "Camera.h"
#include "DebugMode.h"

#include <memory>

namespace IG {
enum class ToneMappingMethod {
	None = 0,
	Reinhard,
	ModifiedReinhard,
	ACES
};

class UI {
public:
	UI(int width, int height, const std::vector<const float*>& aovs, bool showDebug);
	~UI();

	void setTitle(const char* str);
	bool handleInput(uint32_t& iter, bool& run, Camera& cam);
	void update(uint32_t iter, uint32_t samplesPerIteration);

	inline DebugMode currentDebugMode() const { return mDebugMode; }
	inline ToneMappingMethod currentToneMappingMethod() const { return mToneMappingMethod; }
	inline int currentAOV() const { return mCurrentAOV; }

private:
	void changeAOV(int delta_aov);
	inline const float* currentPixels() const { return mAOVs[mCurrentAOV]; }

	int mWidth;
	int mHeight;
	std::vector<const float*> mAOVs;
	int mCurrentAOV;

	const bool mShowDebug;
	DebugMode mDebugMode;

	ToneMappingMethod mToneMappingMethod;

	friend class UIInternal;
	std::unique_ptr<class UIInternal> mInternal;
};
} // namespace IG
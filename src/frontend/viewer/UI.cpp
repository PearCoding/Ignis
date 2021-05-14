#include "UI.h"
#include "Pose.h"

#include <SDL.h>

#include "imgui.h"
#include "imgui_markdown.h"
#include "imgui_sdl.h"

#include "Color.h"
#include "Image.h"
#include "Logger.h"

#include <atomic>
#include <execution>

namespace IG {
namespace UI {

#define CULL_BAD_COLOR
#define CATCH_BAD_COLOR
#define USE_MEDIAN_FOR_LUMINANCE_ESTIMATION

static SDL_Window* sWindow;
static SDL_Renderer* sRenderer;
static SDL_Texture* sTexture;
static std::vector<uint32_t> sBuffer;
static int sWidth;
static int sHeight;
static const float* sPixels;

struct LuminanceInfo {
	std::atomic<float> Min = FltInf;
	std::atomic<float> Max = 0.0f;
	float Avg			   = 0.0f;
	float Est			   = 0.000001f;

	inline LuminanceInfo& operator=(const LuminanceInfo& other)
	{
		Min = other.Min.load();
		Max = other.Max.load();
		Avg = other.Avg;
		Est = other.Est;
		return *this;
	}
};

static int sPoseRequest		   = -1;
static bool sPoseResetRequest  = false;
static bool sScreenshotRequest = false;
static bool sShowHelp		   = false;
static bool sShowUI			   = true;
static bool sLockInteraction   = false;

// Stats
static LuminanceInfo sLastLum;
constexpr size_t HISTOGRAM_SIZE = 100;
static std::array<std::atomic<uint32>, HISTOGRAM_SIZE> sHistogram;
static std::array<float, HISTOGRAM_SIZE> sHistogramF;

static bool sToneMapping_Automatic = true;
static float sToneMapping_Exposure = 1.0f;
static float sToneMapping_Offset   = 0.0f;

static bool sRunning = true;

enum class ToneMappingMethod {
	None = 0,
	Reinhard,
	ModifiedReinhard,
	ACES
};
static const char* ToneMappingMethodOptions[] = {
	"None", "Reinhard", "Mod. Reinhard", "ACES"
};
static const char* sToneMapping_Method = ToneMappingMethodOptions[(int)ToneMappingMethod::ACES];
static bool sToneMappingGamma		   = false;

static DebugMode sCurrentDebugMode	  = DebugMode::Normal;
static const char* DebugModeOptions[] = {
	"Normal", "Tangent", "Bitangent", "Geom. Norm.", "Tex. Coords", "UV Coords", "Point", "Hit Dist.",
	"Raw Prim ID", "Prim ID", "Raw Entity ID", "Entity ID",
	"Is Emissive", "Is Specular", "Is Entering", "Check BSDF"
};
static const char* sDebugMode_Method = DebugModeOptions[(int)sCurrentDebugMode];
static bool sShowDebugMode			 = false;

// Pose IO
constexpr char POSE_FILE[] = "poses.lst";
static PoseManager sPoseManager;
CameraPose sLastCameraPose;

// Events
static void handle_pose_input(size_t posenmbr, bool capture, const Camera& cam)
{
	if (!capture) {
		sPoseRequest = posenmbr;
	} else {
		sPoseManager.setPose(posenmbr, CameraPose(cam));
		IG_LOG(L_INFO) << "Captured pose for " << posenmbr + 1 << std::endl;
	}
}

// Events
static bool handle_events(uint32_t& iter, bool& run, Camera& cam)
{
	static bool first_call = true;
	if (first_call) {
		sPoseManager.setInitalPose(CameraPose(cam));
		first_call = false;
	}

	ImGuiIO& io = ImGui::GetIO();

	static bool camera_on			   = false;
	static std::array<bool, 12> arrows = { false, false, false, false, false, false, false, false, false, false, false, false };
	static bool speed[2]			   = { false, false };
	const float rspeed				   = 0.005f;
	static float tspeed				   = 0.1f;

	const bool canInteract = !sLockInteraction && run;

	SDL_Event event;
	const bool hover = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
	while (SDL_PollEvent(&event)) {
		bool key_down = event.type == SDL_KEYDOWN;
		switch (event.type) {
		case SDL_TEXTINPUT:
			io.AddInputCharactersUTF8(event.text.text);
			break;
		case SDL_KEYUP:
		case SDL_KEYDOWN: {
			int key = event.key.keysym.scancode;
			IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
			io.KeysDown[key] = (event.type == SDL_KEYDOWN);
			io.KeyShift		 = ((SDL_GetModState() & KMOD_SHIFT) != 0);
			io.KeyCtrl		 = ((SDL_GetModState() & KMOD_CTRL) != 0);
			io.KeyAlt		 = ((SDL_GetModState() & KMOD_ALT) != 0);
#ifdef _WIN32
			io.KeySuper = false;
#else
			io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
#endif

			switch (event.key.keysym.sym) {
			case SDLK_ESCAPE:
				return true;
			case SDLK_KP_PLUS:
				speed[0] = key_down;
				break;
			case SDLK_KP_MINUS:
				speed[1] = key_down;
				break;
			case SDLK_UP:
				arrows[0] = key_down;
				break;
			case SDLK_w:
				arrows[0] = key_down;
				break;
			case SDLK_DOWN:
				arrows[1] = key_down;
				break;
			case SDLK_s:
				arrows[1] = key_down;
				break;
			case SDLK_LEFT:
				arrows[2] = key_down;
				break;
			case SDLK_a:
				arrows[2] = key_down;
				break;
			case SDLK_RIGHT:
				arrows[3] = key_down;
				break;
			case SDLK_d:
				arrows[3] = key_down;
				break;
			case SDLK_e:
				arrows[4] = key_down;
				break;
			case SDLK_q:
				arrows[5] = key_down;
				break;
			case SDLK_PAGEUP:
				arrows[6] = key_down;
				break;
			case SDLK_PAGEDOWN:
				arrows[7] = key_down;
				break;
			case SDLK_KP_2:
				arrows[8] = key_down;
				break;
			case SDLK_KP_8:
				arrows[9] = key_down;
				break;
			case SDLK_KP_4:
				arrows[10] = key_down;
				break;
			case SDLK_KP_6:
				arrows[11] = key_down;
				break;
			}

			// Followings should only be handled once
			if (event.type == SDL_KEYUP) {
				const bool capture = io.KeyCtrl;
				if (canInteract) {
					switch (event.key.keysym.sym) {
					case SDLK_KP_1:
						cam.update_dir(Vector3f(0, 0, 1), Vector3f(0, 1, 0));
						iter = 0;
						break;
					case SDLK_KP_3:
						cam.update_dir(Vector3f(1, 0, 0), Vector3f(0, 1, 0));
						iter = 0;
						break;
					case SDLK_KP_7:
						cam.update_dir(Vector3f(0, 1, 0), Vector3f(0, 0, 1));
						iter = 0;
						break;
					case SDLK_KP_9:
						cam.update_dir(-cam.Direction, cam.Up);
						iter = 0;
						break;
					case SDLK_1:
						handle_pose_input(0, capture, cam);
						break;
					case SDLK_2:
						handle_pose_input(1, capture, cam);
						break;
					case SDLK_3:
						handle_pose_input(2, capture, cam);
						break;
					case SDLK_4:
						handle_pose_input(3, capture, cam);
						break;
					case SDLK_5:
						handle_pose_input(4, capture, cam);
						break;
					case SDLK_6:
						handle_pose_input(5, capture, cam);
						break;
					case SDLK_7:
						handle_pose_input(6, capture, cam);
						break;
					case SDLK_8:
						handle_pose_input(7, capture, cam);
						break;
					case SDLK_9:
						handle_pose_input(8, capture, cam);
						break;
					case SDLK_0:
						handle_pose_input(9, capture, cam);
						break;
					case SDLK_r:
						sPoseResetRequest = true;
						break;
					}
				}
				switch (event.key.keysym.sym) {
				case SDLK_t:
					sToneMapping_Automatic = !sToneMapping_Automatic;
					break;
				case SDLK_g:
					if (!sToneMapping_Automatic) {
						sToneMapping_Exposure = 1.0f;
						sToneMapping_Offset	  = 0.0f;
					}
					break;
				case SDLK_f:
					if (!sToneMapping_Automatic) {
						const float delta = io.KeyCtrl ? 0.05f : 0.5f;
						sToneMapping_Exposure += io.KeyShift ? -delta : delta;
					}
					break;
				case SDLK_v:
					if (!sToneMapping_Automatic) {
						const float delta = io.KeyCtrl ? 0.05f : 0.5f;
						sToneMapping_Offset += io.KeyShift ? -delta : delta;
					}
					break;
				case SDLK_p:
					run = !run;
					break;
				case SDLK_F1:
					sShowHelp = !sShowHelp;
					break;
				case SDLK_F2:
					sShowUI = !sShowUI;
					break;
				case SDLK_F3:
					sLockInteraction = !sLockInteraction;
					break;
				case SDLK_F11:
					sScreenshotRequest = true;
					break;
				}
			}
		} break;
		case SDL_MOUSEBUTTONDOWN:
			if (event.button.button == SDL_BUTTON_LEFT && !hover && canInteract) {
#ifndef IG_DEBUG
				SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
				camera_on = true;
			}
			break;
		case SDL_MOUSEBUTTONUP:
#ifndef IG_DEBUG
			SDL_SetRelativeMouseMode(SDL_FALSE);
#endif
			camera_on = false;
			break;
		case SDL_MOUSEMOTION:
			if (camera_on && !hover && canInteract) {
				cam.rotate(event.motion.xrel * rspeed, event.motion.yrel * rspeed);
				iter = 0;
			}
			break;
		case SDL_MOUSEWHEEL:
			if (event.wheel.x > 0)
				io.MouseWheelH += 1;
			if (event.wheel.x < 0)
				io.MouseWheelH -= 1;
			if (event.wheel.y > 0)
				io.MouseWheel += 1;
			if (event.wheel.y < 0)
				io.MouseWheel -= 1;
			break;
		case SDL_QUIT:
			return true;
		default:
			break;
		}
	}

	int mouseX, mouseY;
	const int buttons = SDL_GetMouseState(&mouseX, &mouseY);

	// Setup low-level inputs (e.g. on Win32, GetKeyboardState(), or write to those fields from your Windows message loop handlers, etc.)
	io.DeltaTime	= 1.0f / 60.0f;
	io.MousePos		= ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
	io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
	io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);

	if (run) {
		if (!sLockInteraction) {
			for (bool b : arrows) {
				if (b) {
					iter = 0;
					break;
				}
			}

			const float drspeed = 10 * rspeed;
			if (arrows[0])
				cam.move(0, 0, tspeed);
			if (arrows[1])
				cam.move(0, 0, -tspeed);
			if (arrows[2])
				cam.move(-tspeed, 0, 0);
			if (arrows[3])
				cam.move(tspeed, 0, 0);
			if (arrows[4])
				cam.roll(drspeed);
			if (arrows[5])
				cam.roll(-drspeed);
			if (arrows[6])
				cam.move(0, tspeed, 0);
			if (arrows[7])
				cam.move(0, -tspeed, 0);
			if (arrows[8])
				cam.rotate(0, drspeed);
			if (arrows[9])
				cam.rotate(0, -drspeed);
			if (arrows[10])
				cam.rotate(-drspeed, 0);
			if (arrows[11])
				cam.rotate(drspeed, 0);
			if (speed[0])
				tspeed *= 1.1f;
			if (speed[1])
				tspeed *= 0.9f;
		}

		if (sPoseResetRequest || sPoseRequest >= 0) {
			auto pose = sPoseResetRequest ? sPoseManager.initialPose() : sPoseManager.pose(sPoseRequest);
			cam.Eye	  = pose.Eye;
			cam.update_dir(pose.Dir, pose.Up);
			iter			  = 0;
			sPoseRequest	  = -1;
			sPoseResetRequest = false;
		}
	}

	sLastCameraPose = CameraPose(cam);
	sRunning		= run;

	return false;
}

// ToneMapping
#define RGB_C(r, g, b) (((r) << 16) | ((g) << 8) | (b))

static inline RGB xyz_to_srgb(const RGB& c)
{
	return RGB(3.2404542f * c.r - 1.5371385f * c.g - 0.4985314f * c.b,
			   -0.9692660f * c.r + 1.8760108f * c.g + 0.0415560f * c.b,
			   0.0556434f * c.r - 0.2040259f * c.g + 1.0572252f * c.b);
}

static inline RGB srgb_to_xyz(const RGB& c)
{
	return RGB(0.4124564f * c.r + 0.3575761f * c.g + 0.1804375f * c.b,
			   0.2126729f * c.r + 0.7151522f * c.g + 0.0721750f * c.b,
			   0.0193339f * c.r + 0.1191920f * c.g + 0.9503041f * c.b);
}

static inline RGB xyY_to_srgb(const RGB& c)
{
	return c.g == 0 ? RGB(0, 0, 0) : xyz_to_srgb(RGB(c.r * c.b / c.g, c.b, (1 - c.r - c.g) * c.b / c.g));
}

static inline RGB srgb_to_xyY(const RGB& c)
{
	const auto s = srgb_to_xyz(c);
	const auto n = s.r + s.g + s.b;
	return (n == 0) ? RGB(0, 0, 0) : RGB(s.r / n, s.g / n, s.g);
}

static inline float reinhard(float L)
{
	return L / (1.0f + L);
}

static inline float reinhard_modified(float L)
{
	constexpr float WhitePoint = 4.0f;
	return (L * (1.0f + L / (WhitePoint * WhitePoint))) / (1.0f + L);
}

static inline float aces(float L)
{
	constexpr float a = 2.51f;
	constexpr float b = 0.03f;
	constexpr float c = 2.43f;
	constexpr float d = 0.59f;
	constexpr float e = 0.14f;
	return std::min(1.0f, std::max(0.0f, (L * (a * L + b)) / (L * (c * L + d) + e)));
}

static inline float srgb_gamma(float x)
{
	if (x <= 0.0031308f)
		return 12.92f * x;
	else
		return 1.055f * std::pow(x, 0.416666667f) - 0.055f;
}

// We do not make use of C++20, therefore we do have our own Range
template <typename T>
class Range {
public:
	class Iterator : public std::iterator<std::random_access_iterator_tag, T> {
		friend class Range;

	public:
		using difference_type = typename std::iterator<std::random_access_iterator_tag, T>::difference_type;

		inline T operator*() const { return mValue; }
		inline T operator[](const T& n) const { return mValue + n; }

		inline const Iterator& operator++()
		{
			++mValue;
			return *this;
		}

		inline Iterator operator++(int)
		{
			Iterator copy(*this);
			++mValue;
			return copy;
		}

		inline const Iterator& operator--()
		{
			--mValue;
			return *this;
		}

		inline Iterator operator--(int)
		{
			Iterator copy(*this);
			--mValue;
			return copy;
		}

		inline Iterator& operator+=(const Iterator& other)
		{
			mValue += other.mValue;
			return *this;
		}

		inline Iterator& operator+=(const T& other)
		{
			mValue += other;
			return *this;
		}

		inline Iterator& operator-=(const Iterator& other)
		{
			mValue -= other.mValue;
			return *this;
		}

		inline Iterator& operator-=(const T& other)
		{
			mValue -= other;
			return *this;
		}

		inline bool operator<(const Iterator& other) const { return mValue < other.mValue; }
		inline bool operator<=(const Iterator& other) const { return mValue <= other.mValue; }
		inline bool operator>(const Iterator& other) const { return mValue > other.mValue; }
		inline bool operator>=(const Iterator& other) const { return mValue >= other.mValue; }

		inline bool operator==(const Iterator& other) const { return mValue == other.mValue; }
		inline bool operator!=(const Iterator& other) const { return mValue != other.mValue; }

		inline Iterator operator+(const Iterator& b) const { return Iterator(mValue + *b); }
		inline Iterator operator+(const Iterator::difference_type& b) const { return Iterator(mValue + b); }
		inline friend Iterator operator+(const Iterator::difference_type& a, const Iterator& b) { return Iterator(a + *b); }
		inline difference_type operator-(const Iterator& b) const { return difference_type(mValue - *b); }
		inline Iterator operator-(const Iterator::difference_type& b) const { return Iterator(mValue - b); }
		inline friend Iterator operator-(const Iterator::difference_type& a, const Iterator& b) { return Iterator(a - *b); }

		inline Iterator()
			: mValue(0)
		{
		}

		inline explicit Iterator(const T& start)
			: mValue(start)
		{
		}

		inline Iterator(const Iterator& other) = default;
		inline Iterator(Iterator&& other)	   = default;
		inline Iterator& operator=(const Iterator& other) = default;
		inline Iterator& operator=(Iterator&& other) = default;

	private:
		T mValue;
	};

	Iterator begin() const { return mBegin; }
	Iterator end() const { return mEnd; }
	Range(const T& begin, const T& end)
		: mBegin(begin)
		, mEnd(end)
	{
	}

private:
	Iterator mBegin;
	Iterator mEnd;
};

template <typename T>
void updateMaximum(std::atomic<T>& maximum_value, T const& value) noexcept
{
	T prev_value = maximum_value;
	while (prev_value < value && !maximum_value.compare_exchange_weak(prev_value, value)) {
	}
}

template <typename T>
void updateMinimum(std::atomic<T>& minimum_value, T const& value) noexcept
{
	T prev_value = minimum_value;
	while (prev_value > value && !minimum_value.compare_exchange_weak(prev_value, value)) {
	}
}

using RangeS = Range<size_t>;

static void analzeLuminance(size_t width, size_t height, uint32_t iter)
{
	const float* film	  = sPixels; // sRGB
	auto inv_iter		  = 1.0f / iter;
	const float avgFactor = 1.0f / (width * height);
	sLastLum			  = LuminanceInfo();

	const RangeS imageRange(0, width * height);

	const auto getL = [&](size_t ind) -> float {
		auto r = film[ind * 3 + 0];
		auto g = film[ind * 3 + 1];
		auto b = film[ind * 3 + 2];

		return srgb_to_xyY(RGB(r, g, b)).b * inv_iter;
	};

	// Extract basic information
	std::for_each(std::execution::par_unseq, imageRange.begin(), imageRange.end(), [&](size_t ind) {
		const auto L = getL(ind);

		updateMaximum(sLastLum.Max, L);
		updateMinimum(sLastLum.Min, L);
	});

	sLastLum.Avg = std::transform_reduce(std::execution::par_unseq, imageRange.begin(), imageRange.end(), 0.0f, std::plus<>(),
										 [&](size_t ind) { return getL(ind) * avgFactor; });

	// Setup histogram
	for (auto& a : sHistogram)
		a = 0;

	const float histogram_factor = HISTOGRAM_SIZE / std::max(sLastLum.Max.load(), 1.0f);
	std::for_each(std::execution::par_unseq, imageRange.begin(), imageRange.end(), [&](size_t ind) {
		const auto L  = getL(ind);
		const int idx = std::max(0, std::min<int>(L * histogram_factor, HISTOGRAM_SIZE - 1));
		sHistogram[idx]++;
	});

	for (size_t i = 0; i < sHistogram.size(); ++i)
		sHistogramF[i] = sHistogram[i] * avgFactor;

#ifdef USE_MEDIAN_FOR_LUMINANCE_ESTIMATION
	if (!sToneMapping_Automatic) {
		sLastLum.Est = sLastLum.Max;
		return;
	}

	// Estimate for reinhard
	// TODO: Parallel?
	constexpr size_t WINDOW_S = 3;
	constexpr size_t EDGE_S	  = WINDOW_S / 2;
	std::array<float, WINDOW_S * WINDOW_S> window;

	for (size_t y = EDGE_S; y < height - EDGE_S; ++y) {
		for (size_t x = EDGE_S; x < width - EDGE_S; ++x) {

			size_t i = 0;
			for (size_t wy = 0; wy < WINDOW_S; ++wy) {
				for (size_t wx = 0; wx < WINDOW_S; ++wx) {
					const auto ix = x + wx - EDGE_S;
					const auto iy = y + wy - EDGE_S;

					auto r = film[(iy * width + ix) * 3 + 0];
					auto g = film[(iy * width + ix) * 3 + 1];
					auto b = film[(iy * width + ix) * 3 + 2];

					window[i] = srgb_to_xyY(RGB(r, g, b)).b;
					++i;
				}
			}

			std::sort(window.begin(), window.end());
			const auto L = window[window.size() / 2];
			sLastLum.Est = std::max(sLastLum.Est, L * inv_iter);
		}
	}
#else
	sLastLum.Est = sLastLum.Max;
#endif
}

static void update_texture(uint32_t* buf, SDL_Texture* texture, size_t width, size_t height, uint32_t iter)
{
	const float* film = sPixels;
	auto inv_iter	  = 1.0f / iter;
	auto inv_gamma	  = 1.0f / 2.2f;

	const float exposure_factor = std::pow(2.0, sToneMapping_Exposure);
	analzeLuminance(width, height, iter);

	const RangeS imageRange(0, width * height);

	std::for_each(std::execution::par_unseq, imageRange.begin(), imageRange.end(), [&](size_t ind) {
		auto r = film[ind * 3 + 0] * inv_iter;
		auto g = film[ind * 3 + 1] * inv_iter;
		auto b = film[ind * 3 + 2] * inv_iter;

		const auto xyY = srgb_to_xyY(RGB(r, g, b));
#ifdef CULL_BAD_COLOR
		if (std::isinf(xyY.b)) {
#ifdef CATCH_BAD_COLOR
			buf[ind] = RGB_C(255, 0, 150); //Pink
#endif
			return;
		} else if (std::isnan(xyY.b)) {
#ifdef CATCH_BAD_COLOR
			buf[ind] = RGB_C(0, 255, 255); //Cyan
#endif
			return;
		} else if (xyY.r < 0.0f || xyY.g < 0.0f || xyY.b < 0.0f) {
#ifdef CATCH_BAD_COLOR
			buf[ind] = RGB_C(255, 255, 0); //Orange
#endif
			return;
		}
#endif

		float L;
		if (sToneMapping_Automatic)
			L = xyY.b / sLastLum.Est;
		else
			L = exposure_factor * xyY.b + sToneMapping_Offset;

		if (sToneMapping_Method == ToneMappingMethodOptions[(int)ToneMappingMethod::Reinhard])
			L = reinhard(L);
		else if (sToneMapping_Method == ToneMappingMethodOptions[(int)ToneMappingMethod::ModifiedReinhard])
			L = reinhard_modified(L);
		else if (sToneMapping_Method == ToneMappingMethodOptions[(int)ToneMappingMethod::ACES])
			L = aces(L);

		RGB color = xyY_to_srgb(RGB(xyY.r, xyY.g, L));

		if (sToneMappingGamma) {
			color.r = srgb_gamma(color.r);
			color.g = srgb_gamma(color.g);
			color.b = srgb_gamma(color.b);
		}

		buf[ind] = (uint32_t(clamp(std::pow(color.r, inv_gamma), 0.0f, 1.0f) * 255.0f) << 16)
				   | (uint32_t(clamp(std::pow(color.g, inv_gamma), 0.0f, 1.0f) * 255.0f) << 8)
				   | uint32_t(clamp(std::pow(color.b, inv_gamma), 0.0f, 1.0f) * 255.0f);
	});

	SDL_UpdateTexture(texture, nullptr, buf, width * sizeof(uint32_t));
}

static RGB get_film_data(size_t width, size_t height, uint32_t iter, uint32_t x, uint32_t y)
{
	const float* film	 = sPixels;
	const float inv_iter = 1.0f / iter;
	const uint32_t ind	 = y * width + x;

	return RGB{
		film[ind * 3 + 0] * inv_iter,
		film[ind * 3 + 1] * inv_iter,
		film[ind * 3 + 2] * inv_iter
	};
}

static void make_screenshot(size_t width, size_t height, uint32_t iter)
{
	std::stringstream out_file;
	auto now	   = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(now);
	out_file << "screenshot_" << std::put_time(std::localtime(&in_time_t), "%Y_%m_%d_%H_%M_%S") << ".exr";

	ImageRgba32 img;
	img.width  = width;
	img.height = height;
	img.pixels.reset(new float[width * height * 4]);

	const float* film = sPixels;
	auto inv_iter	  = 1.0f / iter;
	const RangeS imageRange(0, width * height);

	std::for_each(std::execution::par_unseq, imageRange.begin(), imageRange.end(), [&](size_t ind) {
		auto r = film[ind * 3 + 0];
		auto g = film[ind * 3 + 1];
		auto b = film[ind * 3 + 2];

		img.pixels[4 * ind + 0] = r * inv_iter;
		img.pixels[4 * ind + 1] = g * inv_iter;
		img.pixels[4 * ind + 2] = b * inv_iter;
		img.pixels[4 * ind + 3] = 1.0f;
	});

	if (!img.save(out_file.str()))
		IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "'" << std::endl;
	else
		IG_LOG(L_INFO) << "Screenshot saved to '" << out_file.str() << "'" << std::endl;
}

////////////////////////////////////////////////////////////////

void init(int width, int height, const float* pixels, bool showDebug)
{
	sShowDebugMode = showDebug;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
		IG_LOG(L_FATAL) << "Cannot initialize SDL." << std::endl;

	sWidth	= width;
	sHeight = height;
	sPixels = pixels;
	sWindow = SDL_CreateWindow(
		"Ignis",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		width,
		height,
		0);
	if (!sWindow)
		IG_LOG(L_FATAL) << "Cannot create window." << std::endl;

	sRenderer = SDL_CreateRenderer(sWindow, -1, 0);
	if (!sRenderer)
		IG_LOG(L_FATAL) << "Cannot create renderer." << std::endl;

	sTexture = SDL_CreateTexture(sRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
	if (!sTexture)
		IG_LOG(L_FATAL) << "Cannot create texture" << std::endl;

	sBuffer.resize(width * height);
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // if enabled -> ImGuiKey_Space has to be mapped
	ImGui::StyleColorsDark();

	ImGuiSDL::Initialize(sRenderer, width, height);

	sPoseManager.load(POSE_FILE);
}

void close()
{
	ImGuiSDL::Deinitialize();

	SDL_DestroyTexture(sTexture);
	SDL_DestroyRenderer(sRenderer);
	SDL_DestroyWindow(sWindow);
	SDL_Quit();

	sBuffer.clear();
}

void setTitle(const char* str)
{
	if (sLockInteraction) {
		std::stringstream sstream;
		sstream << str << " [Locked]";
		SDL_SetWindowTitle(sWindow, sstream.str().c_str());
	} else
		SDL_SetWindowTitle(sWindow, str);
}

bool handleInput(uint32_t& iter, bool& run, Camera& cam)
{
	return handle_events(iter, run, cam);
}

DebugMode currentDebugMode() { return sCurrentDebugMode; }

static void handle_imgui(uint32_t iter)
{
	constexpr size_t UI_W = 300;
	constexpr size_t UI_H = 440;
	int mouse_x, mouse_y;
	SDL_GetMouseState(&mouse_x, &mouse_y);

	RGB rgb{ 0, 0, 0 };
	if (mouse_x >= 0 && mouse_x < sWidth && mouse_y >= 0 && mouse_y < sHeight)
		rgb = get_film_data(sWidth, sHeight, iter, mouse_x, mouse_y);

	ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(UI_W, UI_H), ImGuiCond_Once);
	ImGui::Begin("Control");
	if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Iter %i", iter);
		ImGui::Text("SPP  %i", iter * 4);
		ImGui::Text("Cursor  (%f, %f, %f)", rgb.r, rgb.g, rgb.b);
		ImGui::Text("Max Lum %f", sLastLum.Max.load());
		ImGui::Text("Min Lum %f", sLastLum.Min.load());
		ImGui::Text("Avg Lum %f", sLastLum.Avg);
		ImGui::Text("Cam Eye (%f, %f, %f)", sLastCameraPose.Eye(0), sLastCameraPose.Eye(1), sLastCameraPose.Eye(2));
		ImGui::Text("Cam Dir (%f, %f, %f)", sLastCameraPose.Dir(0), sLastCameraPose.Dir(1), sLastCameraPose.Dir(2));
		ImGui::Text("Cam Up  (%f, %f, %f)", sLastCameraPose.Up(0), sLastCameraPose.Up(1), sLastCameraPose.Up(2));

		ImGui::PushItemWidth(-1);
		ImGui::PlotHistogram("", sHistogramF.data(), HISTOGRAM_SIZE, 0, nullptr, 0.0f, 1.0f, ImVec2(0, 60));
		ImGui::PopItemWidth();
	}

	if (sShowDebugMode) {
		if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::BeginCombo("Mode", sDebugMode_Method)) {
				for (int i = 0; i < IM_ARRAYSIZE(DebugModeOptions); ++i) {
					bool is_selected = (sDebugMode_Method == DebugModeOptions[i]);
					if (ImGui::Selectable(DebugModeOptions[i], is_selected) && sRunning) {
						sDebugMode_Method = DebugModeOptions[i];
						sCurrentDebugMode = (DebugMode)i;
					}
					if (is_selected && sRunning)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
		}
	}

	if (ImGui::CollapsingHeader("ToneMapping", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Automatic", &sToneMapping_Automatic);
		if (!sToneMapping_Automatic) {
			ImGui::SliderFloat("Exposure", &sToneMapping_Exposure, -10.0f, 10.0f);
			ImGui::SliderFloat("Offset", &sToneMapping_Offset, -10.0f, 10.0f);
		}
		if (ImGui::BeginCombo("Method", sToneMapping_Method)) {
			for (int i = 0; i < IM_ARRAYSIZE(ToneMappingMethodOptions); ++i) {
				bool is_selected = (sToneMapping_Method == ToneMappingMethodOptions[i]);
				if (ImGui::Selectable(ToneMappingMethodOptions[i], is_selected))
					sToneMapping_Method = ToneMappingMethodOptions[i];
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
		ImGui::Checkbox("Gamma", &sToneMappingGamma);
	}

	if (ImGui::CollapsingHeader("Poses")) {
		if (ImGui::Button("Reload")) {
			sPoseManager.load(POSE_FILE);
			IG_LOG(L_INFO) << "Poses loaed from '" << POSE_FILE << "'" << std::endl;
		}
		ImGui::SameLine();
		if (ImGui::Button("Save")) {
			sPoseManager.save(POSE_FILE);
			IG_LOG(L_INFO) << "Poses saved to '" << POSE_FILE << "'" << std::endl;
		}

		bool f = false;
		for (size_t i = 0; i < sPoseManager.poseCount(); ++i) {
			const auto pose = sPoseManager.pose(i);
			std::stringstream sstream;
			sstream << i + 1 << " | " << pose.Eye(0) << " " << pose.Eye(1) << " " << pose.Eye(2);
			if (ImGui::Selectable(sstream.str().c_str(), &f))
				sPoseRequest = (int)i;
		}
	}
	ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Press F1 for help...");
	ImGui::End();
}

inline static void markdownFormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo_, bool start_)
{
	switch (markdownFormatInfo_.type) {
	default:
		ImGui::defaultMarkdownFormatCallback(markdownFormatInfo_, start_);
		break;
	case ImGui::MarkdownFormatType::EMPHASIS:
		if (start_)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.16f, 0.57f, 0.94f, 1));
		else
			ImGui::PopStyleColor();
		break;
	}
}

static void handle_help()
{
	constexpr size_t UI_W = 300;
	constexpr size_t UI_H = 500;

	static const std::string Markdown =
		R"(- *1..9* number keys to switch between views.
- *1..9* and *Strg/Ctrl* to save the current view on that slot.
- *F1* to toggle this help window.
- *F2* to toggle the UI.
- *F3* to toggle the interaction lock. 
  If enabled, no view changing interaction is possible.
- *F11* to save a screenshot.
  The image will be saved in the current working directory.
- *R* to reset to initial view.
- *P* to pause current rendering. Also implies an interaction lock.
- *T* to toggle automatic tonemapping.
- *G* to reset tonemapping properties.
  Only works if automatic tonemapping is disabled.
- *F* to increase (or with *Shift* to decrease) tonemapping exposure.
  Step size can be decreased with *Strg/Ctrl*.
  Only works if automatic tonemapping is disabled.
- *V* to increase (or with *Shift* to decrease) tonemapping offset.
  Step size can be decreased with *Strg/Ctrl*.
  Only works if automatic tonemapping is disabled.
- *WASD* or arrow keys to travel through the scene.
- *Q/E* to rotate the camera around the viewing direction. 
- *PageUp/PageDown* to pan the camera up and down. 
- *Notepad +/-* to change the travel speed.
- *Numpad 1* to switch to front view.
- *Numpad 3* to switch to side view.
- *Numpad 7* to switch to top view.
- *Numpad 9* look behind you.
- *Numpad 2468* to rotate the camera.
- Mouse to rotate the camera.
)";

	ImGui::MarkdownConfig config;
	config.formatCallback = markdownFormatCallback;

	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Once);
	ImGui::Begin("Help");
	ImGui::Markdown(Markdown.c_str(), Markdown.length(), config);
	ImGui::End();
}

void update(uint32_t iter)
{
	update_texture(sBuffer.data(), sTexture, sWidth, sHeight, iter);
	if (sScreenshotRequest) {
		make_screenshot(sWidth, sHeight, iter);
		sScreenshotRequest = false;
	}
	SDL_RenderClear(sRenderer);
	SDL_RenderCopy(sRenderer, sTexture, nullptr, nullptr);

	if (sShowUI || sShowHelp) {
		ImGui::NewFrame();
		if (sShowUI)
			handle_imgui(iter);
		if (sShowHelp)
			handle_help();
		ImGui::Render();
		ImGuiSDL::Render(ImGui::GetDrawData());
	}

	SDL_RenderPresent(sRenderer);
}

} // namespace UI
} // namespace IG
#include "UI.h"
#include "Interface.h"
#include "Pose.h"

#include <SDL.h>

#include "imgui.h"
#include "imgui_sdl.h"

#include "Color.h"
#include "Image.h"
#include "Logger.h"

#include "generated_interface.h"

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

struct LuminanceInfo {
	float Min = Inf;
	float Max = 0.0f;
	float Avg = 0.0f;
	float Est = 0.000001f;
};

static int sPoseRequest		   = -1;
static bool sScreenshotRequest = false;
static bool sShowUI			   = true;
static bool sLockInteraction   = false;

// Stats
static LuminanceInfo sLastLum;
constexpr size_t HISTOGRAM_SIZE = 100;
static std::array<float, HISTOGRAM_SIZE> sHistogram;

static bool sToneMapping_Automatic = true;
static float sToneMapping_Exposure = 1.0f;
static float sToneMapping_Offset   = 0.0f;

// Pose IO
constexpr char POSE_FILE[] = "data/poses.lst";
static PoseManager sPoseManager;
CameraPose sLastCameraPose;

// Events
static void handle_pose_input(size_t posenmbr, bool capture, const Camera& cam)
{
	if (!capture) {
		sPoseRequest = posenmbr;
	} else {
		sPoseManager.setPose(posenmbr, CameraPose(cam));
		IG_LOG(L_INFO) << "Captured pose for " << posenmbr << std::endl;
	}
}

static bool handle_events(uint32_t& iter, Camera& cam)
{
	ImGuiIO& io = ImGui::GetIO();

	static bool camera_on			   = false;
	static std::array<bool, 12> arrows = { false, false, false, false, false, false, false, false, false, false, false, false };
	static bool speed[2]			   = { false, false };
	const float rspeed				   = 0.005f;
	static float tspeed				   = 0.1f;

	SDL_Event event;
	const bool hover = ImGui::IsAnyItemHovered() || ImGui::IsAnyWindowHovered();
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
				if (!sLockInteraction) {
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
						cam.update_dir(-cam.dir, cam.up);
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
					}
				}
				switch (event.key.keysym.sym) {
				case SDLK_t:
					sToneMapping_Automatic = !sToneMapping_Automatic;
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
			if (event.button.button == SDL_BUTTON_LEFT && !hover && !sLockInteraction) {
				SDL_SetRelativeMouseMode(SDL_TRUE);
				camera_on = true;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			SDL_SetRelativeMouseMode(SDL_FALSE);
			camera_on = false;
			break;
		case SDL_MOUSEMOTION:
			if (camera_on && !hover && !sLockInteraction) {
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

	if (sPoseRequest >= 0) {
		auto pose = sPoseManager.pose(sPoseRequest);
		cam.eye	  = pose.Eye;
		cam.update_dir(pose.Dir, pose.Up);
		iter		 = 0;
		sPoseRequest = -1;
	}

	sLastCameraPose = CameraPose(cam);

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

static inline float reinhard_modified(float L)
{
	constexpr float WhitePoint = 4.0f;
	return (L * (1.0f + L / (WhitePoint * WhitePoint))) / (1.0f + L);
}

static void analzeLuminance(size_t width, size_t height, uint32_t iter)
{
	const auto film		  = get_pixels(); // sRGB
	auto inv_iter		  = 1.0f / iter;
	const float avgFactor = 1.0f / (width * height);
	sLastLum			  = LuminanceInfo();

	// Extract basic information
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto r = film[(y * width + x) * 3 + 0];
			auto g = film[(y * width + x) * 3 + 1];
			auto b = film[(y * width + x) * 3 + 2];

			const auto L = srgb_to_xyY(RGB(r, g, b)).b * inv_iter;

			sLastLum.Max = std::max(sLastLum.Max, L);
			sLastLum.Min = std::min(sLastLum.Min, L);
			sLastLum.Avg += L * avgFactor;
		}
	}

	// Setup histogram
	sHistogram.fill(0.0f);
	const float histogram_factor = HISTOGRAM_SIZE / std::max(sLastLum.Max, 1.0f);
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto r = film[(y * width + x) * 3 + 0];
			auto g = film[(y * width + x) * 3 + 1];
			auto b = film[(y * width + x) * 3 + 2];

			const auto L  = srgb_to_xyY(RGB(r, g, b)).b * inv_iter;
			const int idx = std::max(0, std::min<int>(L * histogram_factor, HISTOGRAM_SIZE - 1));
			sHistogram[idx] += avgFactor;
		}
	}

	// Estimate for reinhard
#ifdef USE_MEDIAN_FOR_LUMINANCE_ESTIMATION
	if (!sToneMapping_Automatic) {
		sLastLum.Est = sLastLum.Max;
		return;
	}
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
	auto film	   = get_pixels();
	auto inv_iter  = 1.0f / iter;
	auto inv_gamma = 1.0f / 2.2f;

	const float exposure_factor = std::pow(2.0, sToneMapping_Exposure);
	analzeLuminance(width, height, iter);

	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto r = film[(y * width + x) * 3 + 0] * inv_iter;
			auto g = film[(y * width + x) * 3 + 1] * inv_iter;
			auto b = film[(y * width + x) * 3 + 2] * inv_iter;

			const auto xyY = srgb_to_xyY(RGB(r, g, b));
#ifdef CULL_BAD_COLOR
			if (std::isinf(xyY.b)) {
#ifdef CATCH_BAD_COLOR
				buf[y * width + x] = RGB_C(255, 0, 150); //Pink
#endif
				continue;
			} else if (std::isnan(xyY.b)) {
#ifdef CATCH_BAD_COLOR
				buf[y * width + x] = RGB_C(0, 255, 255); //Cyan
#endif
				continue;
			} else if (xyY.r < 0.0f || xyY.g < 0.0f || xyY.b < 0.0f) {
#ifdef CATCH_BAD_COLOR
				buf[y * width + x] = RGB_C(255, 255, 0); //Orange
#endif
				continue;
			}
#endif

			RGB color;
			if (sToneMapping_Automatic) {
				const float L = xyY.b / sLastLum.Est;
				color		  = xyY_to_srgb(RGB(xyY.r, xyY.g, reinhard_modified(L)));
			} else {
				color = RGB(exposure_factor * r + sToneMapping_Offset,
							exposure_factor * g + sToneMapping_Offset,
							exposure_factor * b + sToneMapping_Offset);
			}

			buf[y * width + x] = (uint32_t(clamp(std::pow(color.r, inv_gamma), 0.0f, 1.0f) * 255.0f) << 16)
								 | (uint32_t(clamp(std::pow(color.g, inv_gamma), 0.0f, 1.0f) * 255.0f) << 8)
								 | uint32_t(clamp(std::pow(color.b, inv_gamma), 0.0f, 1.0f) * 255.0f);
		}
	}
	SDL_UpdateTexture(texture, nullptr, buf, width * sizeof(uint32_t));
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

	auto film	  = get_pixels();
	auto inv_iter = 1.0f / iter;
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			auto r = film[(y * width + x) * 3 + 0];
			auto g = film[(y * width + x) * 3 + 1];
			auto b = film[(y * width + x) * 3 + 2];

			img.pixels[4 * (y * width + x) + 0] = r * inv_iter;
			img.pixels[4 * (y * width + x) + 1] = g * inv_iter;
			img.pixels[4 * (y * width + x) + 2] = b * inv_iter;
			img.pixels[4 * (y * width + x) + 3] = 1.0f;
		}
	}

	if (!img.save(out_file.str()))
		IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "'" << std::endl;
	else
		IG_LOG(L_INFO) << "Screenshot saved to '" << out_file.str() << "'" << std::endl;
}

////////////////////////////////////////////////////////////////

void init(int width, int height)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
		IG_LOG(L_FATAL) << "Cannot initialize SDL." << std::endl;

	sWidth	= width;
	sHeight = height;
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

bool handleInput(uint32_t& iter, Camera& cam)
{
	return handle_events(iter, cam);
}

constexpr size_t UI_W = 300;
constexpr size_t UI_H = 400;
static void handle_imgui(uint32_t iter)
{
	ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(UI_W, UI_H), ImGuiCond_Once);
	ImGui::Begin("Control");
	if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Iter %i", iter);
		ImGui::Text("SPP %i", iter * get_spp());
		ImGui::Text("Max Lum %f", sLastLum.Max);
		ImGui::Text("Min Lum %f", sLastLum.Min);
		ImGui::Text("Avg Lum %f", sLastLum.Avg);
		ImGui::Text("Cam Eye (%f, %f, %f)", sLastCameraPose.Eye(0), sLastCameraPose.Eye(1), sLastCameraPose.Eye(2));
		ImGui::Text("Cam Dir (%f, %f, %f)", sLastCameraPose.Dir(0), sLastCameraPose.Dir(1), sLastCameraPose.Dir(2));
		ImGui::Text("Cam Up  (%f, %f, %f)", sLastCameraPose.Up(0), sLastCameraPose.Up(1), sLastCameraPose.Up(2));

		ImGui::PushItemWidth(-1);
		ImGui::PlotHistogram("", sHistogram.data(), HISTOGRAM_SIZE, 0, nullptr, 0.0f, 1.0f, ImVec2(0, 60));
		ImGui::PopItemWidth();
	}

	if (ImGui::CollapsingHeader("ToneMapping", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Automatic", &sToneMapping_Automatic);
		if (!sToneMapping_Automatic) {
			ImGui::SliderFloat("Exposure", &sToneMapping_Exposure, 0.01f, 10.0f);
			ImGui::SliderFloat("Offset", &sToneMapping_Offset, 0.0f, 10.0f);
		}
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

	if (sShowUI) {
		ImGui::NewFrame();
		handle_imgui(iter);
		ImGui::Render();
		ImGuiSDL::Render(ImGui::GetDrawData());
	}

	SDL_RenderPresent(sRenderer);
}

} // namespace UI
} // namespace IG
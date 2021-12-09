#include "UI.h"
#include "IO.h"
#include "Pose.h"
#include "Range.h"

#include <SDL.h>

#include "imgui.h"
#include "imgui_markdown.h"
#include "imgui_sdl.h"

#include "Color.h"
#include "Logger.h"
#include "ToneMapping.h"

#include <atomic>
#include <exception>
#include <iomanip>

#ifndef IG_NO_EXECUTION_H
#include <execution>
#endif

namespace IG {

#define CULL_BAD_COLOR
#define CATCH_BAD_COLOR
#define USE_MEDIAN_FOR_LUMINANCE_ESTIMATION

constexpr size_t HISTOGRAM_SIZE				  = 100;
static const char* ToneMappingMethodOptions[] = {
	"None", "Reinhard", "Mod. Reinhard", "ACES"
};

static const char* DebugModeOptions[] = {
	"Normal", "Tangent", "Bitangent", "Geometric Normal", "Texture Coords", "Prim Coords", "Point", "Hit Distance",
	"Raw Prim ID", "Prim ID", "Raw Entity ID", "Entity ID",
	"Is Emissive", "Is Specular", "Is Entering", "Check BSDF"
};

// Pose IO
constexpr char POSE_FILE[] = "poses.lst";

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

class UIInternal {
public:
	UI* Parent;
	SDL_Window* Window;
	SDL_Renderer* Renderer;
	SDL_Texture* Texture;
	std::vector<uint32_t> Buffer;

	int PoseRequest		  = -1;
	bool PoseResetRequest = false;
	int ScreenshotRequest = 0; // 0-Nothing, 1-Screenshot, 2-Full screenshot
	bool ShowHelp		  = false;
	bool ShowUI			  = true;
	bool LockInteraction  = false;

	int Width, Height;

	// Stats
	LuminanceInfo LastLum;
	std::array<std::atomic<uint32>, HISTOGRAM_SIZE> Histogram;
	std::array<float, HISTOGRAM_SIZE> HistogramF;

	bool ToneMapping_Automatic = true;
	float ToneMapping_Exposure = 1.0f;
	float ToneMapping_Offset   = 0.0f;
	bool Running			   = true;

	bool ToneMappingGamma = false;
	bool ShowDebugMode	  = false;

	IG::PoseManager PoseManager;
	CameraPose LastCameraPose;

	// Events
	void handlePoseInput(size_t posenmbr, bool capture, const Camera& cam)
	{
		if (!capture) {
			PoseRequest = posenmbr;
		} else {
			PoseManager.setPose(posenmbr, CameraPose(cam));
			IG_LOG(L_INFO) << "Captured pose for " << posenmbr + 1 << std::endl;
		}
	}

	// Events
	bool handleEvents(uint32_t& iter, bool& run, Camera& cam)
	{
		static bool first_call = true;
		if (first_call) {
			PoseManager.setInitalPose(CameraPose(cam));
			first_call = false;
		}

		ImGuiIO& io = ImGui::GetIO();

		static bool camera_on			   = false;
		static std::array<bool, 12> arrows = { false, false, false, false, false, false, false, false, false, false, false, false };
		static bool speed[2]			   = { false, false };
		const float rspeed				   = 0.005f;
		const float slow_factor			   = 0.1f;
		const float fast_factor			   = 1.5f;
		static float tspeed				   = 0.1f;

		const bool canInteract = !LockInteraction && run;

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
							handlePoseInput(0, capture, cam);
							break;
						case SDLK_2:
							handlePoseInput(1, capture, cam);
							break;
						case SDLK_3:
							handlePoseInput(2, capture, cam);
							break;
						case SDLK_4:
							handlePoseInput(3, capture, cam);
							break;
						case SDLK_5:
							handlePoseInput(4, capture, cam);
							break;
						case SDLK_6:
							handlePoseInput(5, capture, cam);
							break;
						case SDLK_7:
							handlePoseInput(6, capture, cam);
							break;
						case SDLK_8:
							handlePoseInput(7, capture, cam);
							break;
						case SDLK_9:
							handlePoseInput(8, capture, cam);
							break;
						case SDLK_0:
							handlePoseInput(9, capture, cam);
							break;
						case SDLK_r:
							PoseResetRequest = true;
							break;
						}
					}
					switch (event.key.keysym.sym) {
					case SDLK_t:
						ToneMapping_Automatic = !ToneMapping_Automatic;
						break;
					case SDLK_g:
						if (!ToneMapping_Automatic) {
							ToneMapping_Exposure = 1.0f;
							ToneMapping_Offset	 = 0.0f;
						}
						break;
					case SDLK_f:
						if (!ToneMapping_Automatic) {
							const float delta = io.KeyCtrl ? 0.05f : 0.5f;
							ToneMapping_Exposure += io.KeyShift ? -delta : delta;
						}
						break;
					case SDLK_v:
						if (!ToneMapping_Automatic) {
							const float delta = io.KeyCtrl ? 0.05f : 0.5f;
							ToneMapping_Offset += io.KeyShift ? -delta : delta;
						}
						break;
					case SDLK_p:
						run = !run;
						break;
					case SDLK_n:
						Parent->changeAOV(-1);
						break;
					case SDLK_m:
						Parent->changeAOV(1);
						break;
					case SDLK_F1:
						ShowHelp = !ShowHelp;
						break;
					case SDLK_F2:
						ShowUI = !ShowUI;
						break;
					case SDLK_F3:
						LockInteraction = !LockInteraction;
						break;
					case SDLK_F11:
						if (io.KeyCtrl)
							ScreenshotRequest = 2;
						else
							ScreenshotRequest = 1;
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
					const float aspeed	= rspeed * (io.KeyCtrl ? fast_factor : (io.KeyShift ? slow_factor : 1.0f));
					const float xmotion = event.motion.xrel * aspeed;
					const float ymotion = event.motion.yrel * aspeed;
					if (io.KeyAlt)
						cam.rotate_fixroll(xmotion, ymotion);
					else
						cam.rotate(xmotion, ymotion);
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
			if (!LockInteraction) {
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

			if (PoseResetRequest || PoseRequest >= 0) {
				auto pose = PoseResetRequest ? PoseManager.initialPose() : PoseManager.pose(PoseRequest);
				cam.Eye	  = pose.Eye;
				cam.update_dir(pose.Dir, pose.Up);
				iter			 = 0;
				PoseRequest		 = -1;
				PoseResetRequest = false;
			}
		}

		LastCameraPose = CameraPose(cam);
		Running		   = run;

		return false;
	}

	void analzeLuminance(size_t width, size_t height, uint32_t iter)
	{
		const float* film	  = Parent->currentPixels(); // sRGB
		auto inv_iter		  = 1.0f / iter;
		const float avgFactor = 1.0f / (width * height);
		LastLum				  = LuminanceInfo();

		const RangeS imageRange(0, width * height);

		const auto getL = [&](size_t ind) -> float {
			auto r = film[ind * 3 + 0];
			auto g = film[ind * 3 + 1];
			auto b = film[ind * 3 + 2];

			return srgb_to_xyY(RGB(r, g, b)).b * inv_iter;
		};

		// Extract basic information
		const auto updateRange = [&](size_t ind) {
			const auto L = getL(ind);

			updateMaximum(LastLum.Max, L);
			updateMinimum(LastLum.Min, L);
		};

#ifndef IG_NO_EXECUTION_H
		std::for_each(std::execution::par_unseq, imageRange.begin(), imageRange.end(), updateRange);
#else
		for (size_t i : imageRange)
			updateRange(i);
#endif

		const auto lumFactor = [&](size_t ind) { return getL(ind) * avgFactor; };
#ifndef IG_NO_EXECUTION_H
		LastLum.Avg = std::transform_reduce(std::execution::par_unseq, imageRange.begin(), imageRange.end(), 0.0f, std::plus<>(),
											lumFactor);
#else
		LastLum.Avg = 0;
		for (size_t i : imageRange)
			LastLum.Avg += lumFactor(i);

#endif

		// Setup histogram
		for (auto& a : Histogram)
			a = 0;

		const float histogram_factor = HISTOGRAM_SIZE / std::max(LastLum.Max.load(), 1.0f);
		const auto updateHistogram	 = [&](size_t ind) {
			  const auto L	= getL(ind);
			  const int idx = std::max(0, std::min<int>(L * histogram_factor, HISTOGRAM_SIZE - 1));
			  Histogram[idx]++;
		};

#ifndef IG_NO_EXECUTION_H
		std::for_each(std::execution::par_unseq, imageRange.begin(), imageRange.end(), updateHistogram);
#else
		for (size_t i : imageRange)
			updateHistogram(i);
#endif

		for (size_t i = 0; i < Histogram.size(); ++i)
			HistogramF[i] = Histogram[i] * avgFactor;

#ifdef USE_MEDIAN_FOR_LUMINANCE_ESTIMATION
		if (!ToneMapping_Automatic) {
			LastLum.Est = LastLum.Max;
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
				LastLum.Est	 = std::max(LastLum.Est, L * inv_iter);
			}
		}
#else
		sLastLum.Est = sLastLum.Max;
#endif
	}

	void updateSurface(uint32_t iter)
	{
		const float* film = Parent->currentPixels();
		auto inv_iter	  = 1.0f / iter;
		auto inv_gamma	  = 1.0f / 2.2f;

		const float exposure_factor = std::pow(2.0, ToneMapping_Exposure);
		analzeLuminance(Width, Height, iter);

		const RangeS imageRange(0, Width * Height);
		uint32* buf = Buffer.data();

		const auto updateImage = [&](size_t ind) {
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
			if (ToneMapping_Automatic)
				L = xyY.b / LastLum.Est;
			else
				L = exposure_factor * xyY.b + ToneMapping_Offset;

			switch (Parent->mToneMappingMethod) {
			default:
			case ToneMappingMethod::Reinhard:
				L = reinhard(L);
				break;
			case ToneMappingMethod::ModifiedReinhard:
				L = reinhard_modified(L);
				break;
			case ToneMappingMethod::ACES:
				L = aces(L);
				break;
			}

			RGB color = xyY_to_srgb(RGB(xyY.r, xyY.g, L));

			if (ToneMappingGamma) {
				color.r = srgb_gamma(color.r);
				color.g = srgb_gamma(color.g);
				color.b = srgb_gamma(color.b);
			}

			buf[ind] = (uint32_t(clamp(std::pow(color.r, inv_gamma), 0.0f, 1.0f) * 255.0f) << 16)
					   | (uint32_t(clamp(std::pow(color.g, inv_gamma), 0.0f, 1.0f) * 255.0f) << 8)
					   | uint32_t(clamp(std::pow(color.b, inv_gamma), 0.0f, 1.0f) * 255.0f);
		};

#ifndef IG_NO_EXECUTION_H
		std::for_each(std::execution::par_unseq, imageRange.begin(), imageRange.end(), updateImage);
#else
		for (size_t i : imageRange)
			updateImage(i);
#endif

		SDL_UpdateTexture(Texture, nullptr, buf, Width * sizeof(uint32_t));
	}

	RGB getFilmData(size_t width, size_t height, uint32_t iter, uint32_t x, uint32_t y)
	{
		IG_UNUSED(height);

		const float* film	 = Parent->currentPixels();
		const float inv_iter = 1.0f / iter;
		const uint32_t ind	 = y * width + x;

		return RGB{
			film[ind * 3 + 0] * inv_iter,
			film[ind * 3 + 1] * inv_iter,
			film[ind * 3 + 2] * inv_iter
		};
	}

	void makeScreenshot(size_t width, size_t height, uint32_t iter)
	{
		std::stringstream out_file;
		auto now	   = std::chrono::system_clock::now();
		auto in_time_t = std::chrono::system_clock::to_time_t(now);
		out_file << "screenshot_" << std::put_time(std::localtime(&in_time_t), "%Y_%m_%d_%H_%M_%S") << ".exr";

		if (!saveImageRGB(out_file.str(), Parent->currentPixels(), width, height, 1.0f / iter))
			IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "'" << std::endl;
		else
			IG_LOG(L_INFO) << "Screenshot saved to '" << out_file.str() << "'" << std::endl;
	}

	void makeFullScreenshot()
	{
		std::stringstream out_file;
		auto now	   = std::chrono::system_clock::now();
		auto in_time_t = std::chrono::system_clock::to_time_t(now);
		out_file << "screenshot_full_" << std::put_time(std::localtime(&in_time_t), "%Y_%m_%d_%H_%M_%S") << ".exr";

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		Uint32 rmask = 0xff000000;
		Uint32 gmask = 0x00ff0000;
		Uint32 bmask = 0x0000ff00;
		Uint32 amask = 0x000000ff;
#else
		Uint32 rmask = 0x000000ff;
		Uint32 gmask = 0x0000ff00;
		Uint32 bmask = 0x00ff0000;
		Uint32 amask = 0xff000000;
#endif

		SDL_Surface* sshot = SDL_CreateRGBSurface(0, Width, Height, 32,
												  rmask, gmask, bmask, amask);

		if (!sshot) {
			IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "': " << SDL_GetError() << std::endl;
			return;
		}

		int ret = SDL_LockSurface(sshot);
		if (ret != 0) {
			IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "': " << SDL_GetError() << std::endl;
			return;
		}

		ret = SDL_RenderReadPixels(Renderer, nullptr, sshot->format->format,
								   sshot->pixels, sshot->pitch);
		if (ret != 0) {
			IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "': " << SDL_GetError() << std::endl;
			return;
		}

		float* rgba = new float[Width * Height * 4];
		for (int y = 0; y < Height; ++y) {
			const uint8* src = reinterpret_cast<const uint8*>(sshot->pixels) + y * sshot->pitch;
			float* dst		 = rgba + y * Width * 4;
			for (int x = 0; x < Width; ++x) {
				uint32 pixel = *reinterpret_cast<const uint32*>(&src[x * sshot->format->BytesPerPixel]);

				uint8 r, g, b, a;
				SDL_GetRGBA(pixel, sshot->format, &r, &g, &b, &a);

				dst[x * 4 + 0] = r / 255.0f;
				dst[x * 4 + 1] = g / 255.0f;
				dst[x * 4 + 2] = b / 255.0f;
				dst[x * 4 + 3] = a / 255.0f;
			}
		}

		SDL_UnlockSurface(sshot);
		SDL_FreeSurface(sshot);

		if (!saveImageRGBA(out_file.str(), rgba, Width, Height, 1))
			IG_LOG(L_ERROR) << "Failed to save EXR file '" << out_file.str() << "'" << std::endl;
		else
			IG_LOG(L_INFO) << "Screenshot saved to '" << out_file.str() << "'" << std::endl;

		delete[] rgba;
	}

	void handleImgui(uint32_t iter, uint32_t samplesPerIteration)
	{
		constexpr size_t UI_W = 300;
		constexpr size_t UI_H = 440;
		int mouse_x, mouse_y;
		SDL_GetMouseState(&mouse_x, &mouse_y);

		RGB rgb{ 0, 0, 0 };
		if (mouse_x >= 0 && mouse_x < Width && mouse_y >= 0 && mouse_y < Height)
			rgb = getFilmData(Width, Height, iter, mouse_x, mouse_y);

		ImGui::SetNextWindowPos(ImVec2(5, 5), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(UI_W, UI_H), ImGuiCond_Once);
		ImGui::Begin("Control");
		if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Iter %i", iter);
			ImGui::Text("SPP  %i", iter * samplesPerIteration);
			ImGui::Text("Cursor  (%f, %f, %f)", rgb.r, rgb.g, rgb.b);
			ImGui::Text("Max Lum %f", LastLum.Max.load());
			ImGui::Text("Min Lum %f", LastLum.Min.load());
			ImGui::Text("Avg Lum %f", LastLum.Avg);
			ImGui::Text("Cam Eye (%f, %f, %f)", LastCameraPose.Eye(0), LastCameraPose.Eye(1), LastCameraPose.Eye(2));
			ImGui::Text("Cam Dir (%f, %f, %f)", LastCameraPose.Dir(0), LastCameraPose.Dir(1), LastCameraPose.Dir(2));
			ImGui::Text("Cam Up  (%f, %f, %f)", LastCameraPose.Up(0), LastCameraPose.Up(1), LastCameraPose.Up(2));

			ImGui::PushItemWidth(-1);
			ImGui::PlotHistogram("", HistogramF.data(), HISTOGRAM_SIZE, 0, nullptr, 0.0f, 1.0f, ImVec2(0, 60));
			ImGui::PopItemWidth();
		}

		if (ShowDebugMode) {
			if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
				const char* current_method = DebugModeOptions[(int)Parent->mDebugMode];
				if (ImGui::BeginCombo("Mode", current_method)) {
					for (int i = 0; i < IM_ARRAYSIZE(DebugModeOptions); ++i) {
						bool is_selected = (current_method == DebugModeOptions[i]);
						if (ImGui::Selectable(DebugModeOptions[i], is_selected) && Running) {
							Parent->mDebugMode = (DebugMode)i;
						}
						if (is_selected && Running)
							ImGui::SetItemDefaultFocus();
					}

					ImGui::EndCombo();
				}
			}
		}

		if (ImGui::CollapsingHeader("ToneMapping", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Automatic", &ToneMapping_Automatic);
			if (!ToneMapping_Automatic) {
				ImGui::SliderFloat("Exposure", &ToneMapping_Exposure, -10.0f, 10.0f);
				ImGui::SliderFloat("Offset", &ToneMapping_Offset, -10.0f, 10.0f);
			}

			const char* current_method = ToneMappingMethodOptions[(int)Parent->mToneMappingMethod];
			if (ImGui::BeginCombo("Method", current_method)) {
				for (int i = 0; i < IM_ARRAYSIZE(ToneMappingMethodOptions); ++i) {
					bool is_selected = (current_method == ToneMappingMethodOptions[i]);
					if (ImGui::Selectable(ToneMappingMethodOptions[i], is_selected))
						Parent->mToneMappingMethod = (ToneMappingMethod)i;
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::Checkbox("Gamma", &ToneMappingGamma);
		}

		if (ImGui::CollapsingHeader("Poses")) {
			if (ImGui::Button("Reload")) {
				PoseManager.load(POSE_FILE);
				IG_LOG(L_INFO) << "Poses loaed from '" << POSE_FILE << "'" << std::endl;
			}
			ImGui::SameLine();
			if (ImGui::Button("Save")) {
				PoseManager.save(POSE_FILE);
				IG_LOG(L_INFO) << "Poses saved to '" << POSE_FILE << "'" << std::endl;
			}

			bool f = false;
			for (size_t i = 0; i < PoseManager.poseCount(); ++i) {
				const auto pose = PoseManager.pose(i);
				std::stringstream sstream;
				sstream << i + 1 << " | " << pose.Eye(0) << " " << pose.Eye(1) << " " << pose.Eye(2);
				if (ImGui::Selectable(sstream.str().c_str(), &f))
					PoseRequest = (int)i;
			}
		}
		ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Press F1 for help...");
		ImGui::End();
	}
};

////////////////////////////////////////////////////////////////

UI::UI(int width, int height, const std::vector<const float*>& aovs, bool showDebug)
	: mWidth(width)
	, mHeight(height)
	, mAOVs(aovs)
	, mCurrentAOV(0)
	, mShowDebug(showDebug)
	, mDebugMode(DebugMode::Normal)
	, mToneMappingMethod(ToneMappingMethod::ACES)
	, mInternal(std::make_unique<UIInternal>())
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		IG_LOG(L_FATAL) << "Cannot initialize SDL: " << SDL_GetError() << std::endl;
		throw std::runtime_error("Could not setup UI");
	}

	mInternal->Parent = this;
	mInternal->Width  = width;
	mInternal->Height = height;

	mInternal->Window = SDL_CreateWindow(
		"Ignis",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		width,
		height,
		0);
	if (!mInternal->Window) {
		IG_LOG(L_FATAL) << "Cannot create SDL window: " << SDL_GetError() << std::endl;
		throw std::runtime_error("Could not setup UI");
	}

	mInternal->Renderer = SDL_CreateRenderer(mInternal->Window, -1, 0);
	if (!mInternal->Renderer) {
		IG_LOG(L_FATAL) << "Cannot create SDL renderer: " << SDL_GetError() << std::endl;
		throw std::runtime_error("Could not setup UI");
	}

	mInternal->Texture = SDL_CreateTexture(mInternal->Renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (!mInternal->Texture) {
		IG_LOG(L_FATAL) << "Cannot create SDL texture: " << SDL_GetError() << std::endl;
		throw std::runtime_error("Could not setup UI");
	}

	mInternal->Buffer.resize(width * height);
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // if enabled -> ImGuiKey_Space has to be mapped
	ImGui::StyleColorsDark();

	ImGuiSDL::Initialize(mInternal->Renderer, width, height);

	mInternal->PoseManager.load(POSE_FILE);
}

UI::~UI()
{
	ImGuiSDL::Deinitialize();

	SDL_DestroyTexture(mInternal->Texture);
	SDL_DestroyRenderer(mInternal->Renderer);
	SDL_DestroyWindow(mInternal->Window);
	SDL_Quit();

	mInternal->Buffer.clear();
}

void UI::setTitle(const char* str)
{
	if (mInternal->LockInteraction) {
		std::stringstream sstream;
		sstream << str << " [Locked]";
		SDL_SetWindowTitle(mInternal->Window, sstream.str().c_str());
	} else
		SDL_SetWindowTitle(mInternal->Window, str);
}

bool UI::handleInput(uint32_t& iter, bool& run, Camera& cam)
{
	return mInternal->handleEvents(iter, run, cam);
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

static void handleHelp()
{
	static const std::string Markdown =
		R"(- *1..9* number keys to switch between views.
- *1..9* and *Strg/Ctrl* to save the current view on that slot.
- *F1* to toggle this help window.
- *F2* to toggle the UI.
- *F3* to toggle the interaction lock. 
  If enabled, no view changing interaction is possible.
- *F11* to save a snapshot of the current rendering. HDR information will be preserved.
  Use with *Strg/Ctrl* to make a LDR screenshot of the current render including UI and tonemapping.  
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
  *Shift* will move slower, *Strg/Ctrl* will move faster.
  Use with *Alt* to enable first person camera behaviour.
)";

	ImGui::MarkdownConfig config;
	config.formatCallback = markdownFormatCallback;

	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Once);
	ImGui::Begin("Help");
	ImGui::Markdown(Markdown.c_str(), Markdown.length(), config);
	ImGui::End();
}

void UI::update(uint32_t iter, uint32_t samplesPerIteration)
{
	mInternal->updateSurface(iter);
	switch (mInternal->ScreenshotRequest) {
	case 1:
		mInternal->makeScreenshot(mWidth, mHeight, iter);
		mInternal->ScreenshotRequest = 0;
		break;
	case 2:
		mInternal->makeFullScreenshot();
		mInternal->ScreenshotRequest = 0;
		break;
	default:
		break;
	}
	SDL_RenderClear(mInternal->Renderer);
	SDL_RenderCopy(mInternal->Renderer, mInternal->Texture, nullptr, nullptr);

	if (mInternal->ShowUI || mInternal->ShowHelp) {
		ImGui::NewFrame();
		if (mInternal->ShowUI)
			mInternal->handleImgui(iter, samplesPerIteration);
		if (mInternal->ShowHelp)
			handleHelp();
		ImGui::Render();
		ImGuiSDL::Render(ImGui::GetDrawData());
	}

	SDL_RenderPresent(mInternal->Renderer);
}

void UI::changeAOV(int delta_aov)
{
	mCurrentAOV = (mCurrentAOV + delta_aov) % mAOVs.size();
}
} // namespace IG
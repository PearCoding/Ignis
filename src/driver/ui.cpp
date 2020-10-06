#include "ui.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <SDL.h>

#include "imgui.h"
#include "imgui_sdl.h"

#if 0
#include "interface.h"

#include "runtime/color.h"
#include "runtime/common.h"
#include "runtime/image.h"
#endif

#include "Logger.h"

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

struct CameraPose {
	Vector3f Eye = Vector3f(0, 0, 0);
	Vector3f Dir = Vector3f(0, 0, 1);
	Vector3f Up	 = Vector3f(0, 1, 0);

	inline CameraPose() = default;
	inline explicit CameraPose(const Camera& cam)
		: Eye(cam.eye)
		, Dir(cam.dir)
		, Up(cam.up)
	{
	}
};

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

static CameraPose sLastCameraPose;

static bool sToneMapping_Automatic = true;
static float sToneMapping_Exposure = 1.0f;
static float sToneMapping_Offset   = 0.0f;

// Interface
//float* get_pixels();

// Pose IO
static std::array<CameraPose, 10> sCameraPoses;
constexpr char POSE_FILE[] = "data/poses.lst";

static CameraPose read_pose(std::istream& in)
{
	CameraPose pose;
	in >> pose.Eye[0] >> pose.Eye[1] >> pose.Eye[2];
	in >> pose.Dir[0] >> pose.Dir[1] >> pose.Dir[2];
	in >> pose.Up[0] >> pose.Up[1] >> pose.Up[2];
	return pose;
}

static void write_pose(const CameraPose& pose, std::ostream& out)
{
	out << pose.Eye[0] << " " << pose.Eye[1] << " " << pose.Eye[2];
	out << " " << pose.Dir[0] << " " << pose.Dir[1] << " " << pose.Dir[2];
	out << " " << pose.Up[0] << " " << pose.Up[1] << " " << pose.Up[2];
}

inline bool file_exists(const std::string& name)
{
	return std::ifstream(name.c_str()).good();
}

static void read_pose_file()
{
	// Reset poses
	for (auto& pose : sCameraPoses)
		pose = CameraPose();

	// Check if pose exists
	if (!file_exists(POSE_FILE))
		return;

	// Read all poses
	std::ifstream stream(POSE_FILE);
	for (size_t i = 0; i < sCameraPoses.size() && stream.good(); ++i) {
		sCameraPoses[i] = read_pose(stream);
	}
}

static void write_pose_file()
{
	std::ofstream stream(POSE_FILE);
	for (size_t i = 0; i < sCameraPoses.size(); ++i) {
		write_pose(sCameraPoses[i], stream);
		stream << std::endl;
	}
}

// Events
static void handle_pose_input(size_t posenmbr, bool capture, const Camera& cam)
{
	if (!capture) {
		sPoseRequest = posenmbr;
	} else {
		sCameraPoses[posenmbr].Eye = cam.eye;
		sCameraPoses[posenmbr].Dir = cam.dir;
		sCameraPoses[posenmbr].Up  = cam.up;
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
		cam.eye = sCameraPoses[sPoseRequest].Eye;
		cam.update_dir(sCameraPoses[sPoseRequest].Dir, sCameraPoses[sPoseRequest].Up);
		iter		 = 0;
		sPoseRequest = -1;
	}

	sLastCameraPose = CameraPose(cam);

	return false;
}

static void update_texture(uint32_t* buf, SDL_Texture* texture, size_t width, size_t height, uint32_t iter)
{
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

	read_pose_file();
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
		ImGui::Text("SPP %i", iter /* get_spp()*/);
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
		if (ImGui::Button("Reload"))
			read_pose_file();
		ImGui::SameLine();
		if (ImGui::Button("Save"))
			write_pose_file();

		bool f = false;
		for (size_t i = 0; i < sCameraPoses.size(); ++i) {
			std::stringstream sstream;
			sstream << i + 1 << " | " << sCameraPoses[i].Eye(0) << " " << sCameraPoses[i].Eye(1) << " " << sCameraPoses[i].Eye(2);
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
		//make_screenshot(sWidth, sHeight, iter);
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
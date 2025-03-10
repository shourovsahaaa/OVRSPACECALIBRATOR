﻿#include "stdafx.h"
#include "Calibration.h"
#include "Configuration.h"
#include "EmbeddedFiles.h"
#include "UserInterface.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <implot/implot.h>
#include <GL/gl3w.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <openvr.h>
#include <direct.h>
#include <chrono>
#include <thread>
#include <dwmapi.h>
#include <algorithm>

#include <stb_image.h>

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define OPENVR_APPLICATION_KEY "pushrax.SpaceCalibrator"
std::string c_SPACE_CALIBRATOR_STEAM_APP_ID = "3368750";
std::string c_STEAMVR_STEAM_APP_ID = "250820";
constexpr const char* STEAM_MUTEX_KEY = "Global\\MUTEX__SpaceCalibrator_Steam";
HANDLE hSteamMutex = INVALID_HANDLE_VALUE;

extern "C" __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;

void CreateConsole()
{
	static bool created = false;
	if (!created)
	{
		AllocConsole();
		FILE *file = nullptr;
		freopen_s(&file, "CONIN$", "r", stdin);
		freopen_s(&file, "CONOUT$", "w", stdout);
		freopen_s(&file, "CONOUT$", "w", stderr);
		created = true;
	}
}

//#define DEBUG_LOGS

void GLFWErrorCallback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void openGLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
	fprintf(stderr, "OpenGL Debug %u: %.*s\n", id, length, message);
}

static void HandleCommandLine(LPWSTR lpCmdLine);

static GLFWwindow *glfwWindow = nullptr;
static vr::VROverlayHandle_t overlayMainHandle = 0, overlayThumbnailHandle = 0;
static GLuint fboHandle = 0, fboTextureHandle = 0;
static int fboTextureWidth = 0, fboTextureHeight = 0;

static char cwd[MAX_PATH];
const float MINIMIZED_MAX_FPS = 60.0f;

enum DWMA_USE_IMMSERSIVE_DARK_MODE_ENUM {
	DWMA_USE_IMMERSIVE_DARK_MODE = 20,
	DWMA_USE_IMMERSIVE_DARK_MODE_PRE_20H1 = 19,
};

const bool EnableDarkModeTopBar(const HWND windowHwmd) {
	const BOOL darkBorder = TRUE;
	const bool ok =
		SUCCEEDED(DwmSetWindowAttribute(windowHwmd, DWMA_USE_IMMERSIVE_DARK_MODE, &darkBorder, sizeof(darkBorder)))
		|| SUCCEEDED(DwmSetWindowAttribute(windowHwmd, DWMA_USE_IMMERSIVE_DARK_MODE_PRE_20H1, &darkBorder, sizeof(darkBorder)));
	return ok;
}

void CreateGLFWWindow()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, false);

#ifdef DEBUG_LOGS
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

	fboTextureWidth = 1200;
	fboTextureHeight = 800;

	glfwWindow = glfwCreateWindow(fboTextureWidth, fboTextureHeight, "Space Calibrator", nullptr, nullptr);
	if (!glfwWindow)
		throw std::runtime_error("Failed to create window");

	glfwMakeContextCurrent(glfwWindow);
	glfwSwapInterval(1);
	gl3wInit();

	// Minimise the window
	glfwIconifyWindow(glfwWindow);
	HWND windowHwmd = glfwGetWin32Window(glfwWindow);
	EnableDarkModeTopBar(windowHwmd);

	// Load icon and set it in the window
	GLFWimage images[1] = {};
	std::string iconPath = cwd;
	iconPath += "\\taskbar_icon.png";
	images[0].pixels = stbi_load(iconPath.c_str(), &images[0].width, &images[0].height, 0, 4);
	glfwSetWindowIcon(glfwWindow, 1, images);
	stbi_image_free(images[0].pixels);

#ifdef DEBUG_LOGS
	glDebugMessageCallback(openGLDebugCallback, nullptr);
	glEnable(GL_DEBUG_OUTPUT);
#endif

	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.IniFilename = nullptr;
	io.Fonts->AddFontFromMemoryCompressedTTF(DroidSans_compressed_data, DroidSans_compressed_size, 24.0f);

	ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	ImGui::StyleColorsDark();

	glGenTextures(1, &fboTextureHandle);
	glBindTexture(GL_TEXTURE_2D, fboTextureHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboTextureWidth, fboTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glGenFramebuffers(1, &fboHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, fboHandle);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fboTextureHandle, 0);

	GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		throw std::runtime_error("OpenGL framebuffer incomplete");
	}
}

void TryCreateVROverlay() {
	if (overlayMainHandle || !vr::VROverlay())
		return;

	vr::VROverlayError error = vr::VROverlay()->CreateDashboardOverlay(
		OPENVR_APPLICATION_KEY, "Space Calibrator",
		&overlayMainHandle, &overlayThumbnailHandle
	);

	if (error == vr::VROverlayError_KeyInUse) {
		throw std::runtime_error("Another instance of Space Calibrator is already running");
	} else if (error != vr::VROverlayError_None) {
		throw std::runtime_error("Error creating VR overlay: " + std::string(vr::VROverlay()->GetOverlayErrorNameFromEnum(error)));
	}

	vr::VROverlay()->SetOverlayWidthInMeters(overlayMainHandle, 3.0f);
	vr::VROverlay()->SetOverlayInputMethod(overlayMainHandle, vr::VROverlayInputMethod_Mouse);
	vr::VROverlay()->SetOverlayFlag(overlayMainHandle, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);

	std::string iconPath = cwd;
	iconPath += "\\icon.png";
	vr::VROverlay()->SetOverlayFromFile(overlayThumbnailHandle, iconPath.c_str());
}

void ActivateMultipleDrivers()
{
	vr::EVRSettingsError vrSettingsError;
	bool enabled = vr::VRSettings()->GetBool(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_ActivateMultipleDrivers_Bool, &vrSettingsError);

	if (vrSettingsError != vr::VRSettingsError_None)
	{
		std::string err = "Could not read \"" + std::string(vr::k_pch_SteamVR_ActivateMultipleDrivers_Bool) + "\" setting: "
			+ vr::VRSettings()->GetSettingsErrorNameFromEnum(vrSettingsError);

		throw std::runtime_error(err);
	}

	if (!enabled)
	{
		vr::VRSettings()->SetBool(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_ActivateMultipleDrivers_Bool, true, &vrSettingsError);
		if (vrSettingsError != vr::VRSettingsError_None)
		{
			std::string err = "Could not set \"" + std::string(vr::k_pch_SteamVR_ActivateMultipleDrivers_Bool) + "\" setting: "
				+ vr::VRSettings()->GetSettingsErrorNameFromEnum(vrSettingsError);

			throw std::runtime_error(err);
		}

		std::cerr << "Enabled \"" << vr::k_pch_SteamVR_ActivateMultipleDrivers_Bool << "\" setting" << std::endl;
	}
	else
	{
		std::cerr << "\"" << vr::k_pch_SteamVR_ActivateMultipleDrivers_Bool << "\" setting previously enabled" << std::endl;
	}
}

void InitVR()
{
	auto initError = vr::VRInitError_None;
	vr::VR_Init(&initError, vr::VRApplication_Overlay);
	if (initError != vr::VRInitError_None) {
		auto error = vr::VR_GetVRInitErrorAsEnglishDescription(initError);
		throw std::runtime_error("OpenVR error:" + std::string(error));
	}

	if (!vr::VR_IsInterfaceVersionValid(vr::IVRSystem_Version)) {
		throw std::runtime_error("OpenVR error: Outdated IVRSystem_Version");
	} else if (!vr::VR_IsInterfaceVersionValid(vr::IVRSettings_Version)) {
		throw std::runtime_error("OpenVR error: Outdated IVRSettings_Version");
	} else if (!vr::VR_IsInterfaceVersionValid(vr::IVROverlay_Version)) {
		throw std::runtime_error("OpenVR error: Outdated IVROverlay_Version");
	}

	ActivateMultipleDrivers();
}

static char textBuf[0x400];

static bool immediateRedraw;
void RequestImmediateRedraw() {
	immediateRedraw = true;
}

double lastFrameStartTime = glfwGetTime();
void RunLoop() {
	while (!glfwWindowShouldClose(glfwWindow))
	{
		TryCreateVROverlay();
		double time = glfwGetTime();
		CalibrationTick(time);

		bool dashboardVisible = false;
		int width, height;
		glfwGetFramebufferSize(glfwWindow, &width, &height);
		const bool windowVisible = (width > 0 && height > 0);

		if (overlayMainHandle && vr::VROverlay())
		{
			auto &io = ImGui::GetIO();
			dashboardVisible = vr::VROverlay()->IsActiveDashboardOverlay(overlayMainHandle);

			static bool keyboardOpen = false, keyboardJustClosed = false;

			// After closing the keyboard, this code waits one frame for ImGui to pick up the new text from SetActiveText
			// before clearing the active widget. Then it waits another frame before allowing the keyboard to open again,
			// otherwise it will do so instantly since WantTextInput is still true on the second frame.
			if (keyboardJustClosed && keyboardOpen)
			{
				ImGui::ClearActiveID();
				keyboardOpen = false;
			}
			else if (keyboardJustClosed)
			{
				keyboardJustClosed = false;
			}
			else if (!io.WantTextInput)
			{
				// User might close the keyboard without hitting Done, so we unset the flag to allow it to open again.
				keyboardOpen = false;
			}
			else if (io.WantTextInput && !keyboardOpen && !keyboardJustClosed)
			{
				int id = ImGui::GetActiveID();
				auto textInfo = ImGui::GetInputTextState(id);

				if (textInfo != nullptr) {
					textBuf[0] = 0;
					int len = WideCharToMultiByte(CP_ACP, 0, (LPCWCH)textInfo->TextA.Data, textInfo->TextA.Size, textBuf, sizeof(textBuf), nullptr, nullptr);
					textBuf[std::min(static_cast<size_t>(len), sizeof(textBuf) - 1)] = 0;

					uint32_t unFlags = 0; // EKeyboardFlags 

					vr::VROverlay()->ShowKeyboardForOverlay(
						overlayMainHandle, vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine,
						unFlags, "Space Calibrator Overlay", sizeof textBuf, textBuf, 0
					);
					keyboardOpen = true;
				}
			}

			vr::VREvent_t vrEvent;
			while (vr::VROverlay()->PollNextOverlayEvent(overlayMainHandle, &vrEvent, sizeof(vrEvent)))
			{
				switch (vrEvent.eventType) {
				case vr::VREvent_MouseMove:
					io.AddMousePosEvent(vrEvent.data.mouse.x, vrEvent.data.mouse.y);
					break;
				case vr::VREvent_MouseButtonDown:
					io.AddMouseButtonEvent((vrEvent.data.mouse.button & vr::VRMouseButton_Left) == vr::VRMouseButton_Left ? 0 : 1, true);
					break;
				case vr::VREvent_MouseButtonUp:
					io.AddMouseButtonEvent((vrEvent.data.mouse.button & vr::VRMouseButton_Left) == vr::VRMouseButton_Left ? 0 : 1, false);
					break;
				case vr::VREvent_ScrollDiscrete:
				{
					float x = vrEvent.data.scroll.xdelta * 360.0f * 8.0f;
					float y = vrEvent.data.scroll.ydelta * 360.0f * 8.0f;
					io.AddMouseWheelEvent(x, y);
					break;
				}
				case vr::VREvent_KeyboardDone: {
					vr::VROverlay()->GetKeyboardText(textBuf, sizeof textBuf);

					int id = ImGui::GetActiveID();
					auto textInfo = ImGui::GetInputTextState(id);
					int bufSize = MultiByteToWideChar(CP_ACP, 0, textBuf, -1, nullptr, 0);
					textInfo->TextA.resize(bufSize);
					MultiByteToWideChar(CP_ACP, 0, textBuf, -1, (LPWSTR)textInfo->TextA.Data, bufSize);
					textInfo->CurLenA = bufSize;
					textInfo->CurLenA = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)textInfo->TextA.Data, textInfo->TextA.Size, nullptr, 0, nullptr, nullptr);
					
					keyboardJustClosed = true;
					break;
				}
				case vr::VREvent_Quit:
					return;
				}
			}
		}
		
		if (windowVisible || dashboardVisible)
		{
			auto &io = ImGui::GetIO();
			
			// These change state now, so we must execute these before doing our own modifications to the io state for VR
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();

			io.DisplaySize = ImVec2((float)fboTextureWidth, (float)fboTextureHeight);
			io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

			io.ConfigFlags = io.ConfigFlags & ~ImGuiConfigFlags_NoMouseCursorChange;
			if (dashboardVisible) {
				io.ConfigFlags = io.ConfigFlags | ImGuiConfigFlags_NoMouseCursorChange;
			}

			ImGui::NewFrame();

			BuildMainWindow(dashboardVisible);

			ImGui::Render();

			glBindFramebuffer(GL_FRAMEBUFFER, fboHandle);
			glViewport(0, 0, fboTextureWidth, fboTextureHeight);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			if (width && height)
			{
				glBindFramebuffer(GL_READ_FRAMEBUFFER, fboHandle);
				glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				glfwSwapBuffers(glfwWindow);
			}

			if (dashboardVisible)
			{
				vr::Texture_t vrTex = {
					.handle = (void*)
#if defined _WIN64 || defined _LP64
				(uint64_t)
#endif
						fboTextureHandle,
					.eType = vr::TextureType_OpenGL,
					.eColorSpace = vr::ColorSpace_Auto,
				};

				vr::HmdVector2_t mouseScale = { (float) fboTextureWidth, (float) fboTextureHeight };

				vr::VROverlay()->SetOverlayTexture(overlayMainHandle, &vrTex);
				vr::VROverlay()->SetOverlayMouseScale(overlayMainHandle, &mouseScale);
			}
		}

		const double dashboardInterval = 1.0 / 90.0; // fps
		double waitEventsTimeout = std::max(CalCtx.wantedUpdateInterval, dashboardInterval);

		if (dashboardVisible && waitEventsTimeout > dashboardInterval)
			waitEventsTimeout = dashboardInterval;

		if (immediateRedraw) {
			waitEventsTimeout = 0;
			immediateRedraw = false;
		}

		glfwWaitEventsTimeout(waitEventsTimeout);

		// If we're minimized rendering won't limit our frame rate so we need to do it ourselves.
		if (glfwGetWindowAttrib(glfwWindow, GLFW_ICONIFIED))
		{
			double targetFrameTime = 1 / MINIMIZED_MAX_FPS;
			double waitTime = targetFrameTime - (glfwGetTime() - lastFrameStartTime);
			if (waitTime > 0)
			{
				std::this_thread::sleep_for(std::chrono::duration<double>(waitTime));
			}
		
			lastFrameStartTime += targetFrameTime;
		}
	}
}

void VerifySetupCorrect() {
	if (!vr::VRApplications()->IsApplicationInstalled(OPENVR_APPLICATION_KEY)) {
		std::string manifestPath = std::format("{}\\{}", cwd, "manifest.vrmanifest");
		std::cout << "Adding manifest path: " << manifestPath << std::endl;
		// If manifest is not installed, try installing it, and set it to auto-start with SteamVR
		auto vrAppErr = vr::VRApplications()->AddApplicationManifest(manifestPath.c_str());
		if (vrAppErr != vr::VRApplicationError_None) {
			fprintf(stderr, "Failed to add manifest: %s\n", vr::VRApplications()->GetApplicationsErrorNameFromEnum(vrAppErr));
		} else {
			vr::VRApplications()->SetApplicationAutoLaunch(OPENVR_APPLICATION_KEY, true);
		}
	} else {
		// Application is already registered, do not alter settings
		std::cout << "Space Calibrator already registered with SteamVR. Skipping..." << std::endl;
	}
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	if (_getcwd(cwd, MAX_PATH) == nullptr) {
		// @TODO: Handle Invalid working dir case. Should never happen but you never know 
	}
	HandleCommandLine(lpCmdLine);

#ifdef DEBUG_LOGS
	CreateConsole();
#endif

	if (!glfwInit())
	{
		MessageBox(nullptr, L"Failed to initialize GLFW", L"", 0);
		return 0;
	}

	glfwSetErrorCallback(GLFWErrorCallback);

	char steamAppId[256] = {};
	DWORD result = GetEnvironmentVariableA("SteamAppId", steamAppId, sizeof(steamAppId));
	if (result > 0 && steamAppId != nullptr) {
		if (c_SPACE_CALIBRATOR_STEAM_APP_ID == steamAppId ||
			c_STEAMVR_STEAM_APP_ID == steamAppId) {
			// We got launched via the Steam client UI.
			hSteamMutex = CreateMutexA(NULL, FALSE, STEAM_MUTEX_KEY);
			if (hSteamMutex == nullptr) {
				hSteamMutex = INVALID_HANDLE_VALUE;
			}
			else {
				// mutex opened, check if we opened one, if so exit
				if (GetLastError() == ERROR_ALREADY_EXISTS) {
					CloseHandle(hSteamMutex);
					hSteamMutex = INVALID_HANDLE_VALUE;
					return 0;
				}
			}
		}
	}

	try {
		InitVR();
		VerifySetupCorrect();
		CreateGLFWWindow();
		InitCalibrator();
		LoadProfile(CalCtx);
		RunLoop();

		vr::VR_Shutdown();

		if (fboHandle)
			glDeleteFramebuffers(1, &fboHandle);

		if (fboTextureHandle)
			glDeleteTextures(1, &fboTextureHandle);

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImPlot::DestroyContext();
		ImGui::DestroyContext();
	}
	catch (std::runtime_error &e)
	{
		std::cerr << "Runtime error: " << e.what() << std::endl;
		wchar_t message[1024];
		swprintf(message, 1024, L"%hs", e.what());
		MessageBox(nullptr, message, L"Runtime Error", 0);
	}

	if (hSteamMutex != INVALID_HANDLE_VALUE && hSteamMutex != nullptr) {
		CloseHandle(hSteamMutex);
		hSteamMutex = nullptr;
	}

	if (glfwWindow)
		glfwDestroyWindow(glfwWindow);

	glfwTerminate();
	return 0;
}

static void HandleCommandLine(LPWSTR lpCmdLine)
{
	if (lstrcmp(lpCmdLine, L"-openvrpath") == 0)
	{
		auto vrErr = vr::VRInitError_None;
		vr::VR_Init(&vrErr, vr::VRApplication_Utility);
		if (vrErr == vr::VRInitError_None)
		{
			char cruntimePath[MAX_PATH] = { 0 };
			unsigned int pathLen;
			vr::VR_GetRuntimePath(cruntimePath, MAX_PATH, &pathLen);

			printf("%s", cruntimePath);
			vr::VR_Shutdown();
			exit(0);
		}
		fprintf(stderr, "Failed to initialize OpenVR: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(vrErr));
		vr::VR_Shutdown();
		exit(-2);
	}
	else if (lstrcmp(lpCmdLine, L"-installmanifest") == 0)
	{
		auto vrErr = vr::VRInitError_None;
		vr::VR_Init(&vrErr, vr::VRApplication_Utility);
		if (vrErr == vr::VRInitError_None)
		{
			if (vr::VRApplications()->IsApplicationInstalled(OPENVR_APPLICATION_KEY))
			{
				char oldWd[MAX_PATH] = { 0 };
				auto vrAppErr = vr::VRApplicationError_None;
				vr::VRApplications()->GetApplicationPropertyString(OPENVR_APPLICATION_KEY, vr::VRApplicationProperty_WorkingDirectory_String, oldWd, MAX_PATH, &vrAppErr);
				if (vrAppErr != vr::VRApplicationError_None)
				{
					fprintf(stderr, "Failed to get old working dir, skipping removal: %s\n", vr::VRApplications()->GetApplicationsErrorNameFromEnum(vrAppErr));
				}
				else
				{
					std::string manifestPath = oldWd;
					manifestPath += "\\manifest.vrmanifest";
					std::cout << "Removing old manifest path: " << manifestPath << std::endl;
					vr::VRApplications()->RemoveApplicationManifest(manifestPath.c_str());
				}
			}
			std::string manifestPath = cwd;
			manifestPath += "\\manifest.vrmanifest";
			std::cout << "Adding manifest path: " << manifestPath << std::endl;
			auto vrAppErr = vr::VRApplications()->AddApplicationManifest(manifestPath.c_str());
			if (vrAppErr != vr::VRApplicationError_None)
			{
				fprintf(stderr, "Failed to add manifest: %s\n", vr::VRApplications()->GetApplicationsErrorNameFromEnum(vrAppErr));
			}
			else
			{
				vr::VRApplications()->SetApplicationAutoLaunch(OPENVR_APPLICATION_KEY, true);
			}
			vr::VR_Shutdown();
			exit(-2);
		}
		fprintf(stderr, "Failed to initialize OpenVR: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(vrErr));
		vr::VR_Shutdown();
		exit(-2);
	}
	else if (lstrcmp(lpCmdLine, L"-removemanifest") == 0)
	{
		auto vrErr = vr::VRInitError_None;
		vr::VR_Init(&vrErr, vr::VRApplication_Utility);
		if (vrErr == vr::VRInitError_None)
		{
			if (vr::VRApplications()->IsApplicationInstalled(OPENVR_APPLICATION_KEY))
			{
				std::string manifestPath = cwd;
				manifestPath += "\\manifest.vrmanifest";
				std::cout << "Removing manifest path: " << manifestPath << std::endl;
				vr::VRApplications()->RemoveApplicationManifest(manifestPath.c_str());
			}
			vr::VR_Shutdown();
			exit(0);
		}
		fprintf(stderr, "Failed to initialize OpenVR: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(vrErr));
		vr::VR_Shutdown();
		exit(-2);
	}
	else if (lstrcmp(lpCmdLine, L"-activatemultipledrivers") == 0)
	{
		int ret = -2;
		auto vrErr = vr::VRInitError_None;
		vr::VR_Init(&vrErr, vr::VRApplication_Utility);
		if (vrErr == vr::VRInitError_None)
		{
			try
			{
				ActivateMultipleDrivers();
				ret = 0;
			}
			catch (std::runtime_error &e)
			{
				std::cerr << e.what() << std::endl;
			}
		}
		else
		{
			fprintf(stderr, "Failed to initialize OpenVR: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(vrErr));
		}
		vr::VR_Shutdown();
		exit(ret);
	}
}

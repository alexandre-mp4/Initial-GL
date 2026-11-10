// fenêtre, entrées, caméras et machine à états du jeu
#ifndef GAME_H
#define GAME_H

#define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <string>
#include <vector>
#include "AssetManager.h"
#include "Renderer.h"
#include "Scene.h"
#include "Camera.h"
#include "Ui.h"
#include "Hud.h"
#include "Session.h"
#include "Audio.h"

enum GameState { STATE_MAIN_MENU, STATE_CAR_SELECT, STATE_SETTINGS, STATE_PLAYING, STATE_PAUSED, STATE_RESULTS };

// poursuite, capot, cockpit, orbitale, libre (touche C)
enum CameraMode { CAM_CHASE, CAM_HOOD, CAM_COCKPIT, CAM_ORBIT, CAM_FPS };

class Game
{
public:

	Game();
	bool init();
	void run();

private:

	bool initOpenGL();
	void processInput(double deltaTime);
	void updateCamera(double deltaTime);
	Camera& activeCamera();
	const char* cameraName() const;
	void showFPS();

	void setState(GameState s);
	void startSession(GameMode mode, bool skipCountdown = false);
	void endSession();
	void handleKey(int key, int action);
	void handleMenuKey(int key, int action);
	void activateItem();
	void adjustItem(int dir);
	void selectCar(int index);
	int  menuItemCount() const;
	void drawInterface(int fbW, int fbH);
	void drawMainMenu();
	void drawCarSelect();
	void drawSettings();
	void drawPause();
	void drawResults();
	void drawMenuList(const std::vector<std::string>& items, float x, float y);
	void drawStatBar(float x, float y, float w, const std::string& label, float value01, const std::string& text);
	void loadSettings();
	void saveSettings() const;

	// les callbacks GLFW n'ont pas de this : ils retrouvent le Game avec glfwSetWindowUserPointer
	static void onKey(GLFWwindow* window, int key, int scancode, int action, int mode);
	static void onFramebufferSize(GLFWwindow* window, int width, int height);
	static void onScroll(GLFWwindow* window, double deltaX, double deltaY);
	static Game* from(GLFWwindow* window);

	GLFWwindow* mWindow;
	int mWindowWidth, mWindowHeight;
	GameState mState, mSettingsReturn;
	bool mWireframe, mFlashlightOn;
	int mMenuSel;
	float mMenuTime;
	float mFade;

	AssetManager mAssets;
	Renderer mRenderer;
	Scene mScene;
	Ui mUi;
	Hud mHud;
	Audio mAudio;
	void updateAudio(float dt);
	void playStateMusic();
	float mPrevImpact = 0.0f;
	float mThrottleInput = 0.0f;
	int mPrevCount = 0;
	bool mPrevRacing = false;
	GameState mMusicState = STATE_RESULTS;
	Session mSession;
	float mBestTime, mBestScore;
	bool mNewRecord;
	float mSinceStart;
	int mCarIndex;
	CarTuning mSavedTuning;

	CameraMode mCameraMode, mPreviousCameraMode;
	FPSCamera mFpsCamera;
	ChaseCamera mChaseCamera;
	OrbitCamera mOrbitCamera;
	HoodCamera mHoodCamera;
	float mOrbitAngle;
};
#endif

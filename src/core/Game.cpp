#include "Game.h"
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "glm/gtc/matrix_transform.hpp"
#include "UiStyle.h"

static const float MOON_AZIMUTH_OFFSET_DEG = 35.0f;
static const float MOON_ELEVATION_DEG = 24.0f;
static const char* APP_TITLE = "Initial GL";
static const float ORBIT_SPEED = 20.0f;
static const double ZOOM_SENSITIVITY = -3.0;
static const float MOVE_SPEED = 15.0f;
static const float MOUSE_SENSITIVITY = 0.1f;
static const glm::vec4 CLEAR_COLOR(0.01f, 0.01f, 0.03f, 1.0f);
static const char* SETTINGS_FILE = "settings.cfg";

static const int NUM_CARS = 3;
static const char* CAR_IDS[NUM_CARS] = { "ae86", "rx7", "r34" };

static const glm::vec4 UI_WHITE(1.0f, 1.0f, 1.0f, 1.0f);
static const glm::vec4 UI_DIM(1.0f, 1.0f, 1.0f, 0.55f);
static const glm::vec4 UI_ACCENT(1.0f, 0.62f, 0.12f, 1.0f);
static const glm::vec4 UI_PANEL(0.02f, 0.03f, 0.06f, 0.62f);

Game::Game()
	: mWindow(NULL), mWindowWidth(1280), mWindowHeight(800), mState(STATE_MAIN_MENU), mSettingsReturn(STATE_MAIN_MENU),
	  mWireframe(false), mFlashlightOn(false),
	  mMenuSel(0), mMenuTime(0.0f), mFade(1.0f), mBestTime(0.0f), mBestScore(0.0f), mNewRecord(false), mSinceStart(0.0f), mCarIndex(1),
	  mCameraMode(CAM_CHASE), mPreviousCameraMode(CAM_CHASE),
	  mFpsCamera(glm::vec3(0.0f, 3.5f, 10.0f)),
	  mChaseCamera(9.0f, 3.5f, 6.0f),
	  mOrbitAngle(30.0f)
{
}

bool Game::init()
{
	if (!initOpenGL())
		return false;

	int fbw, fbh;
	glfwGetFramebufferSize(mWindow, &fbw, &fbh);
	if (!mRenderer.init(fbw, fbh))
		return false;
	if (!mUi.init("assets/fonts/Rajdhani-Medium.ttf", "assets/fonts/Rajdhani-Bold.ttf", "assets/fonts/MPLUS1p-Black.ttf"))
		return false;

	mAudio.init();

	loadSettings();

	if (!mScene.load(mAssets, CAR_IDS[mCarIndex]))
		return false;

	// toutes les voitures sont chargées au départ pour changer instantanément en menu
	for (int i = 0; i < NUM_CARS; i++)
		mAssets.model(std::string("assets/models/cars/") + CAR_IDS[i] + ".glb");
	mScene.getCar().tuning = mSavedTuning;
	mHud.setCourse(mScene.getRoad());

	mRenderer.getEnvironment().setMoon(mScene.getCar().getYaw() + glm::radians(MOON_AZIMUTH_OFFSET_DEG), glm::radians(MOON_ELEVATION_DEG));

	const Car& car = mScene.getCar();
	mFpsCamera.setPosition(mScene.getStartPosition() - 12.0f * mScene.getStartDirection() + glm::vec3(0.0f, 5.0f, 0.0f));
	mFpsCamera.rotate(glm::degrees(car.getYaw() - glm::pi<float>()), -20.0f);
	mChaseCamera.reset(car.getPosition(), car.getCameraYaw());

	setState(STATE_MAIN_MENU);
	return true;
}

// réglages : fichier texte « clé valeur »
void Game::loadSettings()
{
	std::ifstream in(SETTINGS_FILE);
	std::string key;
	while (in >> key)
	{
		if (key == "car")
		{
			std::string id; in >> id;
			for (int i = 0; i < NUM_CARS; i++) if (id == CAR_IDS[i]) mCarIndex = i;
			continue;
		}
		float v;
		if (!(in >> v)) break;
		if (key == "quality") mRenderer.setQuality((int)v);
		else if (key == "assist") mSavedTuning.steerAssist = glm::clamp(v, 0.0f, 1.0f);
		else if (key == "grip") mSavedTuning.gripScale = glm::clamp(v, 0.6f, 1.5f);
		else if (key == "power") mSavedTuning.powerScale = glm::clamp(v, 0.5f, 2.0f);
		else if (key == "traction") mSavedTuning.tractionAid = glm::clamp(v, 0.0f, 1.0f);
		else if (key == "driftmax") mSavedTuning.driftAngleMax = glm::clamp(v, 0.35f, 1.2f);
		else if (key == "music") mAudio.musicVolume = glm::clamp(v, 0.0f, 1.0f);
		else if (key == "sfx") mAudio.sfxVolume = glm::clamp(v, 0.0f, 1.0f);
	}
}

void Game::saveSettings() const
{
	std::ofstream out(SETTINGS_FILE);
	const CarTuning& t = mScene.getCar().tuning;
	out << "car " << CAR_IDS[mCarIndex] << "\nquality " << mRenderer.getQuality() << "\nassist " << t.steerAssist
		<< "\nmusic " << mAudio.musicVolume << "\nsfx " << mAudio.sfxVolume << "\ngrip " << t.gripScale << "\npower " << t.powerScale << "\ntraction " << t.tractionAid << "\ndriftmax " << t.driftAngleMax << "\n";
}

void Game::setState(GameState s)
{
	mState = s;
	mMenuSel = 0;
	mMenuTime = 0.0f;
	mFade = 1.0f;
}

void Game::selectCar(int index)
{
	mCarIndex = (index % NUM_CARS + NUM_CARS) % NUM_CARS;
	mScene.changeCar(mAssets, CAR_IDS[mCarIndex]);
	mBestTime = Session::loadBestTime(CAR_IDS[mCarIndex]);
	mBestScore = Session::loadBestScore(CAR_IDS[mCarIndex]);
}

void Game::startSession(GameMode mode, bool skipCountdown)
{
	mScene.resetToStart();
	mSession.start(mode, mScene.getFinishIndex(), mScene.getRoad().getNumPoints());
	if (skipCountdown) { mSession.countdown = 0.0f; mSession.racing = true; }
	mBestTime = Session::loadBestTime(CAR_IDS[mCarIndex]);
	mBestScore = Session::loadBestScore(CAR_IDS[mCarIndex]);
	mNewRecord = false;
	mSinceStart = 0.0f;
	mChaseCamera.reset(mScene.getCar().getPosition(), mScene.getCar().getCameraYaw());

	mCameraMode = CAM_CHASE;
	mPreviousCameraMode = CAM_ORBIT;
	setState(STATE_PLAYING);
	mFade = skipCountdown ? 0.0f : 1.0f;
}

void Game::endSession()
{
	const std::string id = CAR_IDS[mCarIndex];
	if (mSession.chainScore > 0.0f) { mSession.totalScore += mSession.chainScore; mSession.chainScore = 0.0f; }
	mNewRecord = false;
	if (mSession.mode == MODE_TIME_ATTACK && mSession.finished && (mBestTime <= 0.0f || mSession.time < mBestTime))
	{
		Session::saveBestTime(id, mSession.time);
		mNewRecord = true;
	}
	if (mSession.totalScore > mBestScore && mSession.totalScore > 0.0f)
	{
		Session::saveBestScore(id, mSession.totalScore);
		mNewRecord = true;
	}
	if (mNewRecord) mAudio.blip(BLIP_RECORD);
	setState(STATE_RESULTS);
}

void Game::run()
{
	double lastTime = glfwGetTime();

	while (!glfwWindowShouldClose(mWindow))
	{
		showFPS();

		double now = glfwGetTime();
		double deltaTime = glm::min(now - lastTime, 0.1);
		lastTime = now;

		glfwPollEvents();
		mRenderer.time = (float)now;
		mMenuTime += (float)deltaTime;
		mFade = glm::max(0.0f, mFade - (float)deltaTime * 1.6f);

		if (mState == STATE_PLAYING)
		{
			mSinceStart += (float)deltaTime;
			processInput(deltaTime);
			mSession.update((float)deltaTime, mScene.getCar(), mScene.getNearestRoadIndex());
			if (mSession.finished)
				endSession();
		}
		else if (mState != STATE_PAUSED)
		{
			// en menu la voiture reste immobile (frein à main), la physique tourne quand même
			CarInput hold;
			hold.handbrake = true;
			if (mState == STATE_RESULTS) hold = CarInput();
			mScene.update((float)deltaTime, hold);
		}
		updateCamera(deltaTime);

		int fbW, fbH;
		glfwGetFramebufferSize(mWindow, &fbW, &fbH);
		mScene.render(mRenderer, activeCamera(), (float)mWindowWidth / (float)mWindowHeight, mFlashlightOn);
		drawInterface(fbW, fbH);

		updateAudio((float)deltaTime);
		glfwSwapBuffers(mWindow);
	}

	saveSettings();
	glfwTerminate();
}

bool Game::initOpenGL()
{
	if (!glfwInit())
	{
		std::cerr << "GLFW initialization failed" << std::endl;
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, APP_TITLE, NULL, NULL);
	if (mWindow == NULL)
	{
		std::cerr << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return false;
	}

	glfwMakeContextCurrent(mWindow);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return false;
	}

	glfwSetWindowUserPointer(mWindow, this);
	glfwSetKeyCallback(mWindow, onKey);
	glfwSetFramebufferSizeCallback(mWindow, onFramebufferSize);
	glfwSetScrollCallback(mWindow, onScroll);

	glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPos(mWindow, mWindowWidth / 2.0, mWindowHeight / 2.0);

	glClearColor(CLEAR_COLOR.r, CLEAR_COLOR.g, CLEAR_COLOR.b, CLEAR_COLOR.a);

	int w, h;
	glfwGetFramebufferSize(mWindow, &w, &h);
	glViewport(0, 0, w, h);

	glEnable(GL_DEPTH_TEST);

	return true;
}

Game* Game::from(GLFWwindow* window)
{
	return static_cast<Game*>(glfwGetWindowUserPointer(window));
}

void Game::onKey(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	from(window)->handleKey(key, action);
}

void Game::onFramebufferSize(GLFWwindow* window, int width, int height)
{
	Game* g = from(window);

	// width et height sont en pixels, mWindowWidth et mWindowHeight en points (souris)
	glfwGetWindowSize(window, &g->mWindowWidth, &g->mWindowHeight);

	int w, h;
	glfwGetFramebufferSize(window, &w, &h);
	glViewport(0, 0, w, h);
	g->mRenderer.resize(w, h);
}

void Game::onScroll(GLFWwindow* window, double deltaX, double deltaY)
{
	Game* g = from(window);
	if (g->mState != STATE_PLAYING) return;
	Camera& camera = g->activeCamera();
	double fov = glm::clamp(camera.getFOV() + deltaY * ZOOM_SENSITIVITY, 1.0, 120.0);
	camera.setFOV((float)fov);
}

void Game::handleKey(int key, int action)
{
	bool press = (action == GLFW_PRESS);
	if (press && key == GLFW_KEY_F1)
	{
		mWireframe = !mWireframe;
		mRenderer.setWireframe(mWireframe);
	}

	if (mState != STATE_PLAYING)
	{
		handleMenuKey(key, action);
		return;
	}
	if (!press) return;

	switch (key)
	{
	case GLFW_KEY_ESCAPE: setState(STATE_PAUSED); mFade = 0.0f; mAudio.blip(BLIP_BACK); break;
	case GLFW_KEY_F: mFlashlightOn = !mFlashlightOn; break;
	case GLFW_KEY_C: mCameraMode = (CameraMode)((mCameraMode + 1) % 5); break;
	case GLFW_KEY_V: mCameraMode = (mCameraMode == CAM_ORBIT) ? CAM_CHASE : CAM_ORBIT; break;
	case GLFW_KEY_L: mScene.headlightsOn = !mScene.headlightsOn; break;
	case GLFW_KEY_N: mScene.neonsOn = !mScene.neonsOn; break;
	case GLFW_KEY_M: mScene.neonColorIndex = (mScene.neonColorIndex + 1) % Scene::NUM_NEON_COLORS; break;
	case GLFW_KEY_Q: mRenderer.setQuality(mRenderer.getQuality() + 1); break;
	case GLFW_KEY_R: mScene.respawnCar(); break;
	case GLFW_KEY_F3: mScene.debugDraw = !mScene.debugDraw; break;

	// réglages rapides : 1/2 contre-braquage, 3/4 adhérence, 5/6 puissance, 7/8 motricité
	case GLFW_KEY_1: mScene.getCar().tuning.steerAssist = glm::max(0.0f, mScene.getCar().tuning.steerAssist - 0.15f); break;
	case GLFW_KEY_2: mScene.getCar().tuning.steerAssist = glm::min(1.0f, mScene.getCar().tuning.steerAssist + 0.15f); break;
	case GLFW_KEY_3: mScene.getCar().tuning.gripScale = glm::max(0.6f, mScene.getCar().tuning.gripScale - 0.1f); break;
	case GLFW_KEY_4: mScene.getCar().tuning.gripScale = glm::min(1.5f, mScene.getCar().tuning.gripScale + 0.1f); break;
	case GLFW_KEY_5: mScene.getCar().tuning.powerScale = glm::max(0.5f, mScene.getCar().tuning.powerScale - 0.1f); break;
	case GLFW_KEY_6: mScene.getCar().tuning.powerScale = glm::min(2.0f, mScene.getCar().tuning.powerScale + 0.1f); break;
	case GLFW_KEY_7: mScene.getCar().tuning.tractionAid = glm::max(0.0f, mScene.getCar().tuning.tractionAid - 0.25f); break;
	case GLFW_KEY_8: mScene.getCar().tuning.tractionAid = glm::min(1.0f, mScene.getCar().tuning.tractionAid + 0.25f); break;
	default: break;
	}
}

int Game::menuItemCount() const
{
	switch (mState)
	{
	case STATE_MAIN_MENU: return 5;
	case STATE_PAUSED:    return 5;
	case STATE_SETTINGS:  return 9;
	case STATE_RESULTS:   return 2;
	default: return 1;
	}
}

void Game::handleMenuKey(int key, int action)
{
	if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
	bool press = (action == GLFW_PRESS);
	int n = menuItemCount();

	if (key == GLFW_KEY_UP || key == GLFW_KEY_W)   { mMenuSel = (mMenuSel + n - 1) % n; mAudio.blip(BLIP_MOVE); return; }
	if (key == GLFW_KEY_DOWN || key == GLFW_KEY_S) { mMenuSel = (mMenuSel + 1) % n; mAudio.blip(BLIP_MOVE); return; }
	if (key == GLFW_KEY_LEFT)  { adjustItem(-1); mAudio.blip(BLIP_MOVE); return; }
	if (key == GLFW_KEY_RIGHT) { adjustItem(+1); mAudio.blip(BLIP_MOVE); return; }
	if (!press) return;
	if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE) { mAudio.blip(BLIP_SELECT); activateItem(); return; }

	if (key == GLFW_KEY_ESCAPE)
	{
		mAudio.blip(BLIP_BACK);
		switch (mState)
		{
		case STATE_CAR_SELECT: saveSettings(); setState(STATE_MAIN_MENU); mMenuSel = 2; break;
		case STATE_SETTINGS:   saveSettings(); setState(mSettingsReturn); mFade = 0.0f; if (mSettingsReturn == STATE_PAUSED) mMenuSel = 2; else mMenuSel = 3; break;
		case STATE_PAUSED:     setState(STATE_PLAYING); mFade = 0.0f; break;
		case STATE_RESULTS:    setState(STATE_MAIN_MENU); break;
		default: break;
		}
	}
}

void Game::adjustItem(int dir)
{
	CarTuning& t = mScene.getCar().tuning;
	if (mState == STATE_CAR_SELECT || (mState == STATE_MAIN_MENU && mMenuSel == 2))
	{
		selectCar(mCarIndex + dir);
		return;
	}
	if (mState != STATE_SETTINGS) return;
	switch (mMenuSel)
	{
	case 0: mRenderer.setQuality(glm::clamp(mRenderer.getQuality() + dir, 0, Renderer::NUM_PRESETS - 1)); break;
	case 1: t.steerAssist = glm::clamp(t.steerAssist + 0.1f * dir, 0.0f, 1.0f); break;
	case 2: t.gripScale = glm::clamp(t.gripScale + 0.1f * dir, 0.6f, 1.5f); break;
	case 3: t.powerScale = glm::clamp(t.powerScale + 0.1f * dir, 0.5f, 2.0f); break;
	case 4: t.tractionAid = glm::clamp(t.tractionAid + 0.1f * dir, 0.0f, 1.0f); break;
	case 5: t.driftAngleMax = glm::clamp(t.driftAngleMax + 0.0873f * dir, 0.35f, 1.2f); break;
	case 6: mAudio.musicVolume = glm::clamp(mAudio.musicVolume + 0.1f * dir, 0.0f, 1.0f); break;
	case 7: mAudio.sfxVolume = glm::clamp(mAudio.sfxVolume + 0.1f * dir, 0.0f, 1.0f); mAudio.blip(BLIP_MOVE); break;
	default: break;
	}
}

void Game::activateItem()
{
	switch (mState)
	{
	case STATE_MAIN_MENU:
		if (mMenuSel == 0) startSession(MODE_TIME_ATTACK);
		else if (mMenuSel == 1) startSession(MODE_FREE_DRIFT);
		else if (mMenuSel == 2) { setState(STATE_CAR_SELECT); selectCar(mCarIndex); }
		else if (mMenuSel == 3) { mSettingsReturn = STATE_MAIN_MENU; setState(STATE_SETTINGS); mFade = 0.0f; }
		else glfwSetWindowShouldClose(mWindow, GL_TRUE);
		break;
	case STATE_CAR_SELECT:
		saveSettings(); setState(STATE_MAIN_MENU); mMenuSel = 2;
		break;
	case STATE_SETTINGS:
		if (mMenuSel == 8) { saveSettings(); setState(mSettingsReturn); mFade = 0.0f; mMenuSel = (mSettingsReturn == STATE_PAUSED) ? 2 : 3; }
		break;
	case STATE_PAUSED:
		if (mMenuSel == 0) { setState(STATE_PLAYING); mFade = 0.0f; }
		else if (mMenuSel == 1) startSession(mSession.mode);
		else if (mMenuSel == 2) { mSettingsReturn = STATE_PAUSED; setState(STATE_SETTINGS); mFade = 0.0f; }
		else if (mMenuSel == 3) endSession();
		else { setState(STATE_MAIN_MENU); mScene.resetToStart(); }
		break;
	case STATE_RESULTS:
		if (mMenuSel == 0) startSession(mSession.mode);
		else { setState(STATE_MAIN_MENU); mScene.resetToStart(); }
		break;
	default: break;
	}
}

void Game::processInput(double deltaTime)
{
	CarInput input;
	input.throttle = (glfwGetKey(mWindow, GLFW_KEY_UP) == GLFW_PRESS) ? 1.0f : 0.0f;
	input.brake    = (glfwGetKey(mWindow, GLFW_KEY_DOWN) == GLFW_PRESS) ? 1.0f : 0.0f;
	if (glfwGetKey(mWindow, GLFW_KEY_LEFT) == GLFW_PRESS)  input.steer += 1.0f;
	if (glfwGetKey(mWindow, GLFW_KEY_RIGHT) == GLFW_PRESS) input.steer -= 1.0f;
	input.handbrake = (glfwGetKey(mWindow, GLFW_KEY_SPACE) == GLFW_PRESS);
	mThrottleInput = input.throttle;

	// pendant le compte à rebours la voiture est retenue
	if (mSession.countdown > 0.0f)
	{
		input = CarInput();
		input.handbrake = true;
	}
	mScene.update((float)deltaTime, input);

	double mouseX, mouseY;
	glfwGetCursorPos(mWindow, &mouseX, &mouseY);

	// la souris ne pilote que la caméra libre, mais le curseur est recentré à chaque image
	if (mCameraMode == CAM_FPS)
		mFpsCamera.rotate((float)(mWindowWidth / 2.0 - mouseX) * MOUSE_SENSITIVITY, (float)(mWindowHeight / 2.0 - mouseY) * MOUSE_SENSITIVITY);

	glfwSetCursorPos(mWindow, mWindowWidth / 2.0, mWindowHeight / 2.0);

	if (mCameraMode != CAM_FPS)
		return;

	float step = MOVE_SPEED * (float)deltaTime;

	if (glfwGetKey(mWindow, GLFW_KEY_W) == GLFW_PRESS)
		mFpsCamera.move(step * mFpsCamera.getLook());
	else if (glfwGetKey(mWindow, GLFW_KEY_S) == GLFW_PRESS)
		mFpsCamera.move(step * -mFpsCamera.getLook());

	if (glfwGetKey(mWindow, GLFW_KEY_A) == GLFW_PRESS)
		mFpsCamera.move(step * -mFpsCamera.getRight());
	else if (glfwGetKey(mWindow, GLFW_KEY_D) == GLFW_PRESS)
		mFpsCamera.move(step * mFpsCamera.getRight());

	if (glfwGetKey(mWindow, GLFW_KEY_Z) == GLFW_PRESS)
		mFpsCamera.move(step * glm::vec3(0.0f, 1.0f, 0.0f));
	else if (glfwGetKey(mWindow, GLFW_KEY_X) == GLFW_PRESS)
		mFpsCamera.move(step * glm::vec3(0.0f, -1.0f, 0.0f));
}

void Game::updateCamera(double deltaTime)
{
	const Car& car = mScene.getCar();
	glm::vec3 carPos = car.getPosition();

	if (mState != STATE_PLAYING && mState != STATE_PAUSED)
	{
		bool select = (mState == STATE_CAR_SELECT);
		mOrbitAngle = fmodf(mOrbitAngle + (select ? 28.0f : 9.0f) * (float)deltaTime, 360.0f);
		mOrbitCamera.setLookAt(carPos + glm::vec3(0.0f, select ? 0.65f : 0.9f, 0.0f));
		mOrbitCamera.setRadius(select ? 6.4f : 8.5f);
		mOrbitCamera.rotate(mOrbitAngle, select ? 9.0f : 13.0f);
		mCameraMode = CAM_ORBIT;
		mPreviousCameraMode = CAM_ORBIT;
		return;
	}
	if (mState == STATE_PAUSED)
		return;

	if (mCameraMode == CAM_CHASE)
	{
		if (mPreviousCameraMode != CAM_CHASE)
			mChaseCamera.reset(carPos, car.getCameraYaw());
		else
			mChaseCamera.follow(carPos, car.getCameraYaw(), (float)deltaTime);

		// la caméra ne passe pas sous le sol
		mChaseCamera.raiseTo(mScene.groundHeight(mChaseCamera.getPosition().x, mChaseCamera.getPosition().z) + 1.1f);
	}
	else if (mCameraMode == CAM_HOOD)
	{
		glm::vec3 pos = car.localToWorldPoint(glm::vec3(-0.32f, 1.18f, 0.55f));
		glm::vec3 fwd = car.localToWorldDirection(glm::vec3(0.0f, -0.05f, 1.0f));
		mHoodCamera.setPose(pos, fwd, car.localToWorldDirection(glm::vec3(0.0f, 1.0f, 0.0f)));
		if (mPreviousCameraMode != CAM_HOOD) mHoodCamera.setFOV(72.0f);
	}
	else if (mCameraMode == CAM_COCKPIT)
	{
		// vue conducteur : le regard suit en partie l'angle de dérive
		glm::vec3 pos = car.localToWorldPoint(mScene.getEyePosition());
		float look = glm::clamp(0.5f * car.getDriftAngle(), -0.35f, 0.35f);
		glm::vec3 fwd = car.localToWorldDirection(glm::vec3(sinf(look), -0.02f, cosf(look)));
		mHoodCamera.setPose(pos, fwd, car.localToWorldDirection(glm::vec3(0.0f, 1.0f, 0.0f)));
		if (mPreviousCameraMode != CAM_COCKPIT) mHoodCamera.setFOV(78.0f);
	}
	else if (mCameraMode == CAM_ORBIT)
	{
		// la caméra orbitale tourne avec le temps
		mOrbitAngle = fmodf(mOrbitAngle + ORBIT_SPEED * (float)deltaTime, 360.0f);
		mOrbitCamera.setLookAt(carPos + glm::vec3(0.0f, 1.0f, 0.0f));
		mOrbitCamera.setRadius(9.0f);
		mOrbitCamera.rotate(mOrbitAngle, 20.0f);
	}
	mPreviousCameraMode = mCameraMode;
}

Camera& Game::activeCamera()
{
	switch (mCameraMode)
	{
	case CAM_CHASE: return mChaseCamera;
	case CAM_HOOD:  return mHoodCamera;
	case CAM_COCKPIT: return mHoodCamera;
	case CAM_ORBIT: return mOrbitCamera;
	default:        return mFpsCamera;
	}
}

const char* Game::cameraName() const
{
	switch (mCameraMode)
	{
	case CAM_CHASE: return "poursuite";
	case CAM_HOOD:  return "capot";
	case CAM_COCKPIT: return "conducteur";
	case CAM_ORBIT: return "cinématique";
	default:        return "libre";
	}
}

void Game::showFPS()
{
	static double previousSeconds = 0.0;
	static int frameCount = 0;
	double currentSeconds = glfwGetTime();
	double elapsedSeconds = currentSeconds - previousSeconds;

	if (elapsedSeconds > 0.25)
	{
		previousSeconds = currentSeconds;
		double fps = (double)frameCount / elapsedSeconds;

		std::ostringstream outs;
		outs.precision(0);
		outs << std::fixed << APP_TITLE << "  —  " << (int)fps << " FPS  —  qualité " << mRenderer.getQualityPreset().name;
		if (mState == STATE_PLAYING)
			outs << "  —  caméra " << cameraName();
		glfwSetWindowTitle(mWindow, outs.str().c_str());
		frameCount = 0;
	}
	frameCount++;
}

void Game::playStateMusic()
{
	bool race = (mState == STATE_PLAYING || mState == STATE_PAUSED || (mState == STATE_SETTINGS && mSettingsReturn == STATE_PAUSED));
	static const std::vector<std::string> menuList = { "assets/music/Rocket.mp3" };
	static const std::vector<std::string> raceList = { "assets/music/Liquid_Flame.mp3", "assets/music/Beats_n_Games.mp3", "assets/music/Cyberpunk_Moonlight_Sonata.mp3", "assets/music/Synth_1.mp3" };
	mAudio.playMusic(race ? raceList : menuList);
}

void Game::updateAudio(float dt)
{
	playStateMusic();
	const Car& car = mScene.getCar();
	bool driving = (mState == STATE_PLAYING);

	mAudio.setEngine(car.getRpm(), mThrottleInput, mCarIndex == 0 ? 0 : (mCarIndex == 1 ? 1 : 2), driving);

	// crissement : dérive ou glissement longitudinal au-delà d'un seuil, selon la vitesse
	float slip = 0.0f;
	for (int i = 0; i < 4; i++)
	{
		const WheelState& w = car.getWheel(i);
		if (!w.contact) continue;
		slip = std::max(slip, std::max(fabsf(w.slipAngle) / 0.26f, fabsf(w.slipRatio) / 0.5f));
	}
	float spd = car.getSpeedKmh() / 3.6f;
	float screech = glm::clamp(slip - 0.55f, 0.0f, 1.0f) * glm::clamp(spd / 7.0f, 0.0f, 1.0f);
	mAudio.setTires(driving ? screech : 0.0f);
	mAudio.setWind(driving ? spd : 0.0f);

	// choc : seulement au front montant de la vitesse d'impact
	float imp = car.getLastImpact();
	if (driving && imp > 2.0f && imp > mPrevImpact + 1.0f)
		mAudio.impact(imp);
	mPrevImpact = imp;

	int count = (mState == STATE_PLAYING && mSession.countdown > 0.0f) ? (int)ceilf(mSession.countdown) : 0;
	if (count > 0 && count != mPrevCount) mAudio.blip(BLIP_COUNT);
	if (mPrevCount > 0 && count == 0 && mSession.racing) mAudio.blip(BLIP_GO);
	mPrevCount = count;

	mAudio.update(dt);
}

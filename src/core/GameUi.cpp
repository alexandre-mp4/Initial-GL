#include "Game.h"
#include <cmath>
#include <cstdio>
#include "UiStyle.h"

using namespace style;

static const char* CAR_IDS_UI[3] = { "ae86", "rx7", "r34" };
static const char* CAR_JP[3] = { "トヨタ スプリンター トレノ", "マツダ サバンナ RX-7", "ニッサン スカイライン GT-R" };

void Game::drawInterface(int fbW, int fbH)
{
	mUi.begin(fbW, fbH);

	switch (mState)
	{
	case STATE_MAIN_MENU:   drawMainMenu(); break;
	case STATE_CAR_SELECT:  drawCarSelect(); break;
	case STATE_SETTINGS:    drawSettings(); break;
	case STATE_PLAYING:     mHud.draw(mUi, mScene.getCar(), mSession, mBestTime, mSinceStart, (float)glfwGetTime()); break;
	case STATE_PAUSED:
		mHud.draw(mUi, mScene.getCar(), mSession, mBestTime, 99.0f, (float)glfwGetTime());
		drawPause();
		break;
	case STATE_RESULTS:     drawResults(); break;
	}
	mUi.flush();

	// fondu depuis le noir, dessiné en dernier pour tout recouvrir
	if (mFade > 0.0f)
	{
		mUi.rect(0.0f, 0.0f, mUi.width(), mUi.height(), glm::vec4(0.0f, 0.0f, 0.0f, mFade));
		mUi.flush();
	}
}

void Game::drawMenuList(const std::vector<std::string>& items, float x, float y)
{
	static const char* sub[] = { "", "", "", "", "" };
	(void)sub;
	for (size_t i = 0; i < items.size(); i++)
	{
		bool sel = ((int)i == mMenuSel);
		float py = y + i * 74.0f;
		float slide = sel ? 26.0f + 4.0f * sinf(mMenuTime * 6.0f) : 0.0f;
		float w = 560.0f, h = 58.0f;
		if (sel)
		{
			mUi.parallelogram(x + slide - 6.0f, py - 4.0f, w + 12.0f, h + 8.0f, SKEW, YELLOW);
			mUi.parallelogramGradient(x + slide, py, w, h, SKEW, RED, DARKRED);
			mUi.triangle(glm::vec2(x - 34.0f + slide, py + 14.0f), glm::vec2(x - 34.0f + slide, py + 44.0f), glm::vec2(x - 10.0f + slide, py + 29.0f), YELLOW);
		}
		else
			mUi.parallelogram(x, py, w, h, SKEW, alpha(BLACK, 0.8f));
		mUi.text(items[i], x + slide + 44.0f, py + 6.0f, 44.0f, WHITE, ALIGN_LEFT, FONT_BOLD, SLANT);
	}
}

void Game::drawMainMenu()
{
	float t = mMenuTime;
	style::menuBackdrop(mUi, 820.0f, t);

	mUi.text("イニシャル ドリフト", 118.0f, 56.0f, 36.0f, YELLOW, ALIGN_LEFT, FONT_JP, 0.0f);
	mUi.text("走り屋の夜", 118.0f, 98.0f, 22.0f, DIM, ALIGN_LEFT, FONT_JP, 0.0f);
	title(mUi, "INITIAL GL", 100.0f, 130.0f, 150.0f, WHITE, ALIGN_LEFT, RED, 0.035f);
	mUi.parallelogram(100.0f, 318.0f, 450.0f, 8.0f, 20.0f, RED);
	mUi.text("DRIFT KING", 108.0f, 322.0f, 60.0f, WHITE, ALIGN_LEFT, FONT_BOLD, SLANT);
	mUi.text("MOUNTAIN PASS  ·  NIGHT DESCENT", 112.0f, 392.0f, 22.0f, DIM, ALIGN_LEFT, FONT_MEDIUM, SLANT);

	drawMenuList({ "TIME ATTACK", "DRIFT LIBRE", std::string("VOITURE  ") + CarSpec::forId(CAR_IDS_UI[mCarIndex]).name, "RÉGLAGES", "QUITTER" }, 120.0f, 440.0f);

	char buf[96];
	float y = mUi.height() - 74.0f;
	mUi.parallelogram(100.0f, y, 640.0f, 46.0f, 28.0f, alpha(BLACK, 0.8f));
	tag(mUi, "BEST", 124.0f, y + 10.0f, 26.0f);
	snprintf(buf, sizeof(buf), "%s", mBestTime > 0.0f ? formatTime(mBestTime).c_str() : "--:--.--");
	mUi.text(buf, 220.0f, y + 6.0f, 32.0f, WHITE, ALIGN_LEFT, FONT_BOLD, SLANT);
	tag(mUi, "DRIFT", 400.0f, y + 10.0f, 26.0f, DARKRED);
	snprintf(buf, sizeof(buf), "%d", (int)mBestScore);
	mUi.text(buf, 520.0f, y + 6.0f, 32.0f, YELLOW, ALIGN_LEFT, FONT_BOLD, SLANT);

	mUi.text("HAUT / BAS  naviguer      GAUCHE / DROITE  voiture      ENTRÉE  valider", mUi.width() - 40.0f, mUi.height() - 40.0f, 20.0f, DIM, ALIGN_RIGHT, FONT_MEDIUM, SLANT);
}

void Game::drawStatBar(float x, float y, float w, const std::string& label, float value01, const std::string& text)
{
	mUi.text(label, x, y, 22.0f, DIM, ALIGN_LEFT, FONT_BOLD, SLANT);
	mUi.text(text, x + w, y - 2.0f, 26.0f, WHITE, ALIGN_RIGHT, FONT_BOLD, SLANT);
	gauge(mUi, x, y + 32.0f, w, 12.0f, value01, 16, value01 > 0.8f ? YELLOW : RED);
}

void Game::drawCarSelect()
{
	const CarSpec& s = CarSpec::forId(CAR_IDS_UI[mCarIndex]);
	float t = mMenuTime;

	speedStripes(mUi, t, 0.7f);

	mUi.parallelogramGradient(-100.0f, 28.0f, 640.0f, 66.0f, 34.0f, RED, alpha(RED, 0.0f));
	mUi.text("CHOIX DE LA VOITURE", 50.0f, 36.0f, 40.0f, WHITE, ALIGN_LEFT, FONT_BOLD, SLANT);
	mUi.text("車を選ぶ", 60.0f, 96.0f, 24.0f, YELLOW, ALIGN_LEFT, FONT_JP, 0.0f);

	float pulse = 8.0f * sinf(t * 5.0f);
	float my = mUi.height() * 0.42f;
	mUi.triangle(glm::vec2(70.0f - pulse, my), glm::vec2(110.0f - pulse, my - 38.0f), glm::vec2(110.0f - pulse, my + 38.0f), alpha(WHITE, 0.85f));
	mUi.triangle(glm::vec2(mUi.width() - 70.0f + pulse, my), glm::vec2(mUi.width() - 110.0f + pulse, my - 38.0f), glm::vec2(mUi.width() - 110.0f + pulse, my + 38.0f), alpha(WHITE, 0.85f));

	float h = 300.0f, y0 = mUi.height() - h;
	mUi.parallelogram(-150.0f, y0, mUi.width() + 300.0f, h, 90.0f, alpha(BLACK, 0.9f));
	mUi.parallelogram(-150.0f, y0 - 7.0f, mUi.width() + 300.0f, 7.0f, 90.0f, RED);
	mUi.parallelogram(-150.0f, y0 - 14.0f, mUi.width() + 300.0f, 3.0f, 90.0f, YELLOW);

	char buf[64];
	snprintf(buf, sizeof(buf), "No.%02d", mCarIndex + 1);
	mUi.text(buf, 70.0f, y0 + 18.0f, 30.0f, YELLOW, ALIGN_LEFT, FONT_BOLD, SLANT);
	title(mUi, s.name, 66.0f, y0 + 48.0f, 76.0f, WHITE, ALIGN_LEFT, RED, 0.04f);
	mUi.text(CAR_JP[mCarIndex], 72.0f, y0 + 152.0f, 28.0f, DIM, ALIGN_LEFT, FONT_JP, 0.0f);
	mUi.text("PROPULSION  ·  BOÎTE AUTOMATIQUE", 72.0f, y0 + 200.0f, 22.0f, DIM, ALIGN_LEFT, FONT_MEDIUM, SLANT);
	snprintf(buf, sizeof(buf), "%d / 3", mCarIndex + 1);
	mUi.text(buf, 72.0f, y0 + 238.0f, 24.0f, WHITE, ALIGN_LEFT, FONT_BOLD, SLANT);

	// puissance approximative en ch : couple * régime moyen / 7127
	float hp = s.peakTorque * (0.75f * s.rpmLimit) / 7127.0f;
	float cx = 640.0f, w = 300.0f;
	snprintf(buf, sizeof(buf), "%d ch", (int)hp);
	drawStatBar(cx, y0 + 36.0f, w, "PUISSANCE", hp / 380.0f, buf);
	snprintf(buf, sizeof(buf), "%d kg", (int)s.mass);
	drawStatBar(cx, y0 + 116.0f, w, "LÉGÈRETÉ", 1.0f - (s.mass - 950.0f) / 700.0f, buf);
	snprintf(buf, sizeof(buf), "%.2f", s.tireGrip);
	drawStatBar(cx, y0 + 196.0f, w, "ADHÉRENCE", (s.tireGrip - 0.95f) / 0.35f, buf);
	float driftiness = (1.0f - s.rearGripFactor) / 0.12f;
	drawStatBar(cx + w + 80.0f, y0 + 36.0f, w, "DRIFT", driftiness, driftiness > 0.75f ? "FACILE" : driftiness > 0.45f ? "MOYEN" : "EXIGEANT");
	snprintf(buf, sizeof(buf), "%d", (int)s.rpmLimit);
	drawStatBar(cx + w + 80.0f, y0 + 116.0f, w, "RÉGIME MAX", s.rpmLimit / 8500.0f, buf);
	snprintf(buf, sizeof(buf), "%d N·m", (int)s.peakTorque);
	drawStatBar(cx + w + 80.0f, y0 + 196.0f, w, "COUPLE", s.peakTorque / 450.0f, buf);

	mUi.text("GAUCHE / DROITE  changer      ENTRÉE  choisir", mUi.width() - 40.0f, mUi.height() - 38.0f, 20.0f, DIM, ALIGN_RIGHT, FONT_MEDIUM, SLANT);
}

void Game::drawSettings()
{
	if (mSettingsReturn == STATE_PAUSED)
	{
		mUi.flush();
		mUi.rect(0.0f, 0.0f, mUi.width(), mUi.height(), glm::vec4(0.0f, 0.0f, 0.03f, 0.55f));
	}
	style::menuBackdrop(mUi, 1000.0f, mMenuTime);
	mUi.text("設定", 120.0f, 52.0f, 34.0f, YELLOW, ALIGN_LEFT, FONT_JP, 0.0f);
	title(mUi, "RÉGLAGES", 100.0f, 80.0f, 100.0f, WHITE, ALIGN_LEFT, RED, 0.04f);

	const CarTuning& t = mScene.getCar().tuning;
	char buf[48];
	std::vector<std::string> labels = { "QUALITÉ GRAPHIQUE", "AIDE AU CONTRE-BRAQUAGE", "ADHÉRENCE DES PNEUS", "PUISSANCE MOTEUR", "AIDE À LA MOTRICITÉ", "LIMITEUR D'ANGLE DE DRIFT", "VOLUME MUSIQUE", "VOLUME EFFETS", "RETOUR" };
	std::vector<std::string> values(9);
	std::vector<float> bars(9, -1.0f);
	values[0] = mRenderer.getQualityPreset().name;
	snprintf(buf, sizeof(buf), "%d %%", (int)lroundf(t.steerAssist * 100)); values[1] = buf; bars[1] = t.steerAssist;
	snprintf(buf, sizeof(buf), "%d %%", (int)lroundf(t.gripScale * 100));   values[2] = buf; bars[2] = (t.gripScale - 0.6f) / 0.9f;
	snprintf(buf, sizeof(buf), "%d %%", (int)lroundf(t.powerScale * 100));  values[3] = buf; bars[3] = (t.powerScale - 0.5f) / 1.5f;
	snprintf(buf, sizeof(buf), "%d %%", (int)lroundf(t.tractionAid * 100)); values[4] = buf; bars[4] = t.tractionAid;
	snprintf(buf, sizeof(buf), "%d°", (int)lroundf(t.driftAngleMax * 57.2958f)); values[5] = buf; bars[5] = (t.driftAngleMax - 0.35f) / 0.85f;
	snprintf(buf, sizeof(buf), "%d %%", (int)lroundf(mAudio.musicVolume * 100)); values[6] = buf; bars[6] = mAudio.musicVolume;
	snprintf(buf, sizeof(buf), "%d %%", (int)lroundf(mAudio.sfxVolume * 100));   values[7] = buf; bars[7] = mAudio.sfxVolume;

	for (int i = 0; i < 9; i++)
	{
		bool sel = (i == mMenuSel);
		float y = 232.0f + i * 60.0f + (i == 8 ? 12.0f : 0.0f), x = 110.0f, w = 800.0f, h = 50.0f;
		float slide = sel ? 20.0f : 0.0f;
		if (sel)
		{
			mUi.parallelogram(x + slide - 5.0f, y - 4.0f, w + 10.0f, h + 8.0f, SKEW, YELLOW);
			mUi.parallelogramGradient(x + slide, y, w, h, SKEW, RED, DARKRED);
		}
		else
			mUi.parallelogram(x, y, w, h, SKEW, alpha(BLACK, 0.8f));
		mUi.text(labels[i], x + slide + 36.0f, y + 7.0f, 30.0f, WHITE, ALIGN_LEFT, FONT_BOLD, SLANT);
		if (i < 8)
		{
			if (bars[i] >= 0.0f) gauge(mUi, x + slide + 440.0f, y + 19.0f, 170.0f, 12.0f, bars[i], 10, sel ? WHITE : RED);
			mUi.text(values[i], x + slide + w - 30.0f, y + 5.0f, 32.0f, sel ? YELLOW : WHITE, ALIGN_RIGHT, FONT_BOLD, SLANT);
		}
	}
	mUi.text("HAUT / BAS  ligne      GAUCHE / DROITE  modifier      ÉCHAP  retour", 100.0f, mUi.height() - 40.0f, 20.0f, DIM, ALIGN_LEFT, FONT_MEDIUM, SLANT);

	const char* help[9] = {
		"Faible : 75 % de résolution, sans halo.  Élevé : anti-crénelage 4x, ombres haute résolution.",
		"Braque les roues avant pour vous aider à rattraper une glissade.",
		"Multiplie l'adhérence des pneus : plus bas, la voiture glisse plus facilement.",
		"Multiplie le couple du moteur.",
		"Limite le couple quand les roues arrière patinent. 0 % = patinage libre.",
		"Angle maximal de la glissade au frein à main : plus bas, le drift est plus facile à tenir.",
		"Volume de la musique.", "Volume du moteur, des pneus et des chocs.", "" };
	mUi.text(help[mMenuSel], 120.0f, 812.0f, 22.0f, WHITE, ALIGN_LEFT, FONT_MEDIUM, SLANT);
}

void Game::drawPause()
{
	mUi.flush();
	mUi.rect(0.0f, 0.0f, mUi.width(), mUi.height(), glm::vec4(0.0f, 0.0f, 0.03f, 0.6f));
	style::menuBackdrop(mUi, 820.0f, mMenuTime);
	mUi.text("一時停止", 120.0f, 100.0f, 30.0f, YELLOW, ALIGN_LEFT, FONT_JP, 0.0f);
	title(mUi, "PAUSE", 100.0f, 124.0f, 190.0f, WHITE, ALIGN_LEFT, RED, 0.035f);
	drawMenuList({ "REPRENDRE", "RECOMMENCER", "RÉGLAGES", "TERMINER LA SESSION", "MENU PRINCIPAL" }, 120.0f, 400.0f);
}

void Game::drawResults()
{
	style::menuBackdrop(mUi, 1020.0f, mMenuTime);
	bool finished = (mSession.mode == MODE_TIME_ATTACK && mSession.finished);
	mUi.text(finished ? "ゴール" : "結果", 120.0f, 50.0f, 34.0f, YELLOW, ALIGN_LEFT, FONT_JP, 0.0f);
	title(mUi, finished ? "ARRIVÉE" : "RÉSULTAT", 100.0f, 80.0f, 110.0f, WHITE, ALIGN_LEFT, RED, 0.04f);
	mUi.text(CarSpec::forId(CAR_IDS_UI[mCarIndex]).name, 108.0f, 200.0f, 28.0f, DIM, ALIGN_LEFT, FONT_BOLD, SLANT);

	char buf[96];
	struct Row { const char* label; std::string value; const char* note; glm::vec4 color; };
	std::string recordNote = (mBestTime > 0.0f && mSession.mode == MODE_TIME_ATTACK) ? "RECORD " + formatTime(mBestTime) : "";
	snprintf(buf, sizeof(buf), "RECORD %d", (int)mBestScore);
	Row rows[3];
	int nbRows;
	if (mSession.mode == MODE_TIME_ATTACK)
	{
		rows[0] = { "TEMPS", formatTime(mSession.time), recordNote.c_str(), WHITE };
		nbRows = 1;
	}
	else
	{
		rows[0] = { "SCORE DE DRIFT", std::to_string((int)mSession.totalScore), buf, YELLOW };
		rows[1] = { "MEILLEURE CHAÎNE", std::to_string((int)mSession.bestChain), "", WHITE };
		nbRows = 2;
	}
	for (int i = 0; i < nbRows; i++)
	{
		float y = 270.0f + i * 118.0f;
		mUi.parallelogramGradient(100.0f, y, 780.0f, 100.0f, 44.0f, alpha(BLACK, 0.92f), alpha(BLACK, 0.55f));
		mUi.parallelogram(92.0f, y, 8.0f, 100.0f, 44.0f, RED);
		tag(mUi, rows[i].label, 134.0f, y + 12.0f, 26.0f);
		mUi.text(rows[i].note, 134.0f, y + 66.0f, 20.0f, DIM, ALIGN_LEFT, FONT_MEDIUM, SLANT);
		mUi.text(rows[i].value, 840.0f, y + 6.0f, 82.0f, rows[i].color, ALIGN_RIGHT, FONT_BOLD, SLANT);
	}
	if (mNewRecord)
	{
		float p = 0.6f + 0.4f * sinf(mMenuTime * 9.0f);
		mUi.parallelogram(100.0f, 636.0f, 520.0f, 54.0f, 34.0f, alpha(YELLOW, p));
		mUi.text("NOUVEAU RECORD !", 360.0f, 642.0f, 40.0f, glm::vec4(0.1f, 0.0f, 0.0f, 1.0f), ALIGN_CENTER, FONT_BOLD, SLANT);
	}
	mMenuSel = glm::clamp(mMenuSel, 0, 1);
	drawMenuList({ "REJOUER", "MENU PRINCIPAL" }, 120.0f, 712.0f);
}

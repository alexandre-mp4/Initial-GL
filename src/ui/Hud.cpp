#include "Hud.h"
#include <cmath>
#include <cstdio>
#include "UiStyle.h"

using namespace style;

std::string formatTime(float t)
{
	int m = (int)(t / 60.0f);
	float s = t - 60.0f * m;
	char buf[32];
	snprintf(buf, sizeof(buf), "%d:%05.2f", m, s);
	return buf;
}

void Hud::setCourse(const Road& road)
{
	// un point tous les 25 m suffit pour la mini-carte
	std::vector<glm::vec2> pts;
	glm::vec2 mn(1e9f), mx(-1e9f);
	for (int i = 0; i < road.getNumPoints(); i += 25)
	{
		glm::vec2 p(road.getCenter(i).x, road.getCenter(i).z);
		pts.push_back(p);
		mn = glm::min(mn, p); mx = glm::max(mx, p);
	}
	mMapMin = mn;
	mMapSize = mx - mn;
	mMapScale = 1.0f / std::max(mMapSize.x, mMapSize.y);
	mMap = pts;
}

void Hud::draw(Ui& ui, const Car& car, const Session& s, float bestTime, float since, float time)
{
	drawMap(ui, car, s);
	drawTachometer(ui, car);

	if (s.mode == MODE_TIME_ATTACK) drawTimer(ui, s, bestTime);
	else drawDrift(ui, car, s, time);

	if (s.countdown > 0.0f)
	{
		int n = (int)ceilf(s.countdown);
		float frac = s.countdown - floorf(s.countdown);
		float size = 260.0f * (0.8f + 0.35f * frac);
		title(ui, std::to_string(n), ui.width() * 0.5f, ui.height() * 0.22f, size, alpha(WHITE, 0.3f + 0.7f * frac), ALIGN_CENTER, alpha(RED, 0.3f + 0.7f * frac));
	}
	else if (s.racing && s.time < 1.4f)
	{
		float a = 1.0f - s.time / 1.4f;
		float k = 1.0f + 0.5f * (1.0f - a);
		title(ui, "GO!", ui.width() * 0.5f, ui.height() * 0.2f, 280.0f * k, alpha(YELLOW, a), ALIGN_CENTER, alpha(RED, a));
	}

	if (since < 10.0f)
	{
		float a = glm::clamp(10.0f - since, 0.0f, 1.0f);
		float w = 1180.0f, x = ui.width() * 0.5f - w * 0.5f, y = ui.height() - 58.0f;
		ui.parallelogram(x, y, w, 40.0f, 28.0f, alpha(BLACK, 0.8f * a));
		ui.text("Flèches : conduire     ESPACE : frein à main (drift)     C : caméra (dont vue conducteur)     R : route     ÉCHAP : pause",
				ui.width() * 0.5f, y + 6.0f, 23.0f, alpha(WHITE, a), ALIGN_CENTER, FONT_MEDIUM);
	}
}

// compteur : cadran de 270° (0 = droite, sens horaire), graduations tous les 1000 tr/min, zone rouge, aiguille
void Hud::drawTachometer(Ui& ui, const Car& car)
{
	const float R = 150.0f, cx = ui.width() - 60.0f - R, cy = ui.height() - 46.0f - R;
	const float A0 = 2.35619f, SWEEP = 4.71239f;
	float limit = car.getRpmLimit();
	float frac = glm::clamp(car.getRpm() / limit, 0.0f, 1.0f);

	ui.arc(cx, cy, 0.0f, R + 16.0f, 0.0f, 6.28319f, alpha(BLACK, 0.78f), 64);
	ui.arc(cx, cy, R + 4.0f, R + 16.0f, A0, A0 + SWEEP, glm::vec4(1, 1, 1, 0.12f), 64);

	float redStart = (limit - 1000.0f) / limit;
	ui.arc(cx, cy, R - 8.0f, R + 4.0f, A0 + SWEEP * redStart, A0 + SWEEP, alpha(RED, 0.9f), 16);
	ui.arc(cx, cy, R - 30.0f, R - 14.0f, A0, A0 + SWEEP * frac, frac > redStart ? RED : (frac > 0.6f ? YELLOW : WHITE), 64);

	int maxK = (int)(limit / 1000.0f);
	for (int k = 0; k <= maxK; k++)
	{
		float a = A0 + SWEEP * (k * 1000.0f / limit);
		bool red = (k * 1000.0f >= limit - 1000.0f);
		glm::vec2 d(cosf(a), sinf(a));
		ui.line(glm::vec2(cx, cy) + d * (R - 10.0f), glm::vec2(cx, cy) + d * (R + 2.0f), 4.0f, red ? RED : WHITE);
		ui.text(std::to_string(k), cx + d.x * (R - 52.0f), cy + d.y * (R - 52.0f) - 13.0f, 26.0f, red ? RED : alpha(WHITE, 0.85f), ALIGN_CENTER, FONT_BOLD, 0.15f);
	}

	float na = A0 + SWEEP * frac;
	glm::vec2 nd(cosf(na), sinf(na));
	ui.line(glm::vec2(cx, cy) - nd * 14.0f, glm::vec2(cx, cy) + nd * (R - 6.0f), 5.0f, RED);
	ui.arc(cx, cy, 0.0f, 13.0f, 0.0f, 6.28319f, WHITE, 20);

	int g = car.getGear();
	ui.text(g < 0 ? "R" : std::to_string(g), cx, cy + 6.0f, 62.0f, YELLOW, ALIGN_CENTER, FONT_BOLD, SLANT);
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", (int)car.getSpeedKmh());
	ui.text(buf, cx + 36.0f, cy + R - 86.0f, 78.0f, WHITE, ALIGN_RIGHT, FONT_BOLD, SLANT);
	ui.text("km/h", cx + 44.0f, cy + R - 56.0f, 24.0f, DIM, ALIGN_LEFT, FONT_BOLD, SLANT);
}

void Hud::drawTimer(Ui& ui, const Session& s, float bestTime)
{
	float x = 40.0f, y = 34.0f;
	ui.parallelogramGradient(x, y, 430.0f, 128.0f, 40.0f, alpha(BLACK, 0.9f), alpha(BLACK, 0.45f));
	ui.parallelogram(x - 10.0f, y, 8.0f, 128.0f, 40.0f, RED);
	tag(ui, s.mode == MODE_TIME_ATTACK ? "TIME ATTACK" : "FREE DRIFT", x + 24.0f, y + 10.0f, 24.0f);
	ui.text(formatTime(s.time), x + 26.0f, y + 36.0f, 74.0f, WHITE, ALIGN_LEFT, FONT_BOLD, SLANT);

	if (s.mode == MODE_TIME_ATTACK)
	{
		ui.text(bestTime > 0.0f ? "BEST " + formatTime(bestTime) : std::string("BEST --:--.--"), x + 30.0f, y + 106.0f, 20.0f, DIM, ALIGN_LEFT, FONT_MEDIUM, SLANT);

		float bx = x + 230.0f, bw = 170.0f;
		ui.parallelogram(bx, y + 108.0f, bw, 10.0f, 10.0f, glm::vec4(1, 1, 1, 0.18f));
		ui.parallelogram(bx, y + 108.0f, bw * s.progress, 10.0f, 10.0f, RED);
		ui.parallelogram(bx + bw * s.progress - 4.0f, y + 102.0f, 8.0f, 22.0f, 10.0f, YELLOW);
	}
}

void Hud::drawDrift(Ui& ui, const Car& car, const Session& s, float time)
{
	float cx = ui.width() * 0.5f;
	char buf[64];

	ui.parallelogram(cx - 130.0f, 22.0f, 260.0f, 64.0f, 30.0f, alpha(BLACK, 0.85f));
	tag(ui, "SCORE", cx - 100.0f, 28.0f, 20.0f);
	snprintf(buf, sizeof(buf), "%d", (int)s.totalScore);
	ui.text(buf, cx + 98.0f, 36.0f, 46.0f, WHITE, ALIGN_RIGHT, FONT_BOLD, SLANT);

	if (s.chainScore > 0.0f)
	{
		float pulse = s.drifting ? 1.0f : 0.55f;

		float k = 1.0f + (s.drifting ? 0.04f * sinf(time * 22.0f) : 0.0f);
		snprintf(buf, sizeof(buf), "+%d", (int)s.chainScore);
		title(ui, buf, cx, 104.0f, 84.0f * k, alpha(YELLOW, pulse), ALIGN_CENTER, alpha(RED, pulse));
		ui.parallelogram(cx - 150.0f, 196.0f, 300.0f, 40.0f, 28.0f, alpha(s.multiplier >= 4 ? RED : DARKRED, 0.95f * pulse));
		snprintf(buf, sizeof(buf), "DRIFT  x%d   %d°", s.multiplier, (int)(fabsf(car.getDriftAngle()) * 57.2958f));
		ui.text(buf, cx, 201.0f, 30.0f, alpha(WHITE, pulse), ALIGN_CENTER, FONT_BOLD, SLANT);
		float bw = 280.0f, frac = (s.multiplier >= 5) ? 1.0f : fmodf(s.chainTime, 2.0f) / 2.0f;
		ui.parallelogram(cx - bw / 2, 242.0f, bw, 7.0f, 8.0f, glm::vec4(1, 1, 1, 0.18f));
		ui.parallelogram(cx - bw / 2, 242.0f, bw * frac, 7.0f, 8.0f, YELLOW);
	}
	else if (s.bankFlash > 0.0f)
	{
		float a = std::min(1.0f, s.bankFlash);
		snprintf(buf, sizeof(buf), "+%d", (int)s.lastBankedChain);
		title(ui, buf, cx, 104.0f, 72.0f, alpha(glm::vec4(0.5f, 1.0f, 0.55f, 1), a), ALIGN_CENTER, alpha(glm::vec4(0.0f, 0.3f, 0.1f, 1), a));
	}
	if (s.crashFlash > 0.0f)
	{
		float a = std::min(1.0f, s.crashFlash);
		ui.parallelogram(cx - 260.0f, 112.0f, 520.0f, 58.0f, 34.0f, alpha(RED, 0.92f * a));
		ui.text("CRASH!   COMBO PERDU", cx, 120.0f, 40.0f, alpha(WHITE, a), ALIGN_CENTER, FONT_BOLD, SLANT);
	}
}

void Hud::drawMap(Ui& ui, const Car& car, const Session& s)
{
	if (mMap.size() < 2) return;
	float size = 250.0f, x0 = 54.0f, y0 = ui.height() - size - 54.0f;
	ui.parallelogram(x0 - 20.0f, y0 - 20.0f, size + 40.0f, size + 40.0f, 24.0f, alpha(BLACK, 0.72f));
	ui.parallelogram(x0 - 28.0f, y0 - 20.0f, 7.0f, size + 40.0f, 24.0f, RED);

	// sur la carte, z vers le bas
	auto toMap = [&](const glm::vec2& w) {
		glm::vec2 q = (w - mMapMin) * mMapScale;
		float aspect = mMapSize.x / std::max(mMapSize.x, mMapSize.y), aspectZ = mMapSize.y / std::max(mMapSize.x, mMapSize.y);
		return glm::vec2(x0 + (size - size * aspect) * 0.5f + q.x * size, y0 + (size - size * aspectZ) * 0.5f + q.y * size);
	};
	int doneUpTo = (int)(s.progress * (float)(mMap.size() - 1));
	for (size_t i = 0; i + 1 < mMap.size(); i++)
	{
		bool done = (int)i < doneUpTo;
		ui.line(toMap(mMap[i]), toMap(mMap[i + 1]), done ? 4.5f : 3.0f, done ? RED : glm::vec4(1, 1, 1, 0.55f));
	}

	glm::vec2 st = toMap(mMap.front()), fi = toMap(mMap[mMap.size() - 1 - 3]);
	ui.arc(st.x, st.y, 0.0f, 6.0f, 0.0f, 6.28319f, WHITE, 12);
	ui.rect(fi.x - 6.0f, fi.y - 6.0f, 12.0f, 12.0f, WHITE);
	ui.rect(fi.x - 6.0f, fi.y - 6.0f, 6.0f, 6.0f, glm::vec4(0, 0, 0, 1)); ui.rect(fi.x, fi.y, 6.0f, 6.0f, glm::vec4(0, 0, 0, 1));

	glm::vec2 me = toMap(glm::vec2(car.getPosition().x, car.getPosition().z));
	ui.arc(me.x, me.y, 0.0f, 10.0f, 0.0f, 6.28319f, glm::vec4(0, 0, 0, 0.8f), 16);
	ui.arc(me.x, me.y, 0.0f, 7.0f, 0.0f, 6.28319f, YELLOW, 16);
}

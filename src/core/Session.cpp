#include "Session.h"
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include "glm/glm.hpp"

static const char* SCORES_FILE = "scores.txt";

void Session::start(GameMode m, int finishIdx, int n)
{
	*this = Session();
	mode = m;
	finishIndex = finishIdx;
	numPoints = n;
}

void Session::update(float dt, const Car& car, int roadIndex)
{
	if (finished)
		return;

	if (countdown > 0.0f)
	{
		countdown -= dt;
		if (countdown <= 0.0f) { countdown = 0.0f; racing = true; }
		return;
	}

	time += dt;
	progress = glm::clamp((float)roadIndex / (float)std::max(finishIndex, 1), 0.0f, 1.0f);
	distance = (float)roadIndex;

	bankFlash = std::max(0.0f, bankFlash - dt);
	crashFlash = std::max(0.0f, crashFlash - dt);

	int wheelsOnGround = 0;
	for (int i = 0; i < 4; i++) if (car.getWheel(i).contact) wheelsOnGround++;
	float angleDeg = fabsf(car.getDriftAngle()) * 57.2958f;
	float speedKmh = car.getSpeedKmh();
	bool inDrift = (angleDeg > 12.0f && speedKmh > 25.0f && wheelsOnGround >= 3 && car.getSpeed() > 0.0f);

	// un choc fait perdre la chaîne en cours (une seule fois par choc)
	bool impact = car.getLastImpact() > 4.0f;
	if (impact && !mWasImpact && chainScore > 0.0f)
	{
		lostChain = chainScore;
		chainScore = 0.0f; multiplier = 1; chainTime = 0.0f; drifting = false;
		crashFlash = 1.6f;
	}
	mWasImpact = impact;

	if (inDrift)
	{
		drifting = true;
		mNoDriftTime = 0.0f;
		chainTime += dt;
		multiplier = std::min(5, 1 + (int)(chainTime / 2.0f));
		chainScore += angleDeg * speedKmh * 0.02f * (float)multiplier * dt;
	}
	else if (chainScore > 0.0f)
	{
		mNoDriftTime += dt;
		drifting = false;
		if (mNoDriftTime > 0.8f)
		{
			totalScore += chainScore;
			lastBankedChain = chainScore;
			bestChain = std::max(bestChain, chainScore);
			bankFlash = 1.8f;
			chainScore = 0.0f; multiplier = 1; chainTime = 0.0f;
		}
	}

	if (mode == MODE_TIME_ATTACK && roadIndex >= finishIndex)
	{
		if (chainScore > 0.0f) { totalScore += chainScore; bestChain = std::max(bestChain, chainScore); chainScore = 0.0f; }
		finished = true;
		racing = false;
	}
}

// records : lignes « clé valeur »
static std::map<std::string, float> readScores()
{
	std::map<std::string, float> m;
	std::ifstream in(SCORES_FILE);
	std::string kind, id;
	float v;
	while (in >> kind >> id >> v)
		m[kind + " " + id] = v;
	return m;
}

static void writeScores(const std::map<std::string, float>& m)
{
	std::ofstream out(SCORES_FILE);
	for (const auto& kv : m)
		out << kv.first << " " << kv.second << "\n";
}

float Session::loadBestTime(const std::string& id)  { auto m = readScores(); auto it = m.find("time " + id);  return it == m.end() ? 0.0f : it->second; }
float Session::loadBestScore(const std::string& id) { auto m = readScores(); auto it = m.find("score " + id); return it == m.end() ? 0.0f : it->second; }

void Session::saveBestTime(const std::string& id, float t)
{
	auto m = readScores();
	auto it = m.find("time " + id);
	if (it == m.end() || t < it->second) { m["time " + id] = t; writeScores(m); }
}

void Session::saveBestScore(const std::string& id, float s)
{
	auto m = readScores();
	auto it = m.find("score " + id);
	if (it == m.end() || s > it->second) { m["score " + id] = s; writeScores(m); }
}

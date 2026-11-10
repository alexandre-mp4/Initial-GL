// affichage pendant la course : compteur, chrono, score de drift et mini-carte
#ifndef HUD_H
#define HUD_H

#include <vector>
#include "Ui.h"
#include "Car.h"
#include "Road.h"
#include "Session.h"

class Hud
{
public:

	void setCourse(const Road& road);

	void draw(Ui& ui, const Car& car, const Session& session, float bestTime, float since, float time);

private:

	void drawTachometer(Ui& ui, const Car& car);
	void drawTimer(Ui& ui, const Session& s, float bestTime);
	void drawDrift(Ui& ui, const Car& car, const Session& s, float time);
	void drawMap(Ui& ui, const Car& car, const Session& s);

	std::vector<glm::vec2> mMap;
	glm::vec2 mMapMin, mMapSize;
	float mMapScale = 1.0f;
};

std::string formatTime(float seconds);
#endif

#include <iostream>
#include "Game.h"

int main()
{
	Game game;
	if (!game.init())
	{
		std::cerr << "Initialisation du jeu impossible" << std::endl;
		return -1;
	}

	game.run();
	return 0;
}

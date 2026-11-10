// palette et éléments graphiques communs à l'interface (style touge : noir, rouge, jaune, formes en parallélogrammes)
#ifndef UISTYLE_H
#define UISTYLE_H

#include <cmath>
#include "Ui.h"

namespace style
{
	const glm::vec4 WHITE(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::vec4 DIM(1.0f, 1.0f, 1.0f, 0.62f);
	const glm::vec4 RED(0.89f, 0.07f, 0.12f, 1.0f);
	const glm::vec4 DARKRED(0.45f, 0.02f, 0.06f, 1.0f);
	const glm::vec4 YELLOW(1.0f, 0.82f, 0.05f, 1.0f);
	const glm::vec4 BLACK(0.03f, 0.03f, 0.04f, 0.86f);
	const glm::vec4 GLASS(0.03f, 0.03f, 0.05f, 0.62f);

	const float SLANT = 0.22f;
	const float SKEW = 26.0f;

	inline glm::vec4 alpha(glm::vec4 c, float a) { c.a *= a; return c; }

	inline void title(Ui& ui, const std::string& s, float x, float y, float size, const glm::vec4& color, UiAlign align = ALIGN_LEFT,
					  const glm::vec4& shadow = RED, float off = 0.045f)
	{
		ui.text(s, x + size * off, y + size * off, size, shadow, align, FONT_BOLD, SLANT);
		ui.text(s, x, y, size, color, align, FONT_BOLD, SLANT);
	}

	inline void tag(Ui& ui, const std::string& s, float x, float y, float h = 26.0f, const glm::vec4& color = RED)
	{
		float w = ui.textWidth(s, h * 0.78f) + h * 1.2f;
		ui.parallelogram(x, y, w, h, h * 0.55f, color);
		ui.text(s, x + h * 0.6f, y + h * 0.05f, h * 0.78f, WHITE, ALIGN_LEFT, FONT_BOLD, SLANT);
	}

	inline void speedStripes(Ui& ui, float t, float alphaMul = 1.0f)
	{
		for (int i = 0; i < 9; i++)
		{
			float speed = 380.0f + 90.0f * (i % 4);
			float x = fmodf(i * 263.0f - t * speed, ui.width() + 700.0f) - 350.0f;
			float w = 3.0f + (i % 3) * 5.0f;
			float h = ui.height() * 1.4f;
			ui.parallelogram(x, -ui.height() * 0.2f, w, h, -h * 0.55f, glm::vec4(1.0f, 1.0f, 1.0f, (0.06f + 0.04f * (i % 3)) * alphaMul));
		}
	}

	inline void menuBackdrop(Ui& ui, float width, float t)
	{
		ui.parallelogramGradient(-200.0f, 0.0f, width + 200.0f, ui.height(), 180.0f, glm::vec4(0, 0, 0, 0.93f), glm::vec4(0, 0, 0, 0.55f));

		ui.parallelogram(width - 14.0f, 0.0f, 14.0f, ui.height(), 180.0f, alpha(RED, 0.95f));
		ui.parallelogram(width + 26.0f, 0.0f, 5.0f, ui.height(), 180.0f, alpha(YELLOW, 0.9f));
		speedStripes(ui, t, 1.0f);
	}

	inline void gauge(Ui& ui, float x, float y, float w, float h, float value01, int segs = 14, const glm::vec4& on = RED)
	{
		float gap = 4.0f, sw = (w - gap * (segs - 1)) / segs;
		int lit = (int)floorf(glm::clamp(value01, 0.0f, 1.0f) * segs + 0.5f);
		for (int i = 0; i < segs; i++)
			ui.parallelogram(x + i * (sw + gap), y, sw, h, h * 0.6f, i < lit ? on : glm::vec4(1, 1, 1, 0.14f));
	}
}
#endif

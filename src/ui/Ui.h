// texte et formes 2D dans un repère virtuel de hauteur fixe (900), origine en haut à gauche
// les polices sont cuites dans des textures (stb_truetype) et tout est accumulé dans des tableaux de sommets, dessinés par flush()
#ifndef UI_H
#define UI_H

#define GLEW_STATIC
#include "GL/glew.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "glm/glm.hpp"
#include "ShaderProgram.h"

enum UiAlign { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };
enum UiFont { FONT_MEDIUM = 0, FONT_BOLD = 1, FONT_JP = 2 };

class Ui
{
public:

	Ui();
	~Ui();
	Ui(const Ui&) = delete;
	Ui& operator=(const Ui&) = delete;

	bool init(const std::string& mediumPath, const std::string& boldPath, const std::string& jpPath);

	void begin(int framebufferW, int framebufferH);
	float width() const  { return mVW; }
	float height() const { return mVH; }

	void rect(float x, float y, float w, float h, const glm::vec4& color);
	void rectOutline(float x, float y, float w, float h, float thickness, const glm::vec4& color);
	void gradientRect(float x, float y, float w, float h, const glm::vec4& top, const glm::vec4& bottom);

	// parallélogramme : bords haut et bas décalés de skew
	void parallelogram(float x, float y, float w, float h, float skew, const glm::vec4& color);
	void parallelogramGradient(float x, float y, float w, float h, float skew, const glm::vec4& left, const glm::vec4& right);
	void line(const glm::vec2& a, const glm::vec2& b, float thickness, const glm::vec4& color);
	void triangle(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec4& color);

	// arc épais, angles en radians (0 = droite, sens horaire à l'écran)
	void arc(float cx, float cy, float r0, float r1, float a0, float a1, const glm::vec4& color, int segments = 48);

	// slant : inclinaison du texte (italique par cisaillement)
	void text(const std::string& s, float x, float y, float size, const glm::vec4& color, UiAlign align = ALIGN_LEFT,
			  int font = FONT_BOLD, float slant = 0.0f);
	float textWidth(const std::string& s, float size, int font = FONT_BOLD) const;

	void flush();

private:

	struct Glyph { float x0, y0, x1, y1, xoff, yoff, xoff2, yoff2, xadvance; };
	struct Font
	{
		GLuint texture = 0;
		std::unordered_map<int, Glyph> glyphs;
		int atlasW = 1024, atlasH = 1024;
		float bakeSize = 64.0f;
	};

	bool bake(Font& font, const std::string& path, bool japanese);
	const Glyph& lookup(const Font& f, int cp) const;
	void pushTri(int batch, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec2& uvA, const glm::vec2& uvB, const glm::vec2& uvC,
				 const glm::vec4& ca, const glm::vec4& cb, const glm::vec4& cc);
	void pushQuadPts(int batch, const glm::vec2 p[4], const glm::vec2 uv[4], const glm::vec4 c[4]);
	glm::vec2 whiteUV() const;

	Font mFonts[3];
	std::vector<float> mVerts[3];
	ShaderProgram mShader;
	GLuint mVao, mVbo;
	float mVW, mVH;
	int mFbW, mFbH;
};
#endif

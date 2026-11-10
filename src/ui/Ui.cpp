#include "Ui.h"
#include <cstdio>
#include <iostream>
#include <fstream>
#include "glm/gtc/matrix_transform.hpp"
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

static const int WHITE_PIXELS = 4;

// décode un caractère utf-8
static int nextCodepoint(const std::string& s, size_t& i)
{
	unsigned char c = (unsigned char)s[i++];
	if (c < 0x80) return c;
	if ((c >> 5) == 0x6 && i < s.size()) return ((c & 0x1F) << 6) | (s[i++] & 0x3F);
	if ((c >> 4) == 0xE && i + 1 < s.size()) { int r = ((c & 0x0F) << 12) | ((s[i] & 0x3F) << 6) | (s[i + 1] & 0x3F); i += 2; return r; }
	return '?';
}

// seuls les kanji utilisés sont cuits dans l'atlas
static const char* KANJI = "峠走屋頭文字夜速最高度時間点記録山秋名下車回転挑戦始終了選択設定音楽量画質戻再開停止新道路勝負的連続攻守先行追抜限界結果到着一";

Ui::Ui() : mVao(0), mVbo(0), mVW(1600.0f), mVH(900.0f), mFbW(1), mFbH(1) {}

Ui::~Ui()
{
	for (Font& f : mFonts)
		if (f.texture) glDeleteTextures(1, &f.texture);
	if (mVbo) glDeleteBuffers(1, &mVbo);
	if (mVao) glDeleteVertexArrays(1, &mVao);
}

bool Ui::bake(Font& font, const std::string& path, bool japanese)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		std::cerr << "Ui : police introuvable '" << path << "'" << std::endl;
		return false;
	}
	std::vector<unsigned char> ttf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

	struct Range { int first, count; };
	std::vector<Range> ranges;
	if (!japanese) { ranges.push_back({ 32, 95 }); ranges.push_back({ 160, 96 }); }
	else
	{
		ranges.push_back({ 32, 95 }); ranges.push_back({ 0x3040, 96 }); ranges.push_back({ 0x30A0, 96 });
		for (size_t i = 0; i < std::string(KANJI).size();) ranges.push_back({ nextCodepoint(KANJI, i), 1 });
	}

	// l'atlas contient aussi un carré blanc pour les formes pleines
	const int packH = font.atlasH - 8;
	std::vector<unsigned char> bitmap((size_t)font.atlasW * font.atlasH, 0);
	stbtt_pack_context ctx;
	stbtt_PackBegin(&ctx, bitmap.data(), font.atlasW, packH, font.atlasW, 1, nullptr);
	stbtt_PackSetOversampling(&ctx, 2, 2);
	for (const Range& r : ranges)
	{
		std::vector<stbtt_packedchar> pc(r.count);
		stbtt_PackFontRange(&ctx, ttf.data(), 0, font.bakeSize, r.first, r.count, pc.data());
		for (int i = 0; i < r.count; i++)
		{
			const stbtt_packedchar& g = pc[i];
			font.glyphs[r.first + i] = { (float)g.x0 / font.atlasW, (float)g.y0 / font.atlasH, (float)g.x1 / font.atlasW, (float)g.y1 / font.atlasH,
										 g.xoff, g.yoff, g.xoff2, g.yoff2, g.xadvance };
		}
	}
	stbtt_PackEnd(&ctx);

	for (int y = font.atlasH - WHITE_PIXELS; y < font.atlasH; y++)
		for (int x = font.atlasW - WHITE_PIXELS; x < font.atlasW; x++)
			bitmap[(size_t)y * font.atlasW + x] = 255;

	glGenTextures(1, &font.texture);
	glBindTexture(GL_TEXTURE_2D, font.texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, font.atlasW, font.atlasH, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return true;
}

bool Ui::init(const std::string& mediumPath, const std::string& boldPath, const std::string& jpPath)
{
	mFonts[FONT_JP].bakeSize = 56.0f;
	mFonts[FONT_JP].atlasW = 2048; mFonts[FONT_JP].atlasH = 1024;
	if (!bake(mFonts[FONT_MEDIUM], mediumPath, false) || !bake(mFonts[FONT_BOLD], boldPath, false) || !bake(mFonts[FONT_JP], jpPath, true))
		return false;

	glGenVertexArrays(1, &mVao);
	glGenBuffers(1, &mVbo);
	glBindVertexArray(mVao);
	glBindBuffer(GL_ARRAY_BUFFER, mVbo);
	const GLsizei stride = 8 * sizeof(float);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glBindVertexArray(0);
	return mShader.loadShaders("shaders/ui.vert", "shaders/ui.frag");
}

void Ui::begin(int fbW, int fbH)
{
	mFbW = fbW; mFbH = fbH;
	mVH = 900.0f;
	mVW = mVH * (float)fbW / (float)std::max(fbH, 1);
	for (auto& v : mVerts) v.clear();
}

glm::vec2 Ui::whiteUV() const
{
	const Font& f = mFonts[FONT_BOLD];
	return glm::vec2(1.0f - 2.0f / f.atlasW, 1.0f - 2.0f / f.atlasH);
}

void Ui::pushTri(int b, const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& u0, const glm::vec2& u1, const glm::vec2& u2,
				 const glm::vec4& c0, const glm::vec4& c1, const glm::vec4& c2)
{
	std::vector<float>& v = mVerts[b];
	float t[3][8] = { { p0.x, p0.y, u0.x, u0.y, c0.r, c0.g, c0.b, c0.a }, { p1.x, p1.y, u1.x, u1.y, c1.r, c1.g, c1.b, c1.a }, { p2.x, p2.y, u2.x, u2.y, c2.r, c2.g, c2.b, c2.a } };
	for (auto& row : t) v.insert(v.end(), row, row + 8);
}

void Ui::pushQuadPts(int b, const glm::vec2 p[4], const glm::vec2 uv[4], const glm::vec4 c[4])
{
	pushTri(b, p[0], p[1], p[2], uv[0], uv[1], uv[2], c[0], c[1], c[2]);
	pushTri(b, p[0], p[2], p[3], uv[0], uv[2], uv[3], c[0], c[2], c[3]);
}

void Ui::rect(float x, float y, float w, float h, const glm::vec4& c)
{
	parallelogramGradient(x, y, w, h, 0.0f, c, c);
}

void Ui::gradientRect(float x, float y, float w, float h, const glm::vec4& top, const glm::vec4& bottom)
{
	glm::vec2 p[4] = { { x, y }, { x + w, y }, { x + w, y + h }, { x, y + h } }, u[4] = { whiteUV(), whiteUV(), whiteUV(), whiteUV() };
	glm::vec4 c[4] = { top, top, bottom, bottom };
	pushQuadPts(FONT_BOLD, p, u, c);
}

void Ui::parallelogram(float x, float y, float w, float h, float skew, const glm::vec4& color)
{
	parallelogramGradient(x, y, w, h, skew, color, color);
}

void Ui::parallelogramGradient(float x, float y, float w, float h, float skew, const glm::vec4& l, const glm::vec4& r)
{
	// le haut est décalé de +skew/2 et le bas de -skew/2
	glm::vec2 p[4] = { { x + skew * 0.5f, y }, { x + w + skew * 0.5f, y }, { x + w - skew * 0.5f, y + h }, { x - skew * 0.5f, y + h } };
	glm::vec2 u[4] = { whiteUV(), whiteUV(), whiteUV(), whiteUV() };
	glm::vec4 c[4] = { l, r, r, l };
	pushQuadPts(FONT_BOLD, p, u, c);
}

void Ui::rectOutline(float x, float y, float w, float h, float t, const glm::vec4& c)
{
	rect(x, y, w, t, c); rect(x, y + h - t, w, t, c);
	rect(x, y + t, t, h - 2 * t, c); rect(x + w - t, y + t, t, h - 2 * t, c);
}

void Ui::line(const glm::vec2& a, const glm::vec2& b, float t, const glm::vec4& color)
{
	glm::vec2 d = b - a;
	float len = glm::length(d);
	if (len < 1e-4f) return;
	glm::vec2 n = glm::vec2(-d.y, d.x) / len * (t * 0.5f);
	glm::vec2 p[4] = { a + n, b + n, b - n, a - n }, u[4] = { whiteUV(), whiteUV(), whiteUV(), whiteUV() };
	glm::vec4 c[4] = { color, color, color, color };
	pushQuadPts(FONT_BOLD, p, u, c);
}

void Ui::triangle(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec4& color)
{
	pushTri(FONT_BOLD, a, b, c, whiteUV(), whiteUV(), whiteUV(), color, color, color);
}

void Ui::arc(float cx, float cy, float r0, float r1, float a0, float a1, const glm::vec4& color, int segs)
{
	for (int i = 0; i < segs; i++)
	{
		float t0 = a0 + (a1 - a0) * i / segs, t1 = a0 + (a1 - a0) * (i + 1) / segs;
		glm::vec2 p[4] = { { cx + cosf(t0) * r0, cy + sinf(t0) * r0 }, { cx + cosf(t0) * r1, cy + sinf(t0) * r1 },
						   { cx + cosf(t1) * r1, cy + sinf(t1) * r1 }, { cx + cosf(t1) * r0, cy + sinf(t1) * r0 } };
		glm::vec2 u[4] = { whiteUV(), whiteUV(), whiteUV(), whiteUV() };
		glm::vec4 c[4] = { color, color, color, color };
		pushQuadPts(FONT_BOLD, p, u, c);
	}
}

const Ui::Glyph& Ui::lookup(const Font& f, int cp) const
{
	auto it = f.glyphs.find(cp);
	if (it != f.glyphs.end()) return it->second;
	if (cp == 0x2019) return f.glyphs.at('\'');
	return f.glyphs.at('?');
}

float Ui::textWidth(const std::string& s, float size, int font) const
{
	const Font& f = mFonts[font];
	float scale = size / f.bakeSize, w = 0.0f;
	for (size_t i = 0; i < s.size();)
		w += lookup(f, nextCodepoint(s, i)).xadvance * scale;
	return w;
}

void Ui::text(const std::string& s, float x, float y, float size, const glm::vec4& color, UiAlign align, int font, float slant)
{
	const Font& f = mFonts[font];
	float scale = size / f.bakeSize;
	float w = textWidth(s, size, font);
	if (align == ALIGN_CENTER) x -= 0.5f * w;
	else if (align == ALIGN_RIGHT) x -= w;

	// un lot de sommets par police
	float baseline = y + size * 0.82f;
	for (size_t i = 0; i < s.size();)
	{
		const Glyph& g = lookup(f, nextCodepoint(s, i));
		float x0 = x + g.xoff * scale, x1 = x + g.xoff2 * scale;
		float y0 = baseline + g.yoff * scale, y1 = baseline + g.yoff2 * scale;

		float sh0 = (baseline - y0) * slant, sh1 = (baseline - y1) * slant;
		glm::vec2 p[4] = { { x0 + sh0, y0 }, { x1 + sh0, y0 }, { x1 + sh1, y1 }, { x0 + sh1, y1 } };
		glm::vec2 u[4] = { { g.x0, g.y0 }, { g.x1, g.y0 }, { g.x1, g.y1 }, { g.x0, g.y1 } };
		glm::vec4 c[4] = { color, color, color, color };
		pushQuadPts(font, p, u, c);
		x += g.xadvance * scale;
	}
}

void Ui::flush()
{
	glViewport(0, 0, mFbW, mFbH);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	mShader.use();
	mShader.setUniform("ortho", glm::ortho(0.0f, mVW, mVH, 0.0f));
	mShader.setUniformSampler("atlas", 0);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(mVao);
	glBindBuffer(GL_ARRAY_BUFFER, mVbo);

	// les formes et le gras d'abord, le texte medium ensuite pour qu'il soit au-dessus
	for (int fi : { (int)FONT_BOLD, (int)FONT_JP, (int)FONT_MEDIUM })
	{
		if (mVerts[fi].empty()) continue;
		glBindTexture(GL_TEXTURE_2D, mFonts[fi].texture);
		glBufferData(GL_ARRAY_BUFFER, mVerts[fi].size() * sizeof(float), mVerts[fi].data(), GL_STREAM_DRAW);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(mVerts[fi].size() / 8));
	}
	glBindVertexArray(0);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	for (auto& v : mVerts) v.clear();
}

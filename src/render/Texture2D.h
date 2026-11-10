#ifndef TEXTURE2D_H
#define TEXTURE2D_H

#define GLEW_STATIC
#include "GL/glew.h"
#include <string>
using std::string;

class Texture2D
{
public:
	Texture2D();
	virtual ~Texture2D();

	// srgb : couleur encodée en sRGB, convertie en linéaire par le GPU ; faux pour les données (normales, roughness)
	// flipY : retourne l'image (origine en haut pour les fichiers, en bas pour OpenGL)
	bool loadTexture(const string& fileName, bool generateMipMaps = true, bool srgb = false, bool flipY = true);

	bool createSolid(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);

	bool loadFromMemory(const unsigned char* data, size_t size, bool generateMipMaps = true, bool srgb = false, bool flipY = true);
	void bind(GLuint texUnit = 0);
	void unbind(GLuint texUnit = 0);

	// copie interdite : le destructeur libère la texture OpenGL
	Texture2D(const Texture2D& rhs) = delete;
	Texture2D& operator = (const Texture2D& rhs) = delete;

private:

	bool upload(unsigned char* rgbaPixels, int width, int height, bool generateMipMaps, bool srgb, bool flipY);

	GLuint mTexture;
};
#endif

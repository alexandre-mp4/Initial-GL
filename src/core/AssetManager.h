// charge chaque fichier une seule fois, la clé est le chemin
#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <map>
#include <memory>
#include <string>
#include "Texture2D.h"
#include "Mesh.h"
#include "Model.h"

class AssetManager
{
public:

	// srgb : vrai pour les couleurs, faux pour les données (normales, ORM)
	// flipY : faux pour les cartes lues en coordonnées monde
	Texture2D* texture(const std::string& path, bool srgb = false, bool mipmaps = true, bool flipY = true);
	Mesh*      mesh(const std::string& objPath);
	Model*     model(const std::string& glbPath);

private:

	std::map<std::string, std::unique_ptr<Texture2D>> mTextures;
	std::map<std::string, std::unique_ptr<Mesh>>      mMeshes;
	std::map<std::string, std::unique_ptr<Model>>     mModels;
};
#endif

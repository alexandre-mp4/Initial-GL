#include "AssetManager.h"

Texture2D* AssetManager::texture(const std::string& path, bool srgb, bool mipmaps, bool flipY)
{
	std::string key = path + (srgb ? "#srgb" : "#lin") + (flipY ? "" : "#noflip");
	auto it = mTextures.find(key);
	if (it != mTextures.end())
		return it->second.get();

	std::unique_ptr<Texture2D> tex(new Texture2D());
	if (!tex->loadTexture(path, mipmaps, srgb, flipY))
		return nullptr;

	Texture2D* raw = tex.get();
	mTextures[key] = std::move(tex);
	return raw;
}

Mesh* AssetManager::mesh(const std::string& objPath)
{
	auto it = mMeshes.find(objPath);
	if (it != mMeshes.end())
		return it->second.get();

	std::unique_ptr<Mesh> m(new Mesh());
	if (!m->loadOBJ(objPath))
		return nullptr;

	Mesh* raw = m.get();
	mMeshes[objPath] = std::move(m);
	return raw;
}

Model* AssetManager::model(const std::string& glbPath)
{
	auto it = mModels.find(glbPath);
	if (it != mModels.end())
		return it->second.get();

	std::unique_ptr<Model> m(new Model());
	if (!m->load(glbPath))
		return nullptr;

	Model* raw = m.get();
	mModels[glbPath] = std::move(m);
	return raw;
}

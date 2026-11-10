// modèle glTF (.glb) : noeuds en arbre (matrice locale par rapport au parent), primitives (mesh + matériau), matériaux et textures
// pour animer un noeud (roue), draw() reçoit une matrice « extra » appliquée dans son repère local
#ifndef MODEL_H
#define MODEL_H

#include <memory>
#include <string>
#include <vector>
#include "glm/glm.hpp"
#include "Mesh.h"
#include "Material.h"
#include "Texture2D.h"

class Renderer;

class Model
{
public:

	bool load(const std::string& path);

	int findNode(const std::string& name) const;

	int getNodeCount() const { return (int)mNodes.size(); }
	const std::string& getNodeName(int node) const { return mNodes[node].name; }

	glm::vec3 getNodePosition(int node) const;

	// extra : une matrice optionnelle par noeud, appliquée après sa matrice locale
	void draw(Renderer& renderer, const glm::mat4& root, const std::vector<glm::mat4>* extra = nullptr);

	void drawNodeInstanced(Renderer& renderer, int node, GLuint instanceVbo, size_t byteOffset, int count, float treeHeight);

	unsigned int getTriangleCount() const { return mTriangles; }

	const glm::vec3& getBoundsMin() const { return mBoundsMin; }
	const glm::vec3& getBoundsMax() const { return mBoundsMax; }

private:

	struct Node
	{
		std::string name;
		int parent;
		glm::mat4 local;
		std::vector<int> primitives;
	};

	struct Primitive
	{
		std::unique_ptr<Mesh> mesh;
		int material;
	};

	std::vector<Node> mNodes;
	std::vector<Primitive> mPrimitives;
	std::vector<Material> mMaterials;
	std::vector<std::unique_ptr<Texture2D>> mTextures;
	glm::vec3 mBoundsMin = glm::vec3(0.0f), mBoundsMax = glm::vec3(0.0f);
	unsigned int mTriangles = 0;
};
#endif

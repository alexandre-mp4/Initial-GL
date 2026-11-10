#include "Model.h"
#include "Renderer.h"
#include <iostream>
#include <map>
#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf.h"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

// matrice locale d'un noeud : matrice 4x4 ou translation * rotation * échelle
static glm::mat4 nodeLocalMatrix(const cgltf_node* n)
{
	if (n->has_matrix)
		return glm::make_mat4(n->matrix);

	glm::mat4 m(1.0f);
	if (n->has_translation)
		m = glm::translate(m, glm::vec3(n->translation[0], n->translation[1], n->translation[2]));
	if (n->has_rotation)
		m = m * glm::mat4_cast(glm::quat(n->rotation[3], n->rotation[0], n->rotation[1], n->rotation[2]));
	if (n->has_scale)
		m = glm::scale(m, glm::vec3(n->scale[0], n->scale[1], n->scale[2]));
	return m;
}

// texture glTF intégrée au .glb ou fichier voisin ; flipY retourne l'image et inverse V pour garder la convention OpenGL
static Texture2D* loadGltfTexture(const cgltf_texture_view& view, bool srgb, const std::string& dir,
								  std::vector<std::unique_ptr<Texture2D>>& store, std::map<const cgltf_image*, Texture2D*>& cache)
{
	if (!view.texture || !view.texture->image)
		return nullptr;

	const cgltf_image* image = view.texture->image;
	auto it = cache.find(image);
	if (it != cache.end())
		return it->second;

	std::unique_ptr<Texture2D> tex(new Texture2D());
	bool ok = false;
	if (image->buffer_view)
	{
		const unsigned char* data = (const unsigned char*)image->buffer_view->buffer->data + image->buffer_view->offset;
		ok = tex->loadFromMemory(data, image->buffer_view->size, true, srgb, true);
	}
	else if (image->uri)
	{
		ok = tex->loadTexture(dir + image->uri, true, srgb, true);
	}
	if (!ok)
		return nullptr;

	Texture2D* raw = tex.get();
	store.push_back(std::move(tex));
	cache[image] = raw;
	return raw;
}

bool Model::load(const std::string& path)
{
	cgltf_options options = {};
	cgltf_data* data = nullptr;
	if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success)
	{
		std::cerr << "Model : fichier glTF illisible '" << path << "'" << std::endl;
		return false;
	}
	if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success)
	{
		std::cerr << "Model : buffers introuvables pour '" << path << "'" << std::endl;
		cgltf_free(data);
		return false;
	}

	std::string dir = path.substr(0, path.find_last_of("/\\") + 1);

	std::map<const cgltf_image*, Texture2D*> albedoCache, dataCache;
	for (size_t i = 0; i < data->materials_count; i++)
	{
		const cgltf_material& src = data->materials[i];
		Material m;

		if (src.has_pbr_metallic_roughness)
		{
			const cgltf_pbr_metallic_roughness& p = src.pbr_metallic_roughness;
			m.baseColor = glm::make_vec4(p.base_color_factor);
			m.metallic = p.metallic_factor;
			m.roughness = p.roughness_factor;
			m.albedo = loadGltfTexture(p.base_color_texture, COLOR_TEXTURES_SRGB, dir, mTextures, albedoCache);
			m.orm = loadGltfTexture(p.metallic_roughness_texture, false, dir, mTextures, dataCache);
		}
		else if (src.has_pbr_specular_glossiness)
		{
			const cgltf_pbr_specular_glossiness& p = src.pbr_specular_glossiness;
			m.baseColor = glm::make_vec4(p.diffuse_factor);
			m.metallic = 0.0f;
			m.roughness = 1.0f - p.glossiness_factor;
			m.albedo = loadGltfTexture(p.diffuse_texture, COLOR_TEXTURES_SRGB, dir, mTextures, albedoCache);
		}
		m.normal = loadGltfTexture(src.normal_texture, false, dir, mTextures, dataCache);
		m.emissive = glm::make_vec3(src.emissive_factor);
		m.doubleSided = src.double_sided != 0;
		if (src.alpha_mode == cgltf_alpha_mode_mask)
			m.alphaCutoff = src.alpha_cutoff;
		m.blend = (src.alpha_mode == cgltf_alpha_mode_blend);
		mMaterials.push_back(m);
	}
	mMaterials.push_back(Material());
	int defaultMaterial = (int)mMaterials.size() - 1;

	// noeuds rangés dans un tableau, les parents avant les enfants
	std::map<const cgltf_node*, int> nodeIndex;
	std::map<const cgltf_mesh*, std::vector<int>> meshPrims;

	struct Walker
	{
		static void visit(Model& model, cgltf_data* data, const cgltf_node* n, int parent,
						  std::map<const cgltf_node*, int>& nodeIndex, std::map<const cgltf_mesh*, std::vector<int>>& meshPrims,
						  int defaultMaterial)
		{
			Node node;
			node.name = n->name ? n->name : "";
			node.parent = parent;
			node.local = nodeLocalMatrix(n);
			int me = (int)model.mNodes.size();
			nodeIndex[n] = me;

			if (n->mesh)
			{
				auto found = meshPrims.find(n->mesh);
				if (found == meshPrims.end())
				{
					std::vector<int> created;
					for (size_t pi = 0; pi < n->mesh->primitives_count; pi++)
					{
						const cgltf_primitive& prim = n->mesh->primitives[pi];
						if (prim.type != cgltf_primitive_type_triangles)
							continue;

						const cgltf_accessor *accPos = nullptr, *accNrm = nullptr, *accUV = nullptr;
						for (size_t a = 0; a < prim.attributes_count; a++)
						{
							const cgltf_attribute& at = prim.attributes[a];
							if (at.type == cgltf_attribute_type_position) accPos = at.data;
							else if (at.type == cgltf_attribute_type_normal) accNrm = at.data;
							else if (at.type == cgltf_attribute_type_texcoord && at.index == 0) accUV = at.data;
						}
						if (!accPos)
							continue;

						size_t count = accPos->count;
						std::vector<float> pos(count * 3), nrm(count * 3, 0.0f), uv(count * 2, 0.0f);
						cgltf_accessor_unpack_floats(accPos, pos.data(), count * 3);
						if (accNrm) cgltf_accessor_unpack_floats(accNrm, nrm.data(), count * 3);
						if (accUV)  cgltf_accessor_unpack_floats(accUV, uv.data(), count * 2);

						std::vector<Vertex> vertices(count);
						for (size_t v = 0; v < count; v++)
						{
							vertices[v].position = glm::vec3(pos[v * 3], pos[v * 3 + 1], pos[v * 3 + 2]);
							vertices[v].normal = glm::vec3(nrm[v * 3], nrm[v * 3 + 1], nrm[v * 3 + 2]);
							vertices[v].texCoords = glm::vec2(uv[v * 2], 1.0f - uv[v * 2 + 1]);
						}

						std::vector<unsigned int> indices;
						if (prim.indices)
						{
							indices.resize(prim.indices->count);
							for (size_t k = 0; k < indices.size(); k++)
								indices[k] = (unsigned int)cgltf_accessor_read_index(prim.indices, k);
						}
						else
						{
							indices.resize(count);
							for (size_t k = 0; k < count; k++) indices[k] = (unsigned int)k;
						}

						Primitive p;
						p.mesh.reset(new Mesh());
						if (!p.mesh->loadIndexed(vertices, indices))
							continue;
						p.material = prim.material ? (int)(prim.material - data->materials) : defaultMaterial;

						model.mTriangles += (unsigned int)indices.size() / 3;
						model.mPrimitives.push_back(std::move(p));
						created.push_back((int)model.mPrimitives.size() - 1);
					}
					found = meshPrims.insert(std::make_pair(n->mesh, created)).first;
				}
				node.primitives = found->second;
			}

			model.mNodes.push_back(node);
			for (size_t c = 0; c < n->children_count; c++)
				visit(model, data, n->children[c], me, nodeIndex, meshPrims, defaultMaterial);
		}
	};

	const cgltf_scene* scene = data->scene ? data->scene : &data->scenes[0];
	for (size_t i = 0; i < scene->nodes_count; i++)
		Walker::visit(*this, data, scene->nodes[i], -1, nodeIndex, meshPrims, defaultMaterial);

	std::vector<glm::mat4> world(mNodes.size());
	for (size_t i = 0; i < mNodes.size(); i++)
		world[i] = (mNodes[i].parent >= 0 ? world[mNodes[i].parent] : glm::mat4(1.0f)) * mNodes[i].local;
	bool first = true;
	for (const auto& kv : nodeIndex)
	{
		if (!kv.first->mesh) continue;
		for (size_t pi = 0; pi < kv.first->mesh->primitives_count; pi++)
		{
			for (size_t a = 0; a < kv.first->mesh->primitives[pi].attributes_count; a++)
			{
				const cgltf_attribute& at = kv.first->mesh->primitives[pi].attributes[a];
				if (at.type != cgltf_attribute_type_position || !at.data->has_min || !at.data->has_max) continue;
				for (int corner = 0; corner < 8; corner++)
				{
					glm::vec3 c((corner & 1) ? at.data->max[0] : at.data->min[0],
								(corner & 2) ? at.data->max[1] : at.data->min[1],
								(corner & 4) ? at.data->max[2] : at.data->min[2]);
					glm::vec3 w = glm::vec3(world[kv.second] * glm::vec4(c, 1.0f));
					if (first) { mBoundsMin = mBoundsMax = w; first = false; }
					mBoundsMin = glm::min(mBoundsMin, w);
					mBoundsMax = glm::max(mBoundsMax, w);
				}
			}
		}
	}

	cgltf_free(data);
	std::cout << "Model '" << path << "' : " << mNodes.size() << " noeuds, " << mPrimitives.size()
			  << " primitives, " << mTriangles << " triangles, " << mMaterials.size() - 1 << " matériaux" << std::endl;
	return true;
}

int Model::findNode(const std::string& name) const
{
	for (size_t i = 0; i < mNodes.size(); i++)
		if (mNodes[i].name == name)
			return (int)i;
	return -1;
}

glm::vec3 Model::getNodePosition(int node) const
{
	glm::mat4 m(1.0f);
	for (int i = node; i >= 0; i = mNodes[i].parent)
		m = mNodes[i].local * m;
	return glm::vec3(m[3]);
}

void Model::draw(Renderer& renderer, const glm::mat4& root, const std::vector<glm::mat4>* extra)
{
	// matrice monde = monde(parent) * locale * extra
	std::vector<glm::mat4> world(mNodes.size());
	for (size_t i = 0; i < mNodes.size(); i++)
	{
		glm::mat4 m = mNodes[i].local;
		if (extra) m = m * (*extra)[i];
		world[i] = (mNodes[i].parent >= 0 ? world[mNodes[i].parent] : root) * m;
	}

	// deux passes : opaques puis transparents (vitres)
	for (int pass = 0; pass < 2; pass++)
	{
		for (size_t i = 0; i < mNodes.size(); i++)
		{
			for (int pi : mNodes[i].primitives)
			{
				const Material& mat = mMaterials[mPrimitives[pi].material];
				if (mat.blend != (pass == 1))
					continue;
				renderer.setMaterial(mat);
				renderer.draw(*mPrimitives[pi].mesh, world[i]);
			}
		}
	}
}

void Model::drawNodeInstanced(Renderer& renderer, int node, GLuint instanceVbo, size_t byteOffset, int count, float treeHeight)
{
	for (int pi : mNodes[node].primitives)
	{
		renderer.setMaterial(mMaterials[mPrimitives[pi].material]);
		renderer.drawInstanced(*mPrimitives[pi].mesh, instanceVbo, byteOffset, count, treeHeight);
	}
}

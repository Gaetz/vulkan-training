#pragma once
#include <glm/glm.hpp>
#include <assimp/scene.h>

#include <vector>
using std::vector;

#include "VulkanMesh.h"

class VulkanMeshModel
{
public:
	VulkanMeshModel();
	VulkanMeshModel(vector<VulkanMesh> meshesP);
	~VulkanMeshModel();

	size_t getMeshCount() { return meshes.size(); };
	VulkanMesh* getMesh(size_t index);

	glm::mat4 getModel() { return model; };
	void setModel(glm::mat4 modelP) { model = modelP; }
	void destroyMeshModel();

	static vector<string> loadMaterials(const aiScene* scene);
	static vector<VulkanMesh> loadNode(VkPhysicalDevice physicalDeviceP, VkDevice deviceP, VkQueue transferQueue, 
		VkCommandPool transferCommandPool, aiNode* node, const aiScene* scene, vector<int> matToTex);
	static VulkanMesh loadMesh(VkPhysicalDevice physicalDeviceP, VkDevice deviceP, VkQueue transferQueue,
		VkCommandPool transferCommandPool, aiMesh* node, const aiScene* scene, vector<int> matToTex);

private:
	vector<VulkanMesh> meshes;
	glm::mat4 model;
};


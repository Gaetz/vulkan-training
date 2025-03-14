#include "VulkanMeshModel.h"

VulkanMeshModel::VulkanMeshModel(vector<VulkanMesh> meshesP)
	: meshes(meshesP), model(glm::mat4(1.0f))
{
}

VulkanMesh* VulkanMeshModel::getMesh(size_t index)
{
	if (index >= meshes.size())
	{
		throw std::runtime_error("Attempted to access a mesh with not attributed index");
	}
	return &meshes[index];
}

void VulkanMeshModel::destroyMeshModel()
{
	for (auto& mesh : meshes)
	{
		mesh.destroyBuffers();
	}
}

vector<string> VulkanMeshModel::loadMaterials(const aiScene* scene)
{
	// Create one-to-one size list of texture
	vector<string> textures(scene->mNumMaterials);
	// Go through each material and copy its texture file name if it exists
	for (size_t i = 0; i < textures.size(); ++i)
	{
		// Get material
		aiMaterial* material = scene->mMaterials[i];
		// Initialize texture name with empty string
		textures[i] = "";
		// Check for a diffuse texture
		if (material->GetTextureCount(aiTextureType_DIFFUSE))
		{
			// Get the path of the texture file
			aiString path;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
			{
				// Cut off any absolute directory information already present
				int index = string(path.data).rfind("\\");
				string filename = string(path.data).substr(index + 1);
				textures[i] = filename;
			}
		}
	}
	return textures;
}

VulkanMesh VulkanMeshModel::loadMesh(vk::PhysicalDevice physicalDeviceP,
	vk::Device deviceP, vk::Queue transferQueue,
	vk::CommandPool transferCommandPool, aiMesh* mesh, 
	const aiScene* scene, vector<int> matToTex)
{
	vector<Vertex> vertices(mesh->mNumVertices);
	vector<uint32_t> indices;

	// Copy all vertices
	for (size_t i = 0; i < mesh->mNumVertices; ++i)
	{
		// Position
		vertices[i].pos = {
			mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z
		};
		// Tex coords if they exists
		if (mesh->mTextureCoords[0])
		{
			vertices[i].tex = {
				mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y
			};
		}
		else
		{
			vertices[i].tex = { 0.0f, 0.0f };

		}
		// Vertex color (white)
		vertices[i].col = { 1.0f, 1.0f, 1.0f };
	}

	// Copy all indices, stored by face (triangle)
	for (size_t i = 0; i < mesh->mNumFaces; ++i)
	{
		aiFace face = mesh->mFaces[i];
		for (size_t j = 0; j < face.mNumIndices; ++j)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	// Create new mesh
	VulkanMesh newMesh = VulkanMesh(physicalDeviceP, deviceP, transferQueue,
		transferCommandPool, &vertices, &indices, matToTex[mesh->mMaterialIndex]);
	return newMesh;
}

vector<VulkanMesh> VulkanMeshModel::loadNode(vk::PhysicalDevice physicalDeviceP,
	vk::Device deviceP, vk::Queue transferQueue,
	vk::CommandPool transferCommandPool, aiNode* node,
	const aiScene* scene, vector<int> matToTex)
{
	vector<VulkanMesh> meshes;
	// Go through each mesh at this node and create it, then add it to our meshList
	for (size_t i = 0; i < node->mNumMeshes; ++i)
	{
		// Load mesh
		meshes.push_back(loadMesh(physicalDeviceP, deviceP, transferQueue,
			transferCommandPool, scene->mMeshes[node->mMeshes[i]], scene, matToTex));
		// Explanation of scene->mMeshes[node->mMeshes[i]]:
		// The scene actually hold the data for the meshes, and the nodes store ids of
		// meshes, that relate to the scene meshes.
	}

	// Go through each node attached to this node and load it,
	// then append their meshes to this node's meshes
	for (size_t i = 0; i < node->mNumChildren; ++i)
	{
		vector<VulkanMesh> newMeshes = loadNode(physicalDeviceP, deviceP, transferQueue,
			transferCommandPool, node->mChildren[i], scene, matToTex);
		meshes.insert(end(meshes), begin(newMeshes), end(newMeshes));
	}

	return meshes;
}
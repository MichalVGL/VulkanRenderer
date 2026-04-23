#ifndef SCENE_H
#define SCENE_H

#include "pch.h"

namespace dae
{
	class Renderer;
	class Scene final
	{
	public:
		Scene() = default;
		~Scene() = default;
		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(const Scene&) = delete;
		Scene& operator=(Scene&&) = delete;

		void Init(std::filesystem::path gltfFile);

	private:
	};
}

#endif // !SCENE_H

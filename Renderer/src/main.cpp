#include "pch.h"

int main()
{

	Assimp::Importer importer;

	const aiScene* material = importer.ReadFile(std::string{}, 0);

	std::cout << (material != nullptr) << std::endl;

	std::getchar();

	return 0;
}
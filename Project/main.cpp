#include<Windows.h>

#include<memory>

import Lumina;

import PythonTest;

int32_t WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	Lumina::Python pythonMngr{};
	pythonMngr.Initialize();
	pythonMngr.Test();

	std::unique_ptr<Lumina::Context> context{ std::make_unique<Lumina::Context>() };
	context->Initialize();

	while (context->Run());

	context->Finalize();

	pythonMngr.Finalize();

	return 0;
}
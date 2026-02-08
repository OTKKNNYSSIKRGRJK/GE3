#include<Windows.h>

#include<memory>

import Lumina;

int32_t WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	std::unique_ptr<Lumina::Context> context{ new Lumina::Context{} };
	context->Initialize();

	while (context->Run());

	context->Finalize();

	return 0;
}
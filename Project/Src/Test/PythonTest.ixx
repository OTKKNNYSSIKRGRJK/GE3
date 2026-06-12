module;

/// With `PY_SSIZE_T_CLEAN` defined before including `Python.h`,
/// essential standard C library headers will be automatically included.
#define PY_SSIZE_T_CLEAN
#include<Python.h>

export module PythonTest;

import <string>;
import <Windows.h>;

namespace Lumina {
	export class Python {
	public:
		auto Test() -> void {
			PyRun_SimpleString("print('HAROV VAARVDO')");
		}

	public:
		auto Initialize() -> void {
			if (IsInitialized_) { return; }

			std::wstring path{};
			path.resize(256LLU);
			::GetModuleFileNameW(nullptr, path.data(), 256LLU);

			PyConfig config{};
			::PyConfig_InitIsolatedConfig(&config);
			::PyConfig_SetString(&config, &config.program_name, path.data());

			::Py_InitializeFromConfig(&config);
			IsInitialized_ = 1;
		}

		auto Finalize() -> void {
			::Py_Finalize();
		}

	public:
		inline static int IsInitialized_{ 0 };
	};
}
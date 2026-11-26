module;

#include<Windows.h>

// DbgHelp.h			| Debugging tools for Windows such as dump export
// DbgHelp.lib			| DbgHelp implementation
#include<DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")

//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////
//////	//////	//////	//////	//////	//////

export module Lumina.Utils.Debug : Exception;

//****	******	******	******	******	****//

import <string>;
import <format>;

import <fstream>;
import <filesystem>;

import <stdexcept>;

import Lumina.Utils.Time;

//////	//////	//////	//////	//////	//////

namespace Lumina::Utils::Debug {
	export class Exception {
	public:
		static LONG OnCrash(EXCEPTION_POINTERS* exception_);
	};
}

//////	//////	//////	//////	//////	//////
//	ThrowIfFalse							//
//	ThrowIfFailed							//
//////	//////	//////	//////	//////	//////

// Reference: https://qiita.com/Nabetani/items/983a7bf0b72f45c860e3

namespace Lumina::Utils::Debug {
	export template<typename ExceptionType = std::runtime_error>
		requires(std::is_base_of_v<std::exception, ExceptionType>)
	class ThrowIfFalse : public std::string_view {
	public:
		constexpr ThrowIfFalse(std::string&& str_) noexcept :
			std::string_view{ str_.data() } {}
	};
	
	export template<typename ExceptionType>
	inline void operator||(bool condition_, ThrowIfFalse<ExceptionType>&& msg_) {
		if (!condition_) {
			throw ExceptionType{ msg_.data() };
		}
	}

	//****	******	******	******	******	****//

	export template<typename ExceptionType = std::runtime_error>
		requires(std::is_base_of_v<std::exception, ExceptionType>)
	class ThrowIfFailed : public std::string_view {
	public:
		constexpr ThrowIfFailed(std::string&& str_) noexcept :
			std::string_view{ str_.data() } {}
	};

	export template<typename ExceptionType>
	inline void operator||(HRESULT result_, ThrowIfFailed<ExceptionType>&& msg_) {
		if (FAILED(result_)) {
			throw ExceptionType{ msg_.data() };
		}
	}
}
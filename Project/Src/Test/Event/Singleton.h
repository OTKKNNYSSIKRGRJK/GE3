//////	//////	//////	//////	//////	//////	//////	//////	//////
//	Singleton.h															//
//	EventSystem.h が #include していたが、Src/Test/Event に無かったので		//
//	2026-11-11 に最小限で用意したもの（元の Singleton.h の中身は不明）。	//
//////	//////	//////	//////	//////	//////	//////	//////	//////
// INNO_SINGLETON(T) をクラスの中に書くと
//   - T::Instance() … 関数内 static の T を 1 つ作り、そのポインタを返す（初回の呼び出しで生成。スレッドセーフ）
//   - コピー・ムーブ禁止、コンストラクタ／デストラクタは private
// 元の EventSystem.h は `INNO_SINGLETON(EventManager);` の後に `public:` を書いているので、マクロの最後は private のままでよい。

#pragma once

#define INNO_SINGLETON(T)\
public:\
	static T* Instance() {\
		static T instance{};\
		return &instance;\
	}\
	T(T const&) = delete;\
	T& operator=(T const&) = delete;\
	T(T&&) = delete;\
	T& operator=(T&&) = delete;\
private:\
	T() = default;\
	~T() = default

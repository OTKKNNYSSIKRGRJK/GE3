export module Lumina.Editor.DX12;

namespace Lumina::Editor {
	export class DirectX12 {
	public:
		void Update();

	public:
		void Initialize();
		void Finalize() noexcept;

	public:
		~DirectX12() noexcept;

	private:
		class Impl;
		Impl* Impl_{ nullptr };
	};
}
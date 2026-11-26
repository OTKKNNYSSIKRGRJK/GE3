export module Lumina.Mixins;

//////	//////	//////	//////	//////	//////

export namespace Lumina {
	template<typename T>
	class NonCopyable {
	protected:
		NonCopyable() noexcept = default;
		~NonCopyable() noexcept = default;

	public:
		NonCopyable(NonCopyable&&) noexcept = default;
		NonCopyable& operator=(NonCopyable&&) noexcept = default;

		NonCopyable(const NonCopyable&) = delete;
		T& operator=(const T&) = delete;
	};
}
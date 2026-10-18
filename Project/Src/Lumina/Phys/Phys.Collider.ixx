//////	//////	//////	//////	//////	//////
//	Phys.Collider							//
//////	//////	//////	//////	//////	//////
// オブジェクトに持たせるコライダと、それらを総当たりせずに判定する CollisionWorld。
//
// 使い方（1 フレーム）
//   1. 各オブジェクトが自分のコライダの World を更新する
//        collider.UpdateWorld(親行列);   // メッシュのワールド行列やボーンのソケット行列など
//   2. world.Detect();
//   3. world.Contacts() / world.Exits() を見て反応する
//        for (auto const& pair : world.Contacts()) {
//            if (auto hit{ pair.Match(Layer_Weapon, Layer_Body) }) { ... hit->Self / hit->Other / hit->Info ... }
//        }
//
// 方針
// - コライダは「ローカル形状 + 親行列」。親行列を誰が持っているか（ボーン・メッシュ）は所有者側が知っているので、
//   World の更新は所有者に任せる（World 側は行列へのポインタを握らない）
// - CollisionWorld は Collider* を握るだけ。所有者より先に Remove するか、同じ寿命で持つこと
// - ブロードフェーズは x 軸の Sort and Sweep。数百個程度までを想定
// - 同じ Owner を持つコライダどうしは判定しない（プレイヤーの体と武器など）

export module Lumina.Phys.Collider;

import <cstdint>;
import <variant>;
import <vector>;
import <span>;
import <optional>;

export import Lumina.Phys.Intersect;

import Lumina.Math.Numerics;
import Lumina.Math.Matrix;

export namespace Lumina::Phys {
	using Shape = std::variant<Sphere, AABB, OBB>;

	// 形状の種類を問わない判定・変換
	bool OverlapAny(Shape const& a_, Shape const& b_, Contact* out_ = nullptr) noexcept;
	bool RaycastAny(Ray const& ray_, Shape const& shape_, RayHit* out_ = nullptr) noexcept;
	AABB BoundsOf(Shape const& shape_) noexcept;
	Shape TransformShape(Shape const& shape_, Mat4 const& m_) noexcept;

	//////	//////	//////	//////	//////	//////
	//	Collider								//
	//////	//////	//////	//////	//////	//////

	class Collider {
	public:
		static constexpr uint32_t InvalidID{ 0xFFFFFFFFU };

	public:
		// 親空間でのローカル形状を World に変換する（毎フレーム、Detect の前に呼ぶ）
		void UpdateWorld(Mat4 const& parent_) noexcept;
		// 親なし（Local をそのままワールドとして使う）
		void UpdateWorld() noexcept;

		Shape const& World() const noexcept { return World_; }
		AABB const& Bounds() const noexcept { return Bounds_; }
		uint32_t ID() const noexcept { return ID_; }
		bool IsRegistered() const noexcept { return ID_ != InvalidID; }

	public:
		Shape Local{ Sphere{} };		// 親空間での形状

		uint32_t Layer{ 1U };			// 自分の所属（ビット）
		uint32_t Mask{ 0xFFFFFFFFU };	// 判定する相手の所属（ビット）。両者の Layer & Mask が立って初めて判定する
		bool Enabled{ true };

		void* Owner{ nullptr };			// 反応を振り分けるための持ち主（同じ Owner どうしは判定しない）
		uint32_t UserTag{ 0U };			// 所有者が自由に使う（部位番号など）

	private:
		friend class CollisionWorld;

		Shape World_{ Sphere{} };
		AABB Bounds_{};
		uint32_t ID_{ InvalidID };
	};

	//////	//////	//////	//////	//////	//////
	//	判定結果								//
	//////	//////	//////	//////	//////	//////

	enum class ContactPhase : uint8_t {
		Enter,		// このフレームから重なった
		Stay,		// 前のフレームから続いている
	};

	// 片方の視点に向き直した接触（Normal は Self → Other）
	struct Hit {
		Collider* Self{ nullptr };
		Collider* Other{ nullptr };
		Contact Info{};
		ContactPhase Phase{ ContactPhase::Enter };
	};

	struct ContactPair {
		Collider* A{ nullptr };			// A->ID() < B->ID()
		Collider* B{ nullptr };
		Contact Info{};					// Normal は A → B
		ContactPhase Phase{ ContactPhase::Enter };

		bool Involves(Collider const& collider_) const noexcept { return A == &collider_ || B == &collider_; }

		// self_ 側から見た接触。self_ が含まれなければ nullopt
		std::optional<Hit> Of(Collider const& self_) const noexcept;
		// Layer で向きを揃える。selfLayer_ を持つ側を Self、otherLayer_ を持つ側を Other にする
		// （どちらも成り立たなければ nullopt。両方の向きで成り立つなら A を Self にする）
		std::optional<Hit> Match(uint32_t selfLayer_, uint32_t otherLayer_) const noexcept;
	};

	// 前のフレームで重なっていて、このフレームで離れた（または片方が無効になった）組
	struct ExitPair {
		Collider* A{ nullptr };
		Collider* B{ nullptr };

		bool Involves(Collider const& collider_) const noexcept { return A == &collider_ || B == &collider_; }
	};

	struct WorldRaycastHit {
		Collider* Target{ nullptr };
		RayHit Info{};
	};

	//////	//////	//////	//////	//////	//////
	//	CollisionWorld							//
	//////	//////	//////	//////	//////	//////

	class CollisionWorld {
	public:
		void Add(Collider& collider_);
		void Remove(Collider& collider_);
		void Clear();

		// 登録済みで Enabled なコライダを全部判定する（World は更新済みであること）
		void Detect();

		std::span<ContactPair const> Contacts() const noexcept { return Contacts_; }
		std::span<ExitPair const> Exits() const noexcept { return Exits_; }
		std::span<Collider* const> Colliders() const noexcept { return Colliders_; }

		// layerMask_ に所属するコライダのうち、いちばん手前に当たったもの
		bool Raycast(Ray const& ray_, uint32_t layerMask_, WorldRaycastHit* out_ = nullptr) const;

		// デバッグ表示用の統計
		uint32_t NumBroadphasePairs() const noexcept { return NumBroadphasePairs_; }

	private:
		struct SweepEntry {
			float MinX;
			float MaxX;
			uint32_t Index;
		};
		struct PairRecord {
			uint64_t Key;
			Collider* A;
			Collider* B;
		};

		static constexpr uint64_t MakeKey(uint32_t idA_, uint32_t idB_) noexcept {
			return (static_cast<uint64_t>(idA_) << 32U) | static_cast<uint64_t>(idB_);
		}
		static bool ContainsKey(std::vector<PairRecord> const& sorted_, uint64_t key_) noexcept;

	private:
		std::vector<Collider*> Colliders_{};
		std::vector<SweepEntry> Sweep_{};
		std::vector<ContactPair> Contacts_{};
		std::vector<ExitPair> Exits_{};
		std::vector<PairRecord> PrevPairs_{};
		std::vector<PairRecord> CurrPairs_{};

		uint32_t NextID_{ 0U };
		uint32_t NumBroadphasePairs_{ 0U };
	};
}

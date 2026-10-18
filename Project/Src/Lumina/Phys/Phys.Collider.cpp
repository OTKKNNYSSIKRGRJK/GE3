module Lumina.Phys.Collider;

import <algorithm>;

#undef min
#undef max

namespace Lumina::Phys {
	//////	//////	//////	//////	//////	//////
	//	形状を問わない操作						//
	//////	//////	//////	//////	//////	//////

	bool OverlapAny(Shape const& a_, Shape const& b_, Contact* out_) noexcept {
		return std::visit(
			[out_](auto const& a, auto const& b) -> bool { return Overlap(a, b, out_); },
			a_, b_
		);
	}

	bool RaycastAny(Ray const& ray_, Shape const& shape_, RayHit* out_) noexcept {
		return std::visit(
			[&ray_, out_](auto const& s) -> bool { return Raycast(ray_, s, out_); },
			shape_
		);
	}

	AABB BoundsOf(Shape const& shape_) noexcept {
		return std::visit([](auto const& s) -> AABB { return ToAABB(s); }, shape_);
	}

	Shape TransformShape(Shape const& shape_, Mat4 const& m_) noexcept {
		return std::visit([&m_](auto const& s) -> Shape { return Transform(s, m_); }, shape_);
	}

	//////	//////	//////	//////	//////	//////
	//	Collider								//
	//////	//////	//////	//////	//////	//////

	void Collider::UpdateWorld(Mat4 const& parent_) noexcept {
		World_ = TransformShape(Local, parent_);
		Bounds_ = BoundsOf(World_);
	}

	void Collider::UpdateWorld() noexcept {
		World_ = Local;
		Bounds_ = BoundsOf(World_);
	}

	//////	//////	//////	//////	//////	//////
	//	ContactPair								//
	//////	//////	//////	//////	//////	//////

	std::optional<Hit> ContactPair::Of(Collider const& self_) const noexcept {
		if (A == &self_) { return Hit{ A, B, Info, Phase }; }
		if (B == &self_) { return Hit{ B, A, Flipped(Info), Phase }; }
		return std::nullopt;
	}

	std::optional<Hit> ContactPair::Match(uint32_t selfLayer_, uint32_t otherLayer_) const noexcept {
		if ((A->Layer & selfLayer_) && (B->Layer & otherLayer_)) { return Hit{ A, B, Info, Phase }; }
		if ((B->Layer & selfLayer_) && (A->Layer & otherLayer_)) { return Hit{ B, A, Flipped(Info), Phase }; }
		return std::nullopt;
	}

	//////	//////	//////	//////	//////	//////
	//	CollisionWorld							//
	//////	//////	//////	//////	//////	//////

	void CollisionWorld::Add(Collider& collider_) {
		if (collider_.IsRegistered()) { return; }
		collider_.ID_ = NextID_++;
		Colliders_.push_back(&collider_);
	}

	void CollisionWorld::Remove(Collider& collider_) {
		if (!collider_.IsRegistered()) { return; }

		Collider* const target{ &collider_ };
		std::erase(Colliders_, target);

		// このコライダを指す記録を全部捨てる（Exit も出さない）
		std::erase_if(PrevPairs_, [target](PairRecord const& r_) { return r_.A == target || r_.B == target; });
		std::erase_if(CurrPairs_, [target](PairRecord const& r_) { return r_.A == target || r_.B == target; });
		std::erase_if(Contacts_, [target](ContactPair const& p_) { return p_.A == target || p_.B == target; });
		std::erase_if(Exits_, [target](ExitPair const& p_) { return p_.A == target || p_.B == target; });

		collider_.ID_ = Collider::InvalidID;
	}

	void CollisionWorld::Clear() {
		for (Collider* collider : Colliders_) {
			collider->ID_ = Collider::InvalidID;
		}
		Colliders_.clear();
		Sweep_.clear();
		Contacts_.clear();
		Exits_.clear();
		PrevPairs_.clear();
		CurrPairs_.clear();
	}

	bool CollisionWorld::ContainsKey(std::vector<PairRecord> const& sorted_, uint64_t key_) noexcept {
		auto const it{
			std::lower_bound(
				sorted_.begin(), sorted_.end(), key_,
				[](PairRecord const& r_, uint64_t k_) { return r_.Key < k_; }
			)
		};
		return it != sorted_.end() && it->Key == key_;
	}

	void CollisionWorld::Detect() {
		Contacts_.clear();
		Exits_.clear();
		CurrPairs_.clear();
		NumBroadphasePairs_ = 0U;

		//----	ブロードフェーズ: x 軸で Sort and Sweep	----//

		Sweep_.clear();
		for (uint32_t idx{ 0U }; idx < static_cast<uint32_t>(Colliders_.size()); ++idx) {
			Collider const& collider{ *Colliders_[idx] };
			if (!collider.Enabled) { continue; }
			Sweep_.push_back(SweepEntry{ collider.Bounds_.Min.x, collider.Bounds_.Max.x, idx });
		}
		std::sort(
			Sweep_.begin(), Sweep_.end(),
			[](SweepEntry const& l_, SweepEntry const& r_) { return l_.MinX < r_.MinX; }
		);

		for (size_t i{ 0U }; i < Sweep_.size(); ++i) {
			Collider* const ci{ Colliders_[Sweep_[i].Index] };

			for (size_t j{ i + 1U }; j < Sweep_.size(); ++j) {
				// x の区間が重ならなくなったら、それ以降も重ならない
				if (Sweep_[j].MinX > Sweep_[i].MaxX) { break; }

				Collider* const cj{ Colliders_[Sweep_[j].Index] };

				if (ci->Owner != nullptr && ci->Owner == cj->Owner) { continue; }
				if (!(ci->Layer & cj->Mask) || !(cj->Layer & ci->Mask)) { continue; }

				AABB const& bi{ ci->Bounds_ };
				AABB const& bj{ cj->Bounds_ };
				if (bi.Max.y < bj.Min.y || bj.Max.y < bi.Min.y) { continue; }
				if (bi.Max.z < bj.Min.z || bj.Max.z < bi.Min.z) { continue; }

				++NumBroadphasePairs_;

				//----	ナローフェーズ			----//

				// ID の小さい方を A にそろえる（結果の並びと Enter/Stay の照合を安定させるため）
				Collider* a{ ci };
				Collider* b{ cj };
				if (b->ID_ < a->ID_) {
					a = cj;
					b = ci;
				}

				Contact contact{};
				if (!OverlapAny(a->World_, b->World_, &contact)) { continue; }

				Contacts_.push_back(ContactPair{ a, b, contact, ContactPhase::Enter });
			}
		}

		//----	Enter / Stay / Exit			----//

		std::sort(
			Contacts_.begin(), Contacts_.end(),
			[](ContactPair const& l_, ContactPair const& r_) {
				return MakeKey(l_.A->ID_, l_.B->ID_) < MakeKey(r_.A->ID_, r_.B->ID_);
			}
		);

		for (ContactPair& pair : Contacts_) {
			uint64_t const key{ MakeKey(pair.A->ID_, pair.B->ID_) };
			pair.Phase = ContainsKey(PrevPairs_, key) ? ContactPhase::Stay : ContactPhase::Enter;
			CurrPairs_.push_back(PairRecord{ key, pair.A, pair.B });		// Contacts_ がソート済みなのでこちらもソート済み
		}

		for (PairRecord const& prev : PrevPairs_) {
			if (!ContainsKey(CurrPairs_, prev.Key)) {
				Exits_.push_back(ExitPair{ prev.A, prev.B });
			}
		}

		PrevPairs_.swap(CurrPairs_);
	}

	bool CollisionWorld::Raycast(Ray const& ray_, uint32_t layerMask_, WorldRaycastHit* out_) const {
		bool found{ false };
		float bestT{ Infinity };
		WorldRaycastHit best{};

		for (Collider* collider : Colliders_) {
			if (!collider->Enabled || !(collider->Layer & layerMask_)) { continue; }

			// 外接 AABB で先に落とす
			RayHit boundsHit{};
			if (!Phys::Raycast(ray_, collider->Bounds_, &boundsHit) || boundsHit.T > bestT) { continue; }

			RayHit hit{};
			if (!RaycastAny(ray_, collider->World_, &hit) || hit.T >= bestT) { continue; }

			found = true;
			bestT = hit.T;
			best = WorldRaycastHit{ collider, hit };
		}

		if (found && out_) { *out_ = best; }
		return found;
	}
}

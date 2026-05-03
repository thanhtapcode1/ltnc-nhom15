#pragma once
// ════════════════════════════════════════════════════════════
//  Skills.hpp  —  Các skill cụ thể kế thừa ISkill
//
//  Danh sách:
//    SingleShot  — 1 viên thẳng theo hướng nhìn
//    ConeShot    — N viên xòe hình nón
//    BurstShot   — bọc skill khác, bắn nhiều đợt
//    OrbitShot   — bắn theo 4 hướng xung quanh (ví dụ thêm skill mới)
//
//  CÁCH THÊM SKILL MỚI: copy pattern OrbitShot, đổi logic tryFire()
// ════════════════════════════════════════════════════════════
#include <cmath>
#include <memory>

#include "ISkill.hpp"

static constexpr float SKILL_PI = 3.14159265f;

// ── Utility: normalize vector ────────────────────────────────
inline sf::Vector2f normalize(sf::Vector2f v) {
  float len = std::sqrt(v.x * v.x + v.y * v.y);
  return (len > 0.001f) ? v / len : sf::Vector2f{0.f, 1.f};
}

// ════════════════════════════════════════════════════════════
//  SingleShot — 1 viên, kỹ năng cơ bản
// ════════════════════════════════════════════════════════════
class SingleShot : public ISkill {
 public:
  explicit SingleShot(float cooldown = 0.5f)
      : ISkill("single", "Single Shot", cooldown) {
    info_.description = "Ban 1 vien dan thang huong nhin";
    info_.maxLevel = 10;
  }

  std::vector<ShotData> tryFire(sf::Vector2f origin, sf::Vector2f facing,
                                float dt) override {
    tickCooldown(dt);
    if (!isReady()) return {};

    float len = std::sqrt(facing.x * facing.x + facing.y * facing.y);
    if (len < 0.001f) return {};

    resetCooldown();
    ShotData s;
    s.origin = origin;
    s.directions.push_back(facing / len);
    return {s};
  }

  // Upgrade: giảm cooldown 10% mỗi level
  bool upgrade() override {
    if (info_.level >= info_.maxLevel) return false;
    ++info_.level;
    setCooldown(cooldownMax_ * 0.90f);
    return true;
  }
};

// ════════════════════════════════════════════════════════════
//  ConeShot — N viên xòe quạt quanh hướng nhìn
// ════════════════════════════════════════════════════════════
class ConeShot : public ISkill {
 public:
  // bullets_   : số viên (bắt đầu = 3)
  // spreadDeg_ : nửa góc nón (bắt đầu = 20 độ)
  ConeShot(int bullets = 3, float spreadDeg = 20.f, float cooldown = 0.6f)
      : ISkill("cone", "Cone Shot", cooldown),
        bullets_(bullets),
        spreadDeg_(spreadDeg) {
    info_.description = "Ban nhieu vien theo hinh non";
    info_.maxLevel = 6;
  }

  std::vector<ShotData> tryFire(sf::Vector2f origin, sf::Vector2f facing,
                                float dt) override {
    tickCooldown(dt);
    if (!isReady()) return {};

    float len = std::sqrt(facing.x * facing.x + facing.y * facing.y);
    if (len < 0.001f) return {};

    resetCooldown();
    float baseAngle = std::atan2(facing.y, facing.x);
    float halfSpread = spreadDeg_ * SKILL_PI / 180.f;

    ShotData s;
    s.origin = origin;
    for (int i = 0; i < bullets_; ++i) {
      float t = (bullets_ == 1) ? 0.5f : (float)i / (bullets_ - 1);
      float offset = -halfSpread + t * 2.f * halfSpread;
      float a = baseAngle + offset;
      s.directions.push_back({std::cos(a), std::sin(a)});
    }
    return {s};
  }

  // Upgrade: thêm 2 viên mỗi 2 level, tăng góc mỗi level lẻ
  bool upgrade() override {
    if (info_.level >= info_.maxLevel) return false;
    ++info_.level;
    if (info_.level % 2 == 0)
      bullets_ = std::min(bullets_ + 2, 9);
    else
      spreadDeg_ = std::min(spreadDeg_ + 5.f, 45.f);
    return true;
  }

  int getBullets() const { return bullets_; }
  float getSpread() const { return spreadDeg_; }

 private:
  int bullets_;
  float spreadDeg_;
};

// ════════════════════════════════════════════════════════════
//  BurstShot — bọc skill khác, bắn N đợt liên tiếp
//
//  Dùng Decorator pattern: inner_ là skill thực sự bắn đạn.
//  BurstShot chỉ điều khiển timing giữa các đợt.
// ════════════════════════════════════════════════════════════
class BurstShot : public ISkill {
 public:
  // inner      : skill được bọc (SingleShot hoặc ConeShot)
  // totalBursts: tổng số đợt mỗi lần kích hoạt (bắt đầu = 2)
  // burstDelay : giây giữa các đợt
  BurstShot(std::unique_ptr<ISkill> inner, int totalBursts = 2,
            float burstDelay = 0.12f)
      : ISkill("burst", "Burst Shot", inner->getCooldown()),
        inner_(std::move(inner)),
        totalBursts_(totalBursts),
        burstDelay_(burstDelay) {
    info_.description = "Ban nhieu dot lien tiep";
    info_.maxLevel = 5;
  }

  std::vector<ShotData> tryFire(sf::Vector2f origin, sf::Vector2f facing,
                                float dt) override {
    tickCooldown(dt);

    std::vector<ShotData> result;

    // ── Xử lý đợt đang chạy ─────────────────────────────
    if (remainingBursts_ > 0) {
      burstTimer_ -= dt;
      if (burstTimer_ <= 0.f) {
        // Bypass cooldown của inner bằng cách truyền dt=999
        auto shots = inner_->tryFire(savedOrigin_, savedFacing_, 999.f);
        result.insert(result.end(), shots.begin(), shots.end());
        --remainingBursts_;
        burstTimer_ = burstDelay_;
      }
      return result;
    }

    // ── Kích hoạt đợt đầu ───────────────────────────────
    if (!isReady()) return {};
    float len = std::sqrt(facing.x * facing.x + facing.y * facing.y);
    if (len < 0.001f) return {};

    resetCooldown();
    savedOrigin_ = origin;
    savedFacing_ = facing;

    auto shots = inner_->tryFire(origin, facing, 999.f);
    result.insert(result.end(), shots.begin(), shots.end());

    remainingBursts_ = totalBursts_ - 1;
    burstTimer_ = burstDelay_;
    return result;
  }

  // Upgrade: thêm 1 đợt mỗi level
  bool upgrade() override {
    if (info_.level >= info_.maxLevel) return false;
    ++info_.level;
    ++totalBursts_;
    return true;
  }

  // Cho phép upgrade inner skill (vd: Single→Cone sau khi mở Burst)
  ISkill* getInner() const { return inner_.get(); }

 private:
  std::unique_ptr<ISkill> inner_;
  int totalBursts_;
  float burstDelay_;

  // Trạng thái burst đang chạy
  int remainingBursts_ = 0;
  float burstTimer_ = 0.f;
  sf::Vector2f savedOrigin_;
  sf::Vector2f savedFacing_;
};

// ════════════════════════════════════════════════════════════
//  OrbitShot — bắn 4 viên theo 4 hướng vuông góc
//
//  Ví dụ minh hoạ cách thêm skill mới hoàn toàn khác.
//  Không phụ thuộc vào hướng nhìn của nhân vật.
// ════════════════════════════════════════════════════════════
class OrbitShot : public ISkill {
 public:
  explicit OrbitShot(int rays = 4, float cooldown = 1.0f)
      : ISkill("orbit", "Orbit Shot", cooldown), rays_(rays) {
    info_.description = "Ban theo moi huong xung quanh";
    info_.maxLevel = 4;
  }

  std::vector<ShotData> tryFire(sf::Vector2f origin, sf::Vector2f /*facing*/,
                                float dt) override {
    tickCooldown(dt);
    if (!isReady()) return {};

    resetCooldown();
    orbitAngle_ += rotateSpeed_;  // xoay mỗi lần bắn

    ShotData s;
    s.origin = origin;
    float step = 2.f * SKILL_PI / rays_;
    for (int i = 0; i < rays_; ++i) {
      float a = orbitAngle_ + step * i;
      s.directions.push_back({std::cos(a), std::sin(a)});
    }
    return {s};
  }

  bool upgrade() override {
    if (info_.level >= info_.maxLevel) return false;
    ++info_.level;
    rays_ = std::min(rays_ + 2, 12);
    return true;
  }

 private:
  int rays_;
  float orbitAngle_ = 0.f;
  float rotateSpeed_ = SKILL_PI / 8.f;  // xoay 22.5° mỗi lần bắn
};
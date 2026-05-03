#pragma once
// ════════════════════════════════════════════════════════════
//  IMonster.hpp  —  Abstract base cho mọi loại quái vật
//
//  CÁCH THÊM QUÁI MỚI:
//    1. Tạo file MonsterXxx.hpp / MonsterXxx.cpp
//    2. Kế thừa IMonster
//    3. Implement 4 pure virtual: update, draw, drawDebug, overlapsPoint
//    4. Trong constructor: set pos_, hp_, maxHp_, expValue_,
//       attackDamage_, attackRange_, attackCooldown_
//    5. Đăng ký vào MonsterManager::registerFactory()
//
//  IMonster quản lý:
//    - HP, alive/dead state
//    - Attack cooldown & pending damage (Game đọc qua getAndResetPendingDamage)
//    - Interface chung để BulletManager kiểm tra va chạm
// ════════════════════════════════════════════════════════════
#include <SFML/Graphics.hpp>
#include <cmath>
#include <string>

class IMonster {
 public:
  virtual ~IMonster() = default;

  // ── Core interface (phải implement) ─────────────────────
  virtual void update(float dt, sf::Vector2f playerPos) = 0;
  virtual void draw(sf::RenderTarget& target) const = 0;
  virtual void drawDebug(sf::RenderTarget& target) const {}

  // Trúng đạn
  virtual void takeHit(int damage) = 0;

  // Kiểm tra va chạm với điểm (dùng cho bullet hit-test)
  virtual bool overlapsPoint(sf::Vector2f pt, float radius) const = 0;
  void applyKnockback(sf::Vector2f dir, float force) {
    knockbackVel_ += dir * force;
  }
  // ── Getters (không cần override) ────────────────────────
  sf::Vector2f getPosition() const { return pos_; }
  bool isAlive() const { return alive_; }
  bool isDead() const { return dead_; }
  int getHp() const { return hp_; }
  int getMaxHp() const { return maxHp_; }
  int getExpValue() const { return expValue_; }
  const std::string& getTypeId() const { return typeId_; }

  // Lấy damage tích lũy trong frame này rồi reset về 0
  // Game/MonsterManager gọi để trừ HP player
  int getAndResetPendingDamage() {
    int dmg = pendingDamage_;
    pendingDamage_ = 0;
    return dmg;
  }

  // ── Velocity-based movement (Vampire Survivors style) ───
  // MonsterManager tích lũy separation force, rồi flushSeparation() áp dụng
  void addSeparationForce(sf::Vector2f f) { separationForce_ += f; }

  // Gọi cuối frame để áp separation vào velocity rồi tích hợp vị trí
  // dt: delta time, maxSpeed: tốc độ tối đa của quái loại này
  void flushSeparation(float dt, float maxSpeed) {
    velocity_ += separationForce_;
    separationForce_ = {};

    // Clamp velocity để separation không đẩy quá mạnh
    float spd2 = velocity_.x * velocity_.x + velocity_.y * velocity_.y;
    float limit = maxSpeed * 0.55f;  // separation chỉ được dùng ~55% speed
    if (spd2 > limit * limit) {
      float spd = std::sqrt(spd2);
      velocity_ = velocity_ / spd * limit;
    }

    pos_ += velocity_ * dt;

    // Damping: velocity tắt dần về 0 nhanh (quái không trượt dài)
    velocity_ *= 0.12f;
  }

  // Subclass gọi để di chuyển seek player (thay pos_ += dir*speed*dt)
  // Trả về hướng đã chuẩn hóa để subclass dùng lật sprite
  sf::Vector2f seekMove(sf::Vector2f playerPos, float speed, float dt,
                        float stopRange = 0.f) {
    sf::Vector2f diff = playerPos - pos_;
    float dist2 = diff.x * diff.x + diff.y * diff.y;
    if (dist2 < 1.f) return {};
    float dist = std::sqrt(dist2);
    sf::Vector2f dir = diff / dist;
    if (dist > stopRange) {
      velocity_ += dir * speed;
      // Clamp tổng velocity theo maxSpeed
      float spd2 = velocity_.x * velocity_.x + velocity_.y * velocity_.y;
      if (spd2 > speed * speed) {
        float spd = std::sqrt(spd2);
        velocity_ = velocity_ / spd * speed;
      }
    }
    return dir;
  }

  // Áp velocity vào pos (gọi mỗi frame thay vì pos_ += dir*speed*dt trực tiếp)
  void integrateVelocity(float dt, float damping = 0.80f) {
    pos_ += velocity_ * dt;
    velocity_ *= damping;
  }

  void applyOffset(sf::Vector2f offset) { pos_ += offset; }

 protected:
  // Subclass gọi trong update() để xử lý combat tự động
  // Trả về true nếu vừa đánh player
  bool tickAttack(float dt, sf::Vector2f playerPos) {
    if (attackTimer_ < attackCooldown_) attackTimer_ += dt;
    if (attackTimer_ < attackCooldown_) return false;

    sf::Vector2f diff = playerPos - pos_;
    float distSq = diff.x * diff.x + diff.y * diff.y;
    if (distSq > attackRange_ * attackRange_) return false;

    attackTimer_ = 0.f;
    pendingDamage_ += attackDamage_;
    return true;
  }

  // ── Trường subclass được phép set ───────────────────────
  std::string typeId_ = "unknown";
  sf::Vector2f pos_ = {};
  bool alive_ = true;
  bool dead_ = false;
  int hp_ = 1;
  int maxHp_ = 1;
  int expValue_ = 1;
  int attackDamage_ = 1;
  float attackRange_ = 45.f;
  float attackCooldown_ = 1.5f;

 private:
  float attackTimer_ = 999.f;  // sẵn sàng đánh ngay từ đầu
  int pendingDamage_ = 0;
  sf::Vector2f knockbackVel_ = {};

 protected:
  // Velocity-based movement state (Vampire Survivors style)
  sf::Vector2f velocity_ = {};
  sf::Vector2f separationForce_ = {};
};
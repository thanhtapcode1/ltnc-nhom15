#pragma once
// ════════════════════════════════════════════════════════════
//  FlyEye.hpp  —  Quái bay, kế thừa IMonster
//
//  Trạng thái: Chase → Attack → TakeHit / Death
//  Texture: Flight.png, Attack.png, Death.png, Take_Hit.png
// ════════════════════════════════════════════════════════════
#include <SFML/Graphics.hpp>

#include "IMonster.hpp"

enum class FlyEyeState { Chase, Attack, TakeHit, Death };

class FlyEye : public IMonster {
 public:
  // ── Hằng số ─────────────────────────────────────────────
  static constexpr int FRAME_W = 150;
  static constexpr int FRAME_H = 150;
  static constexpr int FRAMES_FLIGHT = 8;
  static constexpr int FRAMES_ATTACK = 8;
  static constexpr int FRAMES_DEATH = 4;
  static constexpr int FRAMES_HIT = 4;
  static constexpr float SCALE = 1.8f;
  static constexpr float SPEED = 50.f;
  static constexpr float ATTACK_RANGE_PX = 45.f;
  static constexpr float HIT_RADIUS = 18.f;
  static constexpr float FRAME_TIME_FLIGHT = 0.10f;
  static constexpr float FRAME_TIME_ATTACK = 0.08f;
  static constexpr float FRAME_TIME_HIT = 0.10f;
  static constexpr float FRAME_TIME_DEATH = 0.14f;
  static constexpr int MAX_HP = 2;

  explicit FlyEye(sf::Vector2f pos);
  static bool loadTextures();

  // ── IMonster interface ───────────────────────────────────
  void update(float dt, sf::Vector2f playerPos) override;
  void draw(sf::RenderTarget& target) const override;
  void drawDebug(sf::RenderTarget& target) const override;
  void takeHit(int damage) override;
  bool overlapsPoint(sf::Vector2f pt, float radius) const override;

 private:
  FlyEyeState state_ = FlyEyeState::Chase;
  int frame_ = 0;
  float timer_ = 0.f;
  bool animDone_ = false;
  bool flipX_ = false;
  float hoverT_ = 0.f;
  mutable bool hitFlash_ = true;

  void setState(FlyEyeState s);
  void advanceAnim(float dt);
  int currentFrameCount() const;
  float currentFrameTime() const;
  const sf::Texture& currentTex() const;

  static sf::Texture texFlight_, texAttack_, texDeath_, texHit_;
  static bool texturesLoaded_;
};
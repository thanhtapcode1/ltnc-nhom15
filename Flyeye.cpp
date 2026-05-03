#include "FlyEye.hpp"

#include <cmath>
#include <iostream>

sf::Texture FlyEye::texFlight_;
sf::Texture FlyEye::texAttack_;
sf::Texture FlyEye::texDeath_;
sf::Texture FlyEye::texHit_;
bool FlyEye::texturesLoaded_ = false;

bool FlyEye::loadTextures() {
  if (texturesLoaded_) return true;
  texturesLoaded_ = true;
  bool ok = true;
  struct {
    sf::Texture& t;
    const char* f;
  } list[] = {
      {texFlight_, "Flight.png"},
      {texAttack_, "Attack.png"},
      {texDeath_, "Death.png"},
      {texHit_, "Take_Hit.png"},
  };
  for (auto& e : list)
    if (!e.t.loadFromFile(e.f)) {
      std::cerr << "[FlyEye] Missing: " << e.f << "\n";
      ok = false;
    }
  return ok;
}

FlyEye::FlyEye(sf::Vector2f pos) {
  typeId_ = "flyeye";
  pos_ = pos;
  hp_ = MAX_HP;
  maxHp_ = MAX_HP;
  alive_ = true;
  dead_ = false;
  attackRange_ = ATTACK_RANGE_PX;
  attackDamage_ = 1;
  attackCooldown_ = 1.5f;
  expValue_ = 1;
}

// ── Helpers ──────────────────────────────────────────────────
void FlyEye::setState(FlyEyeState s) {
  if (state_ == s) return;
  state_ = s;
  frame_ = 0;
  timer_ = 0.f;
  animDone_ = false;
}

const sf::Texture& FlyEye::currentTex() const {
  switch (state_) {
    case FlyEyeState::Attack:
      return texAttack_;
    case FlyEyeState::TakeHit:
      return texHit_;
    case FlyEyeState::Death:
      return texDeath_;
    default:
      return texFlight_;
  }
}

int FlyEye::currentFrameCount() const {
  switch (state_) {
    case FlyEyeState::Attack:
      return FRAMES_ATTACK;
    case FlyEyeState::TakeHit:
      return FRAMES_HIT;
    case FlyEyeState::Death:
      return FRAMES_DEATH;
    default:
      return FRAMES_FLIGHT;
  }
}

float FlyEye::currentFrameTime() const {
  switch (state_) {
    case FlyEyeState::Attack:
      return FRAME_TIME_ATTACK;
    case FlyEyeState::TakeHit:
      return FRAME_TIME_HIT;
    case FlyEyeState::Death:
      return FRAME_TIME_DEATH;
    default:
      return FRAME_TIME_FLIGHT;
  }
}

void FlyEye::advanceAnim(float dt) {
  timer_ += dt;
  if (timer_ >= currentFrameTime()) {
    timer_ -= currentFrameTime();
    hitFlash_ = !hitFlash_;
    if (++frame_ >= currentFrameCount()) {
      if (state_ == FlyEyeState::Chase)
        frame_ = 0;
      else {
        frame_ = currentFrameCount() - 1;
        animDone_ = true;
      }
    }
  }
}

void FlyEye::takeHit(int damage) {
  if (!alive_ || state_ == FlyEyeState::Death) return;
  hp_ -= damage;
  if (hp_ <= 0) {
    hp_ = 0;
    alive_ = false;
    setState(FlyEyeState::Death);
  } else {
    if (state_ != FlyEyeState::TakeHit) setState(FlyEyeState::TakeHit);
  }
}

bool FlyEye::overlapsPoint(sf::Vector2f pt, float r) const {
  if (dead_) return false;
  sf::Vector2f d = pt - pos_;
  float minD = HIT_RADIUS + r;
  return (d.x * d.x + d.y * d.y) < minD * minD;
}

void FlyEye::update(float dt, sf::Vector2f playerPos) {
  if (dead_) return;
  // ── State transitions ────────────────────────────────────
  switch (state_) {
    case FlyEyeState::Chase:
      break;
    case FlyEyeState::Attack:
      if (animDone_) setState(FlyEyeState::Chase);
      break;
    case FlyEyeState::TakeHit:
      if (animDone_) setState(alive_ ? FlyEyeState::Chase : FlyEyeState::Death);
      break;
    case FlyEyeState::Death:
      if (animDone_) {
        dead_ = true;
        return;
      }
      break;
  }

  // ── Movement & combat ────────────────────────────────────
  if (state_ == FlyEyeState::Chase || state_ == FlyEyeState::Attack) {
    sf::Vector2f diff = playerPos - pos_;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

    if (dist > 1.f) {
      if (dist < ATTACK_RANGE_PX) {
        // Trong range tấn công: dừng di chuyển seek, chỉ đánh
        if (state_ == FlyEyeState::Chase) setState(FlyEyeState::Attack);
        tickAttack(dt, playerPos);
      } else {
        // Seek player bằng velocity (thay vì pos_ += dir*speed*dt trực tiếp)
        sf::Vector2f dir = seekMove(playerPos, SPEED, dt, ATTACK_RANGE_PX);
        flipX_ = (dir.x < 0.f);
      }
    }

    // Hover effect: cộng trực tiếp vào pos_ (không qua velocity)
    hoverT_ += dt * 3.f;
    pos_.y += std::sin(hoverT_) * 2.5f * dt;

    // Integrate velocity vào pos (damping cao = responsive, không trơn quá)
    integrateVelocity(dt, /*damping=*/0.78f);
  }

  advanceAnim(dt);
}

void FlyEye::draw(sf::RenderTarget& target) const {
  if (dead_) return;
  sf::CircleShape shadow(HIT_RADIUS * 0.8f);     // Kích thước bằng 80% hitbox
  shadow.setFillColor(sf::Color(0, 0, 0, 100));  // Màu đen, độ trong suốt ~40%
  shadow.setScale({1.5f, 0.5f});  // Làm bẹt hình tròn thành hình elip
  shadow.setOrigin({HIT_RADIUS * 0.8f, HIT_RADIUS * 0.8f});

  // Vị trí bóng: luôn ở dưới quái vật, không bị ảnh hưởng bởi hiệu ứng hover
  // (bay lên xuống) Lưu ý: pos_.y của FlyEye đã bao gồm std::sin(hoverT_), nếu
  // muốn bóng đứng yên trên mặt đất bạn có thể trừ đi phần dao động đó, nhưng
  // đơn giản nhất là đặt cố định bên dưới pos_
  shadow.setPosition({pos_.x, pos_.y + 50.f});
  target.draw(shadow);
  const sf::Texture& tex =
      (state_ == FlyEyeState::TakeHit) ? texFlight_ : currentTex();
  int frameIdx = frame_;
  if (state_ == FlyEyeState::TakeHit) frameIdx %= FRAMES_FLIGHT;

  sf::Sprite sprite(tex);
  sprite.setTextureRect(sf::IntRect(sf::Vector2i(frameIdx * FRAME_W, 0),
                                    sf::Vector2i(FRAME_W, FRAME_H)));
  sprite.setOrigin({FRAME_W / 2.f, FRAME_H / 2.f});
  float sx = flipX_ ? -SCALE : SCALE;
  sprite.setScale({sx, SCALE});
  sprite.setPosition(pos_);

  if (state_ == FlyEyeState::TakeHit) {
    sprite.setColor(hitFlash_ ? sf::Color(255, 80, 80)
                              : sf::Color(255, 160, 160));
  }
  target.draw(sprite);
}

void FlyEye::drawDebug(sf::RenderTarget& target) const {
  if (dead_) return;

  sf::CircleShape hit(HIT_RADIUS);
  hit.setFillColor(sf::Color(255, 0, 0, 60));
  hit.setOutlineColor(sf::Color::Red);
  hit.setOutlineThickness(0.5f);
  hit.setOrigin({HIT_RADIUS, HIT_RADIUS});
  hit.setPosition(pos_);
  target.draw(hit);

  sf::CircleShape atk(ATTACK_RANGE_PX);
  atk.setFillColor(sf::Color::Transparent);
  atk.setOutlineColor(sf::Color(255, 80, 0, 150));
  atk.setOutlineThickness(0.5f);
  atk.setOrigin({ATTACK_RANGE_PX, ATTACK_RANGE_PX});
  atk.setPosition(pos_);
  target.draw(atk);
}
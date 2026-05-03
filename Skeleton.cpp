#include "Skeleton.hpp"

#include <cmath>
#include <iostream>

sf::Texture Skeleton::texFlight_;
sf::Texture Skeleton::texAttack_;
sf::Texture Skeleton::texDeath_;
bool Skeleton::texturesLoaded_ = false;

bool Skeleton::loadTextures() {
  if (texturesLoaded_) return true;
  texturesLoaded_ = true;
  bool ok = true;
  struct {
    sf::Texture& t;
    const char* f;
  } list[] = {
      {texFlight_,
       "C:\\Users\\LENOVO\\OneDrive\\Desktop\\project - Copy\\hinh "
       "anh\\Walksk.png"},
      {texAttack_,
       "C:\\Users\\LENOVO\\OneDrive\\Desktop\\project - Copy\\hinh "
       "anh\\Attacksk.png"},
      {texDeath_,
       "C:\\Users\\LENOVO\\OneDrive\\Desktop\\project - Copy\\hinh "
       "anh\\Deathsk.png"},

  };
  for (auto& e : list)
    if (!e.t.loadFromFile(e.f)) {
      std::cerr << "[skeleton] Missing: " << e.f << "\n";
      ok = false;
    }
  return ok;
}

Skeleton::Skeleton(sf::Vector2f pos) {
  typeId_ = "skeleton";
  pos_ = pos;
  hp_ = MAX_HP;
  maxHp_ = MAX_HP;
  alive_ = true;
  dead_ = false;
  attackRange_ = ATTACK_RANGE_PX;
  attackDamage_ = 1;
  attackCooldown_ = 1.5f;
  expValue_ = 5;
}

// ── Helpers ──────────────────────────────────────────────────
void Skeleton::setState(SkeletonState s) {
  if (state_ == s) return;
  state_ = s;
  frame_ = 0;
  timer_ = 0.f;
  animDone_ = false;
}

const sf::Texture& Skeleton::currentTex() const {
  switch (state_) {
    case SkeletonState::Attack:
      return texAttack_;
    case SkeletonState::Death:
      return texDeath_;
    default:
      return texFlight_;  // Chase và TakeHit đều dùng texFlight_
  }
}

int Skeleton::currentFrameCount() const {
  switch (state_) {
    case SkeletonState::Attack:
      return FRAMES_ATTACK;
    case SkeletonState::TakeHit:
      return FRAMES_HIT;
    case SkeletonState::Death:
      return FRAMES_DEATH;
    default:
      return FRAMES_FLIGHT;
  }
}

float Skeleton::currentFrameTime() const {
  switch (state_) {
    case SkeletonState::Attack:
      return FRAME_TIME_ATTACK;
    case SkeletonState::TakeHit:
      return FRAME_TIME_HIT;
    case SkeletonState::Death:
      return FRAME_TIME_DEATH;
    default:
      return FRAME_TIME_FLIGHT;
  }
}

void Skeleton::advanceAnim(float dt) {
  timer_ += dt;
  if (timer_ >= currentFrameTime()) {
    timer_ -= currentFrameTime();
    hitFlash_ = !hitFlash_;
    if (++frame_ >= currentFrameCount()) {
      if (state_ == SkeletonState::Chase)
        frame_ = 0;
      else {
        frame_ = currentFrameCount() - 1;
        animDone_ = true;
      }
    }
  }
}

void Skeleton::takeHit(int damage) {
  if (!alive_ || state_ == SkeletonState::Death) return;
  hp_ -= damage;
  if (hp_ <= 0) {
    hp_ = 0;
    alive_ = false;
    setState(SkeletonState::Death);
  } else {
    if (state_ != SkeletonState::TakeHit) setState(SkeletonState::TakeHit);
  }
}

bool Skeleton::overlapsPoint(sf::Vector2f pt, float r) const {
  if (dead_) return false;
  sf::Vector2f d = pt - pos_;
  float minD = HIT_RADIUS + r;
  return (d.x * d.x + d.y * d.y) < minD * minD;
}

void Skeleton::update(float dt, sf::Vector2f playerPos) {
  if (dead_) return;
  switch (state_) {
    case SkeletonState::Chase:
      break;
    case SkeletonState::Attack:
      if (animDone_) setState(SkeletonState::Chase);
      break;
    case SkeletonState::TakeHit:
      if (animDone_)
        setState(alive_ ? SkeletonState::Chase : SkeletonState::Death);
      break;
    case SkeletonState::Death:
      if (animDone_) {
        dead_ = true;
        return;
      }
      break;
  }

  if (state_ == SkeletonState::Chase || state_ == SkeletonState::Attack) {
    sf::Vector2f diff = playerPos - pos_;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

    if (dist > 1.f) {
      if (dist < ATTACK_RANGE_PX) {
        // Trong range tấn công: dừng, đánh
        if (state_ == SkeletonState::Chase) setState(SkeletonState::Attack);
        tickAttack(dt, playerPos);
      } else {
        // Seek player bằng velocity
        sf::Vector2f dir = seekMove(playerPos, SPEED, dt, ATTACK_RANGE_PX);
        flipX_ = (dir.x < 0.f);
      }
    }

    // Integrate velocity vào pos
    integrateVelocity(dt, /*damping=*/0.75f);
  }

  advanceAnim(dt);
}

void Skeleton::draw(sf::RenderTarget& target) const {
  if (dead_) return;

  sf::CircleShape shadow(HIT_RADIUS * 0.8f);
  shadow.setFillColor(sf::Color(0, 0, 0, 100));
  shadow.setScale({1.5f, 0.5f});
  shadow.setOrigin({HIT_RADIUS * 0.8f, HIT_RADIUS * 0.8f});
  shadow.setPosition({pos_.x, pos_.y + 50.f});
  target.draw(shadow);

  const sf::Texture& tex = currentTex();
  // TakeHit dùng texFlight_, giới hạn frameIdx trong FRAMES_FLIGHT
  int frameIdx =
      (state_ == SkeletonState::TakeHit) ? (frame_ % FRAMES_FLIGHT) : frame_;

  sf::Sprite sprite(tex);
  sprite.setTextureRect(sf::IntRect(sf::Vector2i(frameIdx * FRAME_W, 0),
                                    sf::Vector2i(FRAME_W, FRAME_H)));
  sprite.setOrigin({FRAME_W / 2.f, FRAME_H / 2.f});
  float sx = flipX_ ? -SCALE : SCALE;
  sprite.setScale({sx, SCALE});
  sprite.setPosition(pos_);

  if (state_ == SkeletonState::TakeHit) {
    sprite.setColor(hitFlash_ ? sf::Color(255, 80, 80)
                              : sf::Color(255, 160, 160));
  }
  target.draw(sprite);
}

void Skeleton::drawDebug(sf::RenderTarget& target) const {
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
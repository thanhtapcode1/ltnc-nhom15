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
       "C:\\Users\\LENOVO\\OneDrive\\Desktop\\project\\hinh "
       "anh\\Walksk.png"},
      {texAttack_,
       "C:\\Users\\LENOVO\\OneDrive\\Desktop\\project\\hinh "
       "anh\\Attacksk.png"},
      {texDeath_,
       "C:\\Users\\LENOVO\\OneDrive\\Desktop\\project\\hinh "
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

  const bool boss = isBoss();
  const float bossScale = boss ? 2.2f : 1.0f;
  const float finalScale = SCALE * bossScale;

  // ── Shadow ───────────────────────────────────────────────
  float shadowR = HIT_RADIUS * 0.8f * bossScale;
  sf::CircleShape shadow(shadowR);
  shadow.setFillColor(sf::Color(0, 0, 0, boss ? 150 : 100));
  shadow.setScale({1.5f, 0.5f});
  shadow.setOrigin({shadowR, shadowR});
  shadow.setPosition({pos_.x, pos_.y + 50.f * bossScale});
  target.draw(shadow);

  // ── Boss: viền ngoài nhấp nháy màu tím/đỏ ───────────────
  if (boss) {
    float pulse = std::abs(std::sin(timer_ * 4.f));
    uint8_t alpha = static_cast<uint8_t>(160 + 95 * pulse);
    float ringR = HIT_RADIUS * bossScale + 10.f;

    // Viền ngoài: đỏ thẫm
    sf::CircleShape ring2(ringR + 6.f);
    ring2.setFillColor(sf::Color::Transparent);
    ring2.setOutlineColor(
        sf::Color(180, 0, 255, static_cast<uint8_t>(alpha * 0.6f)));
    ring2.setOutlineThickness(4.f);
    ring2.setOrigin({ringR + 6.f, ringR + 6.f});
    ring2.setPosition(pos_);
    target.draw(ring2);

    // Viền trong: trắng/tím sáng
    sf::CircleShape ring(ringR);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineColor(sf::Color(220, 100, 255, alpha));
    ring.setOutlineThickness(3.f);
    ring.setOrigin({ringR, ringR});
    ring.setPosition(pos_);
    target.draw(ring);
  }

  // ── Sprite ───────────────────────────────────────────────
  const sf::Texture& tex = currentTex();
  int frameIdx =
      (state_ == SkeletonState::TakeHit) ? (frame_ % FRAMES_FLIGHT) : frame_;

  sf::Sprite sprite(tex);
  sprite.setTextureRect(sf::IntRect(sf::Vector2i(frameIdx * FRAME_W, 0),
                                    sf::Vector2i(FRAME_W, FRAME_H)));
  sprite.setOrigin({FRAME_W / 2.f, FRAME_H / 2.f});
  float sx = flipX_ ? -finalScale : finalScale;
  sprite.setScale({sx, finalScale});
  sprite.setPosition(pos_);

  if (state_ == SkeletonState::TakeHit) {
    sprite.setColor(hitFlash_ ? sf::Color(255, 80, 80)
                              : sf::Color(255, 160, 160));
  } else if (boss) {
    sprite.setColor(sf::Color(220, 180, 255));  // tint tím nhẹ
  }
  target.draw(sprite);

  // ── Boss HP bar lớn hơn ──────────────────────────────────
  if (boss) {
    float ratio = static_cast<float>(getHp()) / static_cast<float>(getMaxHp());
    const float barW = 80.f, barH = 8.f;
    sf::RectangleShape bg({barW, barH});
    bg.setFillColor(sf::Color(60, 0, 0, 200));
    bg.setOrigin({barW / 2.f, barH / 2.f});
    float barY = pos_.y - HIT_RADIUS * bossScale - 20.f;
    bg.setPosition({pos_.x, barY});
    target.draw(bg);
    sf::RectangleShape bar({barW * ratio, barH});
    bar.setFillColor(sf::Color(200, 60, 255, 220));
    bar.setOrigin({barW / 2.f, barH / 2.f});
    bar.setPosition({pos_.x, barY});
    target.draw(bar);
  }
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
#include "Bullet.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>

sf::Texture Bullet::texBullet_;
bool Bullet::texLoaded_ = false;

bool Bullet::loadTextures() {
  if (texLoaded_) return true;
  texLoaded_ = true;

  if (!texBullet_.loadFromFile("spr_wep_iron_long_5.png")) {
    std::cerr << "[Bullet] Missing: spr_wep_iron_long_5.png\n";
    return false;
  }
  return true;
}

Bullet::Bullet(sf::Vector2f pos, sf::Vector2f dir)
    : pos_(pos), vel_(dir * SPEED) {
  angle_ = std::atan2(dir.y, dir.x) * 180.f / 3.14159265f;
}

void Bullet::update(float dt) {
  if (dead_) return;

  if (hitting_) {
    hitTimer_ += dt;
    if (hitTimer_ >= HIT_FRAME_T) {
      hitTimer_ -= HIT_FRAME_T;
      ++hitFrame_;
      if (hitFrame_ >= HIT_FRAMES) dead_ = true;
    }
    return;
  }

  pos_ += vel_ * dt;
  age_ += dt;
  if (age_ >= LIFETIME) dead_ = true;
}

void Bullet::draw(sf::RenderTarget& target) const {
  if (dead_) return;

  if (hitting_) {
    if (hitFrame_ >= HIT_FRAMES) return;

    float t = static_cast<float>(hitFrame_) / static_cast<float>(HIT_FRAMES);
    float r = HIT_RADIUS_START + (HIT_RADIUS_END - HIT_RADIUS_START) * t;
    std::uint8_t alpha = static_cast<std::uint8_t>(255 * (1.f - t));

    // Vòng tròn ngoài (viền vàng)
    sf::CircleShape ring(r);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(3.f);
    ring.setOutlineColor(sf::Color(255, 220, 100, alpha));
    ring.setOrigin({r, r});
    ring.setPosition(pos_);

    // Lõi trắng nhỏ hơn
    float rInner = r * 0.5f;
    sf::CircleShape core(rInner);
    core.setFillColor(
        sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha * 0.6f)));
    core.setOrigin({rInner, rInner});
    core.setPosition(pos_);

    sf::RenderStates states;
    states.blendMode = sf::BlendAdd;
    target.draw(core, states);
    target.draw(ring, states);
    return;
  }

  sf::Sprite sprite(texBullet_);
  sprite.setTextureRect(sf::IntRect({0, 0}, {TEX_W, TEX_H}));
  sprite.setOrigin({TEX_W / 2.f, TEX_H / 2.f});
  sprite.setScale({SCALE, SCALE});
  sprite.setRotation(sf::degrees(angle_));
  sprite.setPosition(pos_);
  target.draw(sprite);
}

bool Bullet::hits(sf::Vector2f center, float r) const {
  if (dead_ || hitting_) return false;
  sf::Vector2f d = center - pos_;
  float minDist = RADIUS + r;
  return (d.x * d.x + d.y * d.y) < minDist * minDist;
}

void Bullet::kill() {
  if (hitting_) return;
  hitting_ = true;
  hitFrame_ = 0;
  hitTimer_ = 0.f;
  vel_ = {0.f, 0.f};
}
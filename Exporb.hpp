#pragma once
#include <SFML/Graphics.hpp>
#include <cmath>
#include <cstdint>
#include <iostream>

class ExpOrb {
 public:
  static constexpr float ATTRACT_R = 90.f;
  static constexpr float PICKUP_R = 16.f;
  static constexpr float ATTRACT_SPEED = 240.f;
  static constexpr float SCALE = 1.2f;
  static constexpr int TEX_SIZE = 16;

  static bool loadTexture() {
    if (texLoaded_) return true;
    texLoaded_ = true;
    if (!tex_.loadFromFile(
            "C:\\Users\\LENOVO\\OneDrive\\Desktop\\project\\hinh "
            "anh\\GemPurple.png")) {
      std::cerr << "[ExpOrb] Missing: GemPurple.png\n";
      return false;
    }
    // Mask nền đen -> transparent (ảnh RGB)
    sf::Image img = tex_.copyToImage();
    for (unsigned y = 0; y < img.getSize().y; ++y)
      for (unsigned x = 0; x < img.getSize().x; ++x) {
        sf::Color c = img.getPixel({x, y});
        if (c.r < 25 && c.g < 25 && c.b < 25)
          img.setPixel({x, y}, sf::Color::Transparent);
      }
    if (!tex_.loadFromImage(img)) {
      return false;
    }
    return true;
  }

  ExpOrb(sf::Vector2f pos, int value = 1)
      : pos_(pos), value_(value), bobBase_(pos.y) {}

  void update(float dt, sf::Vector2f playerPos) {
    if (collected_) return;
    bobT_ += dt * 3.f;

    sf::Vector2f diff = playerPos - pos_;
    float dist2 = diff.x * diff.x + diff.y * diff.y;

    if (dist2 < PICKUP_R * PICKUP_R) {
      collected_ = true;
      return;
    }
    if (dist2 < ATTRACT_R * ATTRACT_R) attracted_ = true;

    if (attracted_) {
      float dist = std::sqrt(dist2);
      float speed =
          ATTRACT_SPEED * (1.f + (ATTRACT_R - dist) / ATTRACT_R * 0.8f);
      pos_ += (diff / dist) * speed * dt;
    } else {
      pos_.y = bobBase_ + std::sin(bobT_) * 3.f;
    }
  }

  void draw(sf::RenderTarget& target) const {
    if (collected_) return;
    sf::Sprite spr(tex_);
    spr.setTextureRect(sf::IntRect({0, 0}, {TEX_SIZE, TEX_SIZE}));
    spr.setOrigin({TEX_SIZE / 2.f, TEX_SIZE / 2.f});
    spr.setScale({SCALE, SCALE});
    spr.setPosition(pos_);
    if (attracted_) {
      float pulse =
          static_cast<uint8_t>(200 + 55 * std::abs(std::sin(bobT_ * 8.f)));
      spr.setColor(sf::Color(255, 255, 255, static_cast<uint8_t>(pulse)));
    }
    target.draw(spr);
  }

  bool isCollected() const { return collected_; }
  int getValue() const { return value_; }

 private:
  sf::Vector2f pos_;
  int value_;
  bool collected_ = false;
  bool attracted_ = false;
  float bobT_ = 0.f;
  float bobBase_;

  static sf::Texture tex_;
  static bool texLoaded_;
};

inline sf::Texture ExpOrb::tex_;
inline bool ExpOrb::texLoaded_ = false;
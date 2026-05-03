#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <memory>
#include <string>

#include "CollisionMap.hpp"
#include "Constants.hpp"

enum class Direction { Down = 0, Left, Right, Up, COUNT };

class Player {
 public:
  static constexpr int FRAME_W = 96;
  static constexpr int FRAME_H = 80;
  static constexpr int NUM_FRAMES = 8;
  static constexpr float FRAME_TIME = 0.10f;
  static constexpr float SPEED = 200.f;
  static constexpr float SCALE = 2.2f;
  static constexpr float HIT_W = 12.f;
  static constexpr float HIT_H = 12.f;

  explicit Player(const CollisionMap& colMap);

  bool load();
  void update(float dt);
  void draw(sf::RenderTarget& target) const;
  void drawDebug(sf::RenderTarget& target) const;

  sf::Vector2f getPosition() const { return pos_; }
  void setPosition(sf::Vector2f p) { pos_ = p; }

  Direction getDirection() const { return dir_; }

  // Trả về vector đơn vị theo hướng player đang nhìn
  sf::Vector2f getFacingVector() const {
    switch (dir_) {
      case Direction::Up:
        return {0.f, -1.f};
      case Direction::Down:
        return {0.f, 1.f};
      case Direction::Left:
        return {-1.f, 0.f};
      case Direction::Right:
        return {1.f, 0.f};
      default:
        return {0.f, 1.f};
    }
  }
  sf::Vector2f getMoveVector() const { return moveVec_; }

 private:
  bool collidesAt(float cx, float cy) const;
  sf::Vector2f readInput();
  void advanceAnimation(float dt, bool isMoving);
  void syncSprite();

  const CollisionMap& colMap_;

  // Spawn giữa map - dùng TILE_RENDER_W/H (32) cho world coords
  sf::Vector2f pos_{Constants::MAP_WIDTH * Constants::TILE_RENDER_W / 2.f,
                    Constants::MAP_HEIGHT* Constants::TILE_RENDER_H / 2.f};

  Direction dir_ = Direction::Down;
  int frame_ = 0;
  float timer_ = 0.f;
  bool moving_ = false;

  static constexpr int TEX_COUNT = 8;
  std::array<sf::Texture, TEX_COUNT> textures_;
  std::unique_ptr<sf::Sprite> sprite_;

  int texIndex(Direction d, bool moving) const {
    return static_cast<int>(d) * 2 + (moving ? 0 : 1);
  }

  static const std::array<std::string, TEX_COUNT> TEX_FILES;
  sf::Vector2f moveVec_ = {};
};
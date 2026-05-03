#include "Player.hpp"

#include <cmath>
#include <iostream>

const std::array<std::string, Player::TEX_COUNT> Player::TEX_FILES = {
    "run_down.png",  "idle_down.png",  "run_left.png", "idle_left.png",
    "run_right.png", "idle_right.png", "run_up.png",   "idle_up.png",
};

Player::Player(const CollisionMap& colMap) : colMap_(colMap) {}

bool Player::load() {
  bool ok = true;
  for (int i = 0; i < TEX_COUNT; ++i) {
    if (!textures_[i].loadFromFile(TEX_FILES[i])) {
      std::cerr << "[Player] Missing: " << TEX_FILES[i] << '\n';
      ok = false;
    }
  }
  if (!ok) return false;

  sprite_ =
      std::make_unique<sf::Sprite>(textures_[texIndex(Direction::Down, false)]);
  sprite_->setScale({SCALE, SCALE});
  sprite_->setOrigin({FRAME_W / 2.f, FRAME_H / 2.f});
  return true;
}

bool Player::collidesAt(float cx, float cy) const {
  return colMap_.isSolidPixel(cx - HIT_W, cy - HIT_H) ||
         colMap_.isSolidPixel(cx + HIT_W, cy - HIT_H) ||
         colMap_.isSolidPixel(cx - HIT_W, cy + HIT_H) ||
         colMap_.isSolidPixel(cx + HIT_W, cy + HIT_H);
}

sf::Vector2f Player::readInput() {
  sf::Vector2f vel(0.f, 0.f);
  moving_ = false;

  if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left) ||
      sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
    vel.x -= 1.f;
    moving_ = true;
  }
  if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right) ||
      sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
    vel.x += 1.f;
    moving_ = true;
  }
  if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
      sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
    vel.y -= 1.f;
    moving_ = true;
  }
  if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down) ||
      sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
    vel.y += 1.f;
    moving_ = true;
  }

  // Luu vector di chuyen TRUOC khi normalize
  // (de KnifeSkill biet huong cheo)
  if (vel.x != 0.f || vel.y != 0.f)
    moveVec_ = vel;  // {-1,0}, {1,0}, {0,-1}, {0,1}, {-1,-1}, {1,1}, v.v.
  // Neu dung yen: giu nguyen moveVec_ huong cu

  // Cap nhat dir_ theo truc chinh de animation dung (uu tien X neu di cheo)
  if (vel.x < 0.f)
    dir_ = Direction::Left;
  else if (vel.x > 0.f)
    dir_ = Direction::Right;
  else if (vel.y < 0.f)
    dir_ = Direction::Up;
  else if (vel.y > 0.f)
    dir_ = Direction::Down;

  return vel;
}

void Player::advanceAnimation(float dt, bool isMoving) {
  timer_ += dt;
  if (timer_ >= FRAME_TIME) {
    timer_ -= FRAME_TIME;
    frame_ = (frame_ + 1) % NUM_FRAMES;
  }
  (void)isMoving;
}

void Player::syncSprite() {
  if (!sprite_) return;
  sprite_->setTexture(textures_[texIndex(dir_, moving_)]);
  sprite_->setTextureRect(sf::IntRect(sf::Vector2i(frame_ * FRAME_W, 0),
                                      sf::Vector2i(FRAME_W, FRAME_H)));
  sprite_->setScale({SCALE, SCALE});
  sprite_->setOrigin({FRAME_W / 2.f, FRAME_H / 2.f});
  sprite_->setPosition(pos_);
}

void Player::update(float dt) {
  if (!sprite_) return;

  sf::Vector2f vel = readInput();

  // Normalize khi di cheo de toc do bang nhau
  if (vel.x != 0.f && vel.y != 0.f) vel /= std::sqrt(2.f);

  float newX = pos_.x + vel.x * SPEED * dt;
  if (!collidesAt(newX, pos_.y)) pos_.x = newX;

  float newY = pos_.y + vel.y * SPEED * dt;
  if (!collidesAt(pos_.x, newY)) pos_.y = newY;

  const float maxX =
      static_cast<float>(Constants::MAP_WIDTH * Constants::TILE_RENDER_W) -
      HIT_W;
  const float maxY =
      static_cast<float>(Constants::MAP_HEIGHT * Constants::TILE_RENDER_H) -
      HIT_H;
  pos_.x = std::max(HIT_W, std::min(pos_.x, maxX));
  pos_.y = std::max(HIT_H, std::min(pos_.y, maxY));

  advanceAnimation(dt, moving_);
  syncSprite();
}

static void drawShadow(sf::RenderTarget& target, sf::Vector2f pos, float rx,
                       float ry, sf::Color color) {
  constexpr int LAYERS = 5;
  for (int i = LAYERS; i >= 1; --i) {
    float t = static_cast<float>(i) / LAYERS;
    sf::CircleShape c(rx * t);
    c.setScale({1.f, ry / rx});
    sf::Color col = color;
    col.a = static_cast<uint8_t>(color.a * (1.f - t * 0.7f));
    c.setFillColor(col);
    c.setOrigin({rx * t, rx * t});
    c.setPosition(pos);
    target.draw(c);
  }
}

void Player::draw(sf::RenderTarget& target) const {
  if (!sprite_) return;
  drawShadow(target, {pos_.x, pos_.y + FRAME_H * SCALE * 0.2f}, 18.f, 6.f,
             sf::Color(0, 0, 0, 130));
  target.draw(*sprite_);
}

void Player::drawDebug(sf::RenderTarget& target) const {
  sf::RectangleShape hb({HIT_W * 2.f, HIT_H * 2.f});
  hb.setFillColor(sf::Color(255, 0, 0, 80));
  hb.setOutlineColor(sf::Color::Red);
  hb.setOutlineThickness(0.5f);
  hb.setOrigin({HIT_W, HIT_H});
  hb.setPosition(pos_);
  target.draw(hb);

  sf::CircleShape dot(1.5f);
  dot.setFillColor(sf::Color::Yellow);
  dot.setOrigin({1.5f, 1.5f});
  dot.setPosition(pos_);
  target.draw(dot);
}
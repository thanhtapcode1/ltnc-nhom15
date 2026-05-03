#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

#include "ExpOrb.hpp"

class ExpManager {
 public:
  static int expRequired(int level) { return level * 10; }
  ExpManager() { ExpOrb::loadTexture(); }

  void spawnOrb(sf::Vector2f pos, int value = 1);
  int update(float dt, sf::Vector2f playerPos);  // trả về EXP nhặt được
  void draw(sf::RenderTarget& target) const;

  // Thêm EXP, trả về true nếu level up
  bool addExp(int amount);
  int getLevel() const { return level_; }
  int getExp() const { return currentExp_; }
  int getExpReq() const { return expRequired(level_); }
  int getTotalExp() const { return totalExp_; }

 private:
  std::vector<ExpOrb> orbs_;
  int level_ = 1;
  int currentExp_ = 0;
  int totalExp_ = 0;
};
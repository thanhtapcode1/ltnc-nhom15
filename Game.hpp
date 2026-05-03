#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <string>

#include "BulletManager.hpp"
#include "Camera.hpp"
#include "CharacterClass.hpp"  // ★ thêm mới
#include "CollisionMap.hpp"
#include "ExpManager.hpp"
#include "MenuSystem.hpp"
#include "MonsterManager.hpp"
#include "Player.hpp"
#include "PlayerStats.hpp"
#include "SkillManager.hpp"
#include "TileMap.hpp"

enum class UpgradeType {
  Damage,
  AttackSpeed,
  Knife,
  LightningRing,
  Garlic,
};

struct UpgradeOption {
  UpgradeType type;
  std::string title;
  std::string desc;
  sf::Color color = sf::Color::White;
};

class Game {
 public:
  Game();
  ~Game() = default;
  void run();

 private:
  void processEvents(const sf::Event& event);
  void update(float dt);
  void render();
  void renderHUD();
  void renderUpgradeScreen();

  void buildUpgradeOptions();
  void applyUpgrade(UpgradeType t);
  void updateHover(sf::Vector2i mousePixel);
  sf::Vector2f mouseToWorld() const;

  // ★ Áp stat + skill khởi đầu theo class được chọn
  void applyCharacterClass(int charIndex);

  sf::RenderWindow window_;
  CollisionMap colMap_;
  TileMap tileMap_;
  Player player_;
  Camera camera_;
  sf::Clock clock_;

  MonsterManager monsters_;
  BulletManager bullets_;
  ExpManager expManager_;
  PlayerStats stats_;
  SkillManager skillMgr_;

  // ★ Tốc độ player (có thể thay đổi theo class)
  float playerSpeed_ = Player::SPEED;

  bool paused_ = false;
  int hoveredCard_ = -1;
  std::array<UpgradeOption, 3> upgradeOptions_;
  float levelUpTimer_ = 0.f;
  int lastLevel_ = 1;

  sf::Font font_;
  bool fontLoaded_ = false;

  MenuSystem menu_{window_, font_};
  bool inMenu_ = true;
  int selectedChar_ = 0;

  static constexpr float MAX_DT = 0.05f;
  bool debugMode_ = false;
};
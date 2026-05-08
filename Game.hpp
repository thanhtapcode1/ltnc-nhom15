#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <string>

#include "BulletManager.hpp"
#include "Camera.hpp"
#include "CharacterClass.hpp"
#include "CollisionMap.hpp"
#include "ExpManager.hpp"
#include "MenuSystem.hpp"
#include "MonsterManager.hpp"
#include "Player.hpp"
#include "PlayerStats.hpp"
#include "SaveSystem.hpp"
#include "ScoreSystem.hpp"
#include "SkillManager.hpp"
#include "SoundManager.hpp"  // ← âm thanh
#include "TileMap.hpp"
#include "WaveManager.hpp"  // ← thêm mới
#include "dokho.hpp"

enum class UpgradeType { Damage, AttackSpeed, Knife, LightningRing, Garlic };

struct UpgradeOption {
  UpgradeType type;
  std::string title;
  std::string desc;
  sf::Color color = sf::Color::White;
};

enum class GameState { Menu, Playing, GameOver };

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
  void renderGameOver();

  void buildUpgradeOptions();
  void applyUpgrade(UpgradeType t);
  void updateHover(sf::Vector2i mousePixel);
  sf::Vector2f mouseToWorld() const;

  void applyCharacterClass(int charIndex);
  void applyDifficulty();
  void endGame();
  void restartGame();

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
  WaveManager waveMgr_;  // ← thêm mới

  ScoreSystem score_;
  SaveData saveData_;
  Difficulty difficulty_ = Difficulty::Easy;

  GameState gameState_ = GameState::Menu;
  bool paused_ = false;
  int hoveredCard_ = -1;
  int selectedChar_ = 0;
  float playerSpeed_ = Player::SPEED;

  std::array<UpgradeOption, 3> upgradeOptions_;
  float levelUpTimer_ = 0.f;

  // ── HUD message (wave / boss / map event) ───────────────
  std::string hudMessage_;
  float hudMessageTimer_ = 0.f;

  sf::Font font_;
  bool fontLoaded_ = false;
  MenuSystem menu_{window_, font_};

  static constexpr float MAX_DT = 0.05f;
  bool debugMode_ = false;
  bool pauseMenuOpen_ = false;
  bool settingsOpen_ = false;

  void renderPauseMenu();
  void handlePauseMenuClick(sf::Vector2f mouseUI);
};
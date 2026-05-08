#include "Game.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

#include "CharacterClass.hpp"
#include "FlyEye.hpp"
#include "IMonster.hpp"
#include "MapData.hpp"
#include "Skeleton.hpp"
#include "Slime.hpp"

using namespace Constants;

// ════════════════════════════════════════════════════════════
//  Constructor
// ════════════════════════════════════════════════════════════
Game::Game()
    : window_(sf::VideoMode({WIN_W, WIN_H}), "Vampire Survivors Clone",
              sf::Style::Default),
      colMap_(),
      tileMap_(),
      player_(colMap_),
      camera_(window_) {
  std::srand(static_cast<unsigned>(std::time(nullptr)));
  window_.setFramerateLimit(60);

  for (int i = 0; i < MapData::TILESET_COUNT; ++i)
    tileMap_.addTileset(MapData::TILESETS[i].filename,
                        MapData::TILESETS[i].firstGid);
  const uint32_t* layerPtrs[] = {MapData::LAYER_FLOOR, MapData::LAYER_CO};
  for (int i = 0; i < MapData::LAYER_COUNT; ++i)
    tileMap_.addLayer(MapData::LAYER_NAMES[i], layerPtrs[i],
                      MapData::LAYER_SIZE);
  tileMap_.loadTilesets();

  if (!player_.load()) std::cerr << "[Game] Player textures missing!\n";

  FlyEye::loadTextures();
  Skeleton::loadTextures();

  monsters_.registerFactory(
      "flyeye", [](sf::Vector2f p) { return std::make_unique<FlyEye>(p); });
  monsters_.registerFactory(
      "slime", [](sf::Vector2f p) { return std::make_unique<Slime>(p); });
  monsters_.registerFactory(
      "skeleton", [](sf::Vector2f p) { return std::make_unique<Skeleton>(p); });

  for (auto* path :
       {"C:/Windows/Fonts/seguisym.ttf", "arial.ttf",
        "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/consola.ttf"}) {
    if (font_.openFromFile(path)) {
      fontLoaded_ = true;
      break;
    }
  }

  try {
    saveData_ = SaveSystem::load();
  } catch (const std::runtime_error& e) {
    std::cerr << "[Game] " << e.what() << "\n"
              << "[Game] Reset ve save mac dinh.\n";
    saveData_ = SaveData{};
  }

  menu_.init();

  // ── Âm thanh ─────────────────────────────────────────────
  SoundManager::get().init();
  SoundManager::get().playMusic(SoundManager::BGM::MENU_BGM);

  std::cout << "=== Vampire Survivors Clone ===\n"
            << "WASD/Arrow: di chuyen  |  F1: debug  |  ESC: thoat\n\n";
}

// ════════════════════════════════════════════════════════════
//  applyCharacterClass
// ════════════════════════════════════════════════════════════
void Game::applyCharacterClass(int idx) {
  if (idx < 0 || idx > 2) idx = 0;
  const CharClassDef& def = CharacterClass::DEFS[idx];

  const DifficultyConfig& cfg = DifficultyConfig::get(difficulty_);
  stats_.maxHp = cfg.playerStartHp + def.bonusHp;
  stats_.hp = stats_.maxHp;
  stats_.damage = 1 + def.bonusDamage;
  playerSpeed_ = Player::SPEED + def.bonusSpeed;

  switch (def.startSkill) {
    case StartingSkill::Knife:
      skillMgr_.applyUpgrade(SkillUpgradeType::Knife);
      break;
    case StartingSkill::Lightning:
      skillMgr_.applyUpgrade(SkillUpgradeType::LightningRing);
      break;
    case StartingSkill::Garlic:
      skillMgr_.applyUpgrade(SkillUpgradeType::Garlic);
      break;
  }

  std::cout << "[Game] Class=" << def.name
            << " Difficulty=" << DifficultyConfig::get(difficulty_).name
            << " HP=" << stats_.maxHp << " DMG=" << stats_.damage << "\n";
}

// ════════════════════════════════════════════════════════════
//  applyDifficulty
//  NOTE: không còn setSpawnWeight — tỉ lệ quái do WaveManager
//        điều khiển qua minCount trong từng WaveDef.
//        Hard mode có thể mở rộng bằng cách chỉnh WaveManager
//        hoặc giảm playerStartHp, tăng monsterDamage ở đây.
// ════════════════════════════════════════════════════════════
void Game::applyDifficulty() {
  // Độ khó ảnh hưởng HP/DMG player (qua DifficultyConfig)
  // Nếu muốn Hard spawn nhiều quái hơn, chỉnh minCount trong
  // WaveManager::buildWaves() theo difficulty_.
  score_.difficulty = difficulty_;
}

// ════════════════════════════════════════════════════════════
//  endGame
// ════════════════════════════════════════════════════════════
void Game::endGame() {
  gameState_ = GameState::GameOver;

  SoundManager::get().play(SoundManager::SFX::PLAYER_DIE);
  SoundManager::get().playMusic(SoundManager::BGM::GAMEOVER_BGM, false);

  saveData_.lastScore = score_.score;
  saveData_.lastCharIndex = selectedChar_;
  saveData_.lastDifficulty =
      (difficulty_ == Difficulty::Hard) ? "Hard" : "Easy";
  saveData_.lastKills = score_.kills;
  saveData_.lastTimeAlive = score_.timeAlive;

  SaveSystem::updateHighScore(saveData_, score_.score, difficulty_);

  try {
    SaveSystem::save(saveData_);
  } catch (const std::runtime_error& e) {
    std::cerr << "[Game] Loi luu file: " << e.what() << "\n";
  }

  std::cout << "[Game] Game Over. Score=" << score_.score
            << " Kills=" << score_.kills << " Time=" << score_.formatTime()
            << "\n";
}

// ════════════════════════════════════════════════════════════
//  restartGame
// ════════════════════════════════════════════════════════════
void Game::restartGame() {
  upgradeOptions_ = {};
  score_.reset();
  skillMgr_ = SkillManager{};
  expManager_ = ExpManager{};
  bullets_ = BulletManager{};
  monsters_ = MonsterManager{};
  stats_ = PlayerStats{};
  waveMgr_ = WaveManager{};
  paused_ = false;
  pauseMenuOpen_ = false;
  settingsOpen_ = false;
  hoveredCard_ = -1;
  levelUpTimer_ = 0.f;
  hudMessage_.clear();
  hudMessageTimer_ = 0.f;

  // Đăng ký lại factory (bị reset theo monsters_)
  monsters_.registerFactory(
      "flyeye", [](sf::Vector2f p) { return std::make_unique<FlyEye>(p); });
  monsters_.registerFactory(
      "slime", [](sf::Vector2f p) { return std::make_unique<Slime>(p); });
  monsters_.registerFactory(
      "skeleton", [](sf::Vector2f p) { return std::make_unique<Skeleton>(p); });

  player_.setPosition(
      {MAP_WIDTH * TILE_RENDER_W / 2.f, MAP_HEIGHT * TILE_RENDER_H / 2.f});
  camera_.reset();

  applyDifficulty();
  applyCharacterClass(selectedChar_);

  // Khởi tạo WaveManager (phải sau registerFactory)
  waveMgr_.init(monsters_);
  monsters_.spawnInitial(camera_, 3);

  SoundManager::get().playMusic(SoundManager::BGM::GAME_BGM);
  gameState_ = GameState::Playing;
}

// ════════════════════════════════════════════════════════════
//  run
// ══════════════════════════════════════════════s══════════════
void Game::run() {
  while (window_.isOpen()) {
    float dt = clock_.restart().asSeconds();
    if (dt > MAX_DT) dt = MAX_DT;

    // Tick SoundManager mỗi frame bất kể state
    // để phát hiện non-loop music (GAMEOVER_BGM) tự hết
    // và reset musicOpened_ TRƯỚC khi stopAll/playMusic chạy.
    SoundManager::get().tick();

    while (const auto event = window_.pollEvent()) {
      if (event->is<sf::Event::Closed>()) {
        window_.close();
        return;
      }

      if (gameState_ == GameState::Menu)
        menu_.handleEvent(*event);
      else
        processEvents(*event);
    }

    if (gameState_ == GameState::Menu) {
      menu_.update(dt);
      window_.clear();
      menu_.render();
      window_.display();

      if (menu_.isDone()) {
        auto res = menu_.getResult();
        if (res.quit) return;
        selectedChar_ = res.charIndex;
        difficulty_ = res.difficulty;
        restartGame();
        window_.setView(camera_.getView());
        gameState_ = GameState::Playing;
      }
      continue;
    }

    if (gameState_ == GameState::Playing) update(dt);
    render();
  }
}

// ════════════════════════════════════════════════════════════
//  update
// ════════════════════════════════════════════════════════════
void Game::update(float dt) {
  if (paused_) return;

  score_.update(dt);
  player_.update(dt);

  sf::Vector2f pPos = player_.getPosition();
  sf::Vector2f facing = player_.getFacingVector();
  sf::Vector2f moveDir = player_.getMoveVector();

  // ── Lấy danh sách quái còn sống ──────────────────────────
  auto liveMonsters = monsters_.getLiveMonsters();
  std::vector<std::pair<sf::Vector2f, void*>> monsterData;
  monsterData.reserve(liveMonsters.size());
  for (auto* m : liveMonsters)
    monsterData.push_back({m->getPosition(), static_cast<void*>(m)});

  // ── Skills ───────────────────────────────────────────────
  auto skillResult =
      skillMgr_.update(pPos, facing, moveDir, dt, stats_.damage, monsterData);

  bullets_.spawnFromShots(skillResult.shots);
  if (!skillResult.shots.empty())
    SoundManager::get().play(SoundManager::SFX::SHOOT);

  for (auto& zap : skillResult.zaps) {
    auto* m = static_cast<IMonster*>(zap.monster);
    if (!m || !m->isAlive()) continue;
    m->takeHit(zap.damage);
    SoundManager::get().play(SoundManager::SFX::LIGHTNING);
    if (!m->isAlive()) {
      expManager_.spawnOrb(zap.pos, m->getExpValue());
      score_.addKill(m->getTypeId());
      SoundManager::get().playVaried(SoundManager::SFX::MONSTER_DIE);
    }
  }

  for (auto& hit : skillResult.garlic) {
    auto* m = static_cast<IMonster*>(hit.monster);
    if (!m || !m->isAlive()) continue;
    m->applyKnockback(hit.knockbackDir, hit.knockbackForce);
    m->takeHit(hit.damage);
    SoundManager::get().play(SoundManager::SFX::GARLIC_TICK);
    if (!m->isAlive()) {
      expManager_.spawnOrb(hit.monsterPos, m->getExpValue());
      score_.addKill(m->getTypeId());
      SoundManager::get().playVaried(SoundManager::SFX::MONSTER_DIE);
      if (skillMgr_.garlicCanHeal())
        stats_.hp = std::min(stats_.hp + 1, stats_.maxHp);
    }
  }

  auto bulletKills = bullets_.update(dt, liveMonsters, stats_.damage);
  for (auto& k : bulletKills) {
    expManager_.spawnOrb(k.pos, k.expValue);
    score_.addKill(k.typeId);
    SoundManager::get().play(SoundManager::SFX::HIT_MONSTER);
    SoundManager::get().playVaried(SoundManager::SFX::MONSTER_DIE);
  }

  // ── WaveManager: chuyển wave / boss / map event ───────────
  waveMgr_.update(dt, pPos, camera_, monsters_);
  if (auto msg = waveMgr_.popMessage()) {
    hudMessage_ = *msg;
    hudMessageTimer_ = 3.0f;
    // Phát SFX boss khi thông báo chứa "BOSS"
    if (hudMessage_.find("BOSS") != std::string::npos)
      SoundManager::get().play(SoundManager::SFX::BOSS_APPEAR);
  }

  // ── MonsterManager: quota spawn + despawn + AI ───────────
  auto monsterKills = monsters_.update(dt, pPos, camera_);
  for (auto& k : monsterKills) {
    expManager_.spawnOrb(k.pos, k.expValue);
    score_.addKill(k.typeId);
    if (k.isBoss) {
      SoundManager::get().play(SoundManager::SFX::BOSS_APPEAR);
      hudMessage_ = "Treasure Chest dropped!";
      hudMessageTimer_ = 3.0f;
    }
  }

  // Damage từ quái → player
  int monsterDmg = monsters_.collectPendingDamage();
  if (monsterDmg > 0) {
    stats_.takeDamage(monsterDmg);
    SoundManager::get().play(SoundManager::SFX::PLAYER_HIT);
  }

  if (stats_.isDead()) {
    endGame();
    return;
  }

  // ── EXP & Level up ───────────────────────────────────────
  int gained = expManager_.update(dt, pPos);
  if (gained > 0) {
    SoundManager::get().play(SoundManager::SFX::PICKUP_EXP);
    bool leveledUp = expManager_.addExp(gained);
    if (leveledUp) {
      score_.addLevelUp(expManager_.getLevel());
      levelUpTimer_ = 2.5f;
      paused_ = true;
      buildUpgradeOptions();
      SoundManager::get().play(SoundManager::SFX::LEVEL_UP);
    }
  }
  if (levelUpTimer_ > 0.f) levelUpTimer_ -= dt;
  if (hudMessageTimer_ > 0.f) hudMessageTimer_ -= dt;

  camera_.update(dt, pPos);
}

// ════════════════════════════════════════════════════════════
//  processEvents
// ════════════════════════════════════════════════════════════
void Game::processEvents(const sf::Event& event) {
  sf::View uiView(sf::FloatRect(
      {0.f, 0.f}, {static_cast<float>(WIN_W), static_cast<float>(WIN_H)}));
  sf::Vector2f mouseUI =
      window_.mapPixelToCoords(sf::Mouse::getPosition(window_), uiView);
  updateHover(
      sf::Vector2i(static_cast<int>(mouseUI.x), static_cast<int>(mouseUI.y)));

  if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
    switch (key->code) {
      case sf::Keyboard::Key::Escape:
        if (gameState_ == GameState::Playing) {
          if (paused_ && pauseMenuOpen_) {
            // Đóng pause menu, tiếp tục chơi
            pauseMenuOpen_ = false;
            paused_ = false;
          } else if (!paused_) {
            // Mở pause menu
            pauseMenuOpen_ = true;
            paused_ = true;
          }
        }
        break;
      case sf::Keyboard::Key::F1:
        debugMode_ = !debugMode_;
        break;
      case sf::Keyboard::Key::Num1:
        if (paused_ && upgradeOptions_.size() > 0)
          applyUpgrade(upgradeOptions_[0].type);
        break;
      case sf::Keyboard::Key::Num2:
        if (paused_ && upgradeOptions_.size() > 1)
          applyUpgrade(upgradeOptions_[1].type);
        break;
      case sf::Keyboard::Key::Num3:
        if (paused_ && upgradeOptions_.size() > 2)
          applyUpgrade(upgradeOptions_[2].type);
        break;
      case sf::Keyboard::Key::R:
        if (gameState_ == GameState::GameOver) restartGame();
        break;
      case sf::Keyboard::Key::M:
        if (gameState_ == GameState::GameOver) {
          gameState_ = GameState::Menu;
          window_.setView(window_.getDefaultView());
          SoundManager::get().stopAll();
          SoundManager::get().playMusic(SoundManager::BGM::MENU_BGM);
          menu_.init();
        }
        break;
      default:
        break;
    }
  }

  if (paused_ && gameState_ == GameState::Playing) {
    if (const auto* click = event.getIf<sf::Event::MouseButtonPressed>()) {
      if (click->button == sf::Mouse::Button::Left) {
        if (pauseMenuOpen_) {
          handlePauseMenuClick(mouseUI);
        } else if (hoveredCard_ >= 0) {
          applyUpgrade(upgradeOptions_[hoveredCard_].type);
        }
      }
    }
  }

  camera_.handleEvent(event);
}

// ════════════════════════════════════════════════════════════
//  render
// ════════════════════════════════════════════════════════════
void Game::render() {
  window_.setView(camera_.getView());
  window_.clear(sf::Color(20, 20, 30));

  tileMap_.renderLayers(window_, camera_.getView(), 0, 1);
  tileMap_.renderLayers(window_, camera_.getView(), 1, 2);

  expManager_.draw(window_);
  bullets_.draw(window_);
  monsters_.draw(window_);
  skillMgr_.drawEffects(window_, player_.getPosition());
  player_.draw(window_);

  if (debugMode_) {
    player_.drawDebug(window_);
    monsters_.drawDebug(window_);
    skillMgr_.drawDebugRanges(window_, player_.getPosition());
  }

  renderHUD();
  if (paused_ && gameState_ == GameState::Playing) {
    if (pauseMenuOpen_)
      renderPauseMenu();
    else
      renderUpgradeScreen();
  }
  if (gameState_ == GameState::GameOver) renderGameOver();

  window_.display();
}

// ════════════════════════════════════════════════════════════
//  renderHUD
// ════════════════════════════════════════════════════════════
static void drawBar(sf::RenderWindow& win, sf::Vector2f pos, sf::Vector2f size,
                    float ratio, sf::Color bg, sf::Color fill, sf::Color glow) {
  sf::RectangleShape glowBox(size + sf::Vector2f(4.f, 4.f));
  glowBox.setFillColor(sf::Color::Transparent);
  glowBox.setOutlineThickness(2.f);
  glowBox.setOutlineColor(sf::Color(glow.r, glow.g, glow.b, 80));
  glowBox.setPosition(pos - sf::Vector2f(2.f, 2.f));
  win.draw(glowBox);

  sf::RectangleShape bgBox(size);
  bgBox.setFillColor(bg);
  bgBox.setPosition(pos);
  win.draw(bgBox);

  if (ratio > 0.f) {
    float w = size.x * std::min(ratio, 1.f);
    sf::RectangleShape fillBox({w, size.y});
    fillBox.setFillColor(fill);
    fillBox.setPosition(pos);
    win.draw(fillBox);
  }
}

void Game::renderHUD() {
  sf::View uiView(sf::FloatRect(
      {0.f, 0.f}, {static_cast<float>(WIN_W), static_cast<float>(WIN_H)}));
  window_.setView(uiView);

  const float mg = 14.f, bW = 280.f, bH = 18.f, gap = 8.f;
  const float hpY = mg, expY = hpY + bH + gap;

  float hpR = static_cast<float>(stats_.hp) / static_cast<float>(stats_.maxHp);
  sf::Color hpCol = (hpR > 0.5f)    ? sf::Color(50, 220, 80)
                    : (hpR > 0.25f) ? sf::Color(240, 190, 30)
                                    : sf::Color(220, 40, 40);
  drawBar(window_, {mg, hpY}, {bW, bH}, hpR, sf::Color(20, 5, 5, 210), hpCol,
          hpCol);

  float expR = static_cast<float>(expManager_.getExp()) /
               static_cast<float>(expManager_.getExpReq());
  drawBar(window_, {mg, expY}, {bW, bH}, expR, sf::Color(8, 5, 20, 210),
          sf::Color(140, 70, 255), sf::Color(170, 110, 255));

  if (!fontLoaded_) {
    window_.setView(camera_.getView());
    return;
  }

  // HP text
  {
    std::ostringstream ss;
    ss << "HP " << stats_.hp << "/" << stats_.maxHp;
    sf::Text t(font_, ss.str(), 11);
    t.setFillColor(sf::Color(240, 240, 240));
    auto b = t.getLocalBounds();
    t.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    t.setPosition({mg + bW / 2.f, hpY + bH / 2.f});
    window_.draw(t);
  }

  // EXP text
  {
    std::ostringstream ss;
    ss << "Lv" << expManager_.getLevel() << "  " << expManager_.getExp() << "/"
       << expManager_.getExpReq();
    sf::Text t(font_, ss.str(), 11);
    t.setFillColor(sf::Color(210, 190, 255));
    auto b = t.getLocalBounds();
    t.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    t.setPosition({mg + bW / 2.f, expY + bH / 2.f});
    window_.draw(t);
  }

  // Score panel (góc trên phải)
  {
    const float panelW = 180.f, panelH = 85.f;
    float posX = static_cast<float>(WIN_W) - mg - panelW;
    float posY = mg;

    sf::RectangleShape bg({panelW, panelH});
    bg.setFillColor(sf::Color(0, 0, 0, 160));
    bg.setOutlineColor(sf::Color(255, 255, 255, 40));
    bg.setOutlineThickness(1.f);
    bg.setPosition({posX, posY});
    window_.draw(bg);

    auto drawRichText = [&](const std::string& s, unsigned sz, sf::Vector2f p,
                            sf::Color c, bool alignRight = true) {
      sf::Text t(font_, s, sz);
      auto b = t.getLocalBounds();
      if (alignRight) t.setOrigin({b.size.x, 0.f});
      t.setFillColor(sf::Color(0, 0, 0, 200));
      t.setPosition({p.x + 1.f, p.y + 1.f});
      window_.draw(t);
      t.setFillColor(c);
      t.setPosition(p);
      window_.draw(t);
    };

    float textRightX = posX + panelW - 10.f;
    std::ostringstream ss;
    ss << score_.score;
    drawRichText("SCORE", 11, {posX + 10.f, posY + 8.f},
                 sf::Color(200, 200, 200), false);
    drawRichText(ss.str(), 24, {textRightX, posY + 2.f},
                 sf::Color(255, 215, 0));
    drawRichText("TIME", 10, {posX + 10.f, posY + 38.f},
                 sf::Color(180, 180, 180), false);
    drawRichText(score_.formatTime(), 16, {textRightX, posY + 32.f},
                 sf::Color(230, 230, 230));
    int hi = (difficulty_ == Difficulty::Hard) ? saveData_.highScoreHard
                                               : saveData_.highScoreEasy;
    drawRichText("BEST", 10, {posX + 10.f, posY + 62.f},
                 sf::Color(100, 200, 255, 180), false);
    drawRichText(std::to_string(hi), 14, {textRightX, posY + 58.f},
                 sf::Color(100, 200, 255));
  }

  // Stats panel (góc trên trái, dưới bars)
  {
    float panelY = expY + bH + 8.f;
    sf::RectangleShape panel({bW, 60.f});
    panel.setFillColor(sf::Color(0, 0, 0, 150));
    panel.setOutlineColor(sf::Color(255, 200, 50, 100));
    panel.setOutlineThickness(1.f);
    panel.setPosition({mg, panelY});
    window_.draw(panel);

    auto put = [&](const std::string& s, sf::Color c, float dy) {
      sf::Text t(font_, s, 11);
      t.setFillColor(c);
      t.setPosition({mg + 6.f, panelY + dy});
      window_.draw(t);
    };

    std::ostringstream kills, skillLine;
    kills << "Kills: " << score_.kills
          << "   Alive: " << monsters_.aliveCount();

    if (skillMgr_.hasKnife()) {
      skillLine << "Knife Lv" << skillMgr_.getKnifeLevel();
      if (skillMgr_.knifeEvolved()) skillLine << "[EVO]";
    }
    if (skillMgr_.hasLightning()) {
      if (!skillLine.str().empty()) skillLine << "  ";
      skillLine << "Lightning Lv" << skillMgr_.getLightningLevel();
      if (skillMgr_.lightningEvolved()) skillLine << "[EVO]";
    }
    if (skillMgr_.hasGarlic()) {
      if (!skillLine.str().empty()) skillLine << "  ";
      skillLine << "Garlic Lv" << skillMgr_.getGarlicLevel();
      if (skillMgr_.garlicEvolved()) skillLine << "[EVO]";
    }

    const DifficultyConfig& cfg = DifficultyConfig::get(difficulty_);
    sf::Color diffCol = (difficulty_ == Difficulty::Hard)
                            ? sf::Color(255, 80, 80)
                            : sf::Color(80, 220, 80);
    put("[" + cfg.name + "]  DMG:" + std::to_string(stats_.damage), diffCol,
        6.f);
    put(kills.str(), sf::Color(200, 200, 200), 22.f);
    put(skillLine.str(), sf::Color(255, 230, 80), 38.f);
  }

  // Wave name
  {
    float panelY = expY + bH + 8.f + 60.f + 6.f;
    sf::Text wt(font_, "Wave: " + waveMgr_.currentWaveName(), 11);
    wt.setFillColor(sf::Color(150, 200, 255, 200));
    wt.setPosition({mg, panelY});
    window_.draw(wt);
  }

  // ── HUD Message (wave alert / boss / map event) ───────────
  if (hudMessageTimer_ > 0.f) {
    float alpha = std::min(hudMessageTimer_ / 0.5f, 1.f) * 255.f;
    auto a = static_cast<uint8_t>(alpha);
    auto a2 = static_cast<uint8_t>(alpha * 0.6f);

    sf::Text shadow(font_, hudMessage_, 22);
    shadow.setFillColor(sf::Color(0, 0, 0, a2));
    auto b = shadow.getLocalBounds();
    shadow.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    shadow.setPosition({WIN_W / 2.f + 2.f, 82.f});
    window_.draw(shadow);

    sf::Text msg(font_, hudMessage_, 22);
    msg.setFillColor(sf::Color(255, 220, 60, a));
    b = msg.getLocalBounds();
    msg.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    msg.setPosition({WIN_W / 2.f, 80.f});
    window_.draw(msg);
  }

  window_.setView(camera_.getView());
}

// ════════════════════════════════════════════════════════════
//  renderGameOver
// ════════════════════════════════════════════════════════════
void Game::renderGameOver() {
  sf::View uiView(sf::FloatRect(
      {0.f, 0.f}, {static_cast<float>(WIN_W), static_cast<float>(WIN_H)}));
  window_.setView(uiView);

  sf::RectangleShape overlay({(float)WIN_W, (float)WIN_H});
  overlay.setFillColor(sf::Color(10, 5, 15, 220));
  window_.draw(overlay);

  if (!fontLoaded_) return;

  float cx = WIN_W / 2.f, cy = WIN_H / 2.f;

  auto drawText = [&](const std::string& s, unsigned sz, float x, float y,
                      sf::Color c, bool center = true) {
    sf::Text t(font_, s, sz);
    auto b = t.getLocalBounds();
    if (center) t.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    t.setFillColor(sf::Color(0, 0, 0, 150));
    t.setPosition({x + 2.f, y + 2.f});
    window_.draw(t);
    t.setFillColor(c);
    t.setPosition({x, y});
    window_.draw(t);
  };

  drawText("G A M E   O V E R", 60, cx, cy - 200.f, sf::Color(255, 70, 70));

  float panW = 440.f, panH = 260.f;
  sf::RectangleShape panel({panW, panH});
  panel.setFillColor(sf::Color(30, 25, 45, 240));
  panel.setOutlineColor(sf::Color(120, 100, 200, 200));
  panel.setOutlineThickness(3.f);
  panel.setOrigin({panW / 2.f, panH / 2.f});
  panel.setPosition({cx, cy - 20.f});
  window_.draw(panel);

  const DifficultyConfig& cfg = DifficultyConfig::get(difficulty_);
  int hiScore = (difficulty_ == Difficulty::Hard) ? saveData_.highScoreHard
                                                  : saveData_.highScoreEasy;

  float startX = cx - 180.f;
  float startY = cy - 110.f;

  drawText(cfg.name + " MODE", 14, cx, startY, sf::Color(150, 150, 150));

  auto drawStat = [&](const std::string& key, const std::string& val, float y,
                      sf::Color valCol) {
    drawText(key, 15, startX, y, sf::Color(180, 180, 180), false);
    sf::Text tmp(font_, val, 16);
    float valX = cx + 180.f - tmp.getLocalBounds().size.x;
    drawText(val, 16, valX, y, valCol, false);
  };

  drawStat("CHARACTER", CharacterClass::DEFS[selectedChar_].name, startY + 40.f,
           sf::Color(150, 200, 255));
  drawStat("SCORE", std::to_string(score_.score), startY + 75.f,
           sf::Color(255, 230, 60));
  drawStat("BEST", std::to_string(hiScore), startY + 105.f,
           sf::Color(100, 200, 255));
  drawStat("KILLS", std::to_string(score_.kills), startY + 135.f,
           sf::Color(255, 130, 100));
  drawStat("SURVIVED", score_.formatTime(), startY + 165.f,
           sf::Color(150, 255, 150));

  if (score_.score >= hiScore && score_.score > 0) {
    static sf::Clock flashClock;
    float flash =
        std::abs(std::sin(flashClock.getElapsedTime().asSeconds() * 5.f));
    drawText(" NEW RECORD ", 20, cx, cy + 90.f,
             sf::Color(255, 255, 0, static_cast<uint8_t>(150 + 105 * flash)));
  }

  drawText("[R] RESTART      [M] MAIN MENU", 16, cx, cy + 180.f,
           sf::Color(130, 130, 130));

  window_.setView(camera_.getView());
}

// ════════════════════════════════════════════════════════════
//  Upgrade system
// ════════════════════════════════════════════════════════════
void Game::buildUpgradeOptions() {
  std::vector<UpgradeOption> pool;

  pool.push_back({UpgradeType::Damage, "TANG DAME", "Tang sat thuong +1",
                  sf::Color(220, 60, 60)});

  if (!skillMgr_.hasKnife())
    pool.push_back({UpgradeType::Knife, "MO KNIFE", "Dao bay theo huong",
                    sf::Color(200, 200, 60)});
  else if (skillMgr_.knifeMaxed() && !skillMgr_.knifeEvolved())
    pool.push_back({UpgradeType::Knife, "KNIFE EVOLVE", "Thousand Edge",
                    sf::Color(255, 220, 0)});
  else if (!skillMgr_.knifeEvolved())
    pool.push_back({UpgradeType::Knife,
                    "KNIFE Lv" + std::to_string(skillMgr_.getKnifeLevel() + 1),
                    "Tang so dao & toc do", sf::Color(200, 200, 60)});

  if (!skillMgr_.hasLightning())
    pool.push_back({UpgradeType::LightningRing, "MO LIGHTNING",
                    "Set danh quai gan", sf::Color(100, 180, 255)});
  else if (skillMgr_.lightningMaxed() && !skillMgr_.lightningEvolved())
    pool.push_back({UpgradeType::LightningRing, "LIGHTNING EVOLVE",
                    "Thunder Loop", sf::Color(180, 230, 255)});
  else if (!skillMgr_.lightningEvolved())
    pool.push_back(
        {UpgradeType::LightningRing,
         "LIGHTNING Lv" + std::to_string(skillMgr_.getLightningLevel() + 1),
         "Tang dame & vung set", sf::Color(100, 180, 255)});

  if (!skillMgr_.hasGarlic())
    pool.push_back({UpgradeType::Garlic, "MO GARLIC", "Vung AoE day lui quai",
                    sf::Color(180, 255, 100)});
  else if (skillMgr_.garlicMaxed() && !skillMgr_.garlicEvolved())
    pool.push_back({UpgradeType::Garlic, "GARLIC EVOLVE", "Soul Eater: hut mau",
                    sf::Color(100, 255, 80)});
  else if (!skillMgr_.garlicEvolved())
    pool.push_back(
        {UpgradeType::Garlic,
         "GARLIC Lv" + std::to_string(skillMgr_.getGarlicLevel() + 1),
         "Tang range & knockback", sf::Color(160, 230, 80)});

  auto rng = std::default_random_engine{std::random_device{}()};
  std::shuffle(pool.begin(), pool.end(), rng);

  // ✅ Đảm bảo luôn fill đủ 3 slot, lặp vòng nếu pool < 3
  for (int i = 0; i < 3; ++i) upgradeOptions_[i] = pool[i % pool.size()];
}

void Game::applyUpgrade(UpgradeType t) {
  SoundManager::get().play(SoundManager::SFX::UPGRADE_SELECT);
  switch (t) {
    case UpgradeType::Damage:
      stats_.upgradeDamage();
      break;
    case UpgradeType::AttackSpeed:
      stats_.upgradeAttackSpeed();
      break;
    case UpgradeType::Knife:
      skillMgr_.applyUpgrade(SkillUpgradeType::Knife);
      break;
    case UpgradeType::LightningRing:
      skillMgr_.applyUpgrade(SkillUpgradeType::LightningRing);
      break;
    case UpgradeType::Garlic:
      skillMgr_.applyUpgrade(SkillUpgradeType::Garlic);
      break;
  }
  paused_ = false;
  hoveredCard_ = -1;
}

// ════════════════════════════════════════════════════════════
//  renderUpgradeScreen
// ════════════════════════════════════════════════════════════
void Game::updateHover(sf::Vector2i mouse) {
  hoveredCard_ = -1;
  if (!paused_) return;
  const float cardW = 220.f, cardH = 280.f, gap = 30.f;
  float totalW = 3 * cardW + 2 * gap;
  float startX = (WIN_W - totalW) / 2.f;
  float cardY = WIN_H / 2.f - cardH / 2.f + 20.f;
  for (int i = 0; i < 3; ++i) {
    float cx = startX + i * (cardW + gap);
    if (mouse.x >= cx && mouse.x <= cx + cardW && mouse.y >= cardY &&
        mouse.y <= cardY + cardH) {
      hoveredCard_ = i;
      break;
    }
  }
}

sf::Vector2f Game::mouseToWorld() const {
  return window_.mapPixelToCoords(sf::Mouse::getPosition(window_),
                                  camera_.getView());
}

void Game::renderUpgradeScreen() {
  sf::View uiView(sf::FloatRect(
      {0.f, 0.f}, {static_cast<float>(WIN_W), static_cast<float>(WIN_H)}));
  window_.setView(uiView);

  sf::RectangleShape overlay(
      {static_cast<float>(WIN_W), static_cast<float>(WIN_H)});
  overlay.setFillColor(sf::Color(0, 0, 0, 170));
  window_.draw(overlay);

  if (fontLoaded_) {
    sf::Text title(font_, "LEVEL  UP!", 38);
    title.setFillColor(sf::Color(255, 230, 60));
    auto b = title.getLocalBounds();
    title.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    title.setPosition({WIN_W / 2.f, WIN_H / 2.f - 180.f});
    window_.draw(title);

    sf::Text sub(font_, "Chon nang cap  (1 / 2 / 3)", 14);
    sub.setFillColor(sf::Color(180, 180, 180));
    b = sub.getLocalBounds();
    sub.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    sub.setPosition({WIN_W / 2.f, WIN_H / 2.f - 135.f});
    window_.draw(sub);
  }

  const float cardW = 220.f, cardH = 280.f, gap = 30.f;
  float totalW = 3 * cardW + 2 * gap;
  float startX = (WIN_W - totalW) / 2.f;
  float cardY = WIN_H / 2.f - cardH / 2.f + 20.f;
  const char* keys[] = {"1", "2", "3"};

  for (int i = 0; i < 3; ++i) {
    float cx = startX + i * (cardW + gap);
    bool hov = (hoveredCard_ == i);
    const auto& opt = upgradeOptions_[i];

    sf::RectangleShape card({cardW, cardH});
    card.setFillColor(hov ? sf::Color(40, 40, 60, 240)
                          : sf::Color(18, 18, 30, 230));
    card.setOutlineThickness(hov ? 2.5f : 1.5f);
    card.setOutlineColor(
        hov ? sf::Color(opt.color.r, opt.color.g, opt.color.b, 255)
            : sf::Color(opt.color.r, opt.color.g, opt.color.b, 130));
    card.setPosition({cx, cardY});
    window_.draw(card);

    sf::RectangleShape bar({cardW, 5.f});
    bar.setFillColor(opt.color);
    bar.setPosition({cx, cardY});
    window_.draw(bar);

    if (!fontLoaded_) continue;

    sf::Text keyT(font_, keys[i], 20);
    keyT.setFillColor(opt.color);
    auto b = keyT.getLocalBounds();
    keyT.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    keyT.setPosition({cx + cardW / 2.f, cardY + 40.f});
    window_.draw(keyT);

    sf::Text titleT(font_, opt.title, 16);
    titleT.setFillColor(sf::Color::White);
    b = titleT.getLocalBounds();
    titleT.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    titleT.setPosition({cx + cardW / 2.f, cardY + 120.f});
    window_.draw(titleT);

    sf::Text descT(font_, opt.desc, 12);
    descT.setFillColor(sf::Color(180, 180, 180));
    b = descT.getLocalBounds();
    descT.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    descT.setPosition({cx + cardW / 2.f, cardY + 160.f});
    window_.draw(descT);

    if (hov) {
      sf::Text hint(font_, "[ CLICK ]", 11);
      hint.setFillColor(sf::Color(opt.color.r, opt.color.g, opt.color.b, 200));
      b = hint.getLocalBounds();
      hint.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
      hint.setPosition({cx + cardW / 2.f, cardY + cardH - 20.f});
      window_.draw(hint);
    }
  }

  window_.setView(camera_.getView());
}

// ════════════════════════════════════════════════════════════
//  renderPauseMenu  –  ESC in-game pause overlay
// ════════════════════════════════════════════════════════════
void Game::renderPauseMenu() {
  sf::View uiView(sf::FloatRect(
      {0.f, 0.f}, {static_cast<float>(WIN_W), static_cast<float>(WIN_H)}));
  window_.setView(uiView);

  // Semi-transparent overlay
  sf::RectangleShape overlay({(float)WIN_W, (float)WIN_H});
  overlay.setFillColor(sf::Color(0, 0, 0, 180));
  window_.draw(overlay);

  if (!fontLoaded_) {
    window_.setView(camera_.getView());
    return;
  }

  float cx = WIN_W / 2.f, cy = WIN_H / 2.f;

  // Panel
  const float panW = 320.f, panH = 420.f;
  sf::RectangleShape panel({panW, panH});
  panel.setFillColor(sf::Color(18, 14, 32, 245));
  panel.setOutlineColor(sf::Color(120, 80, 200, 220));
  panel.setOutlineThickness(2.f);
  panel.setOrigin({panW / 2.f, panH / 2.f});
  panel.setPosition({cx, cy});
  window_.draw(panel);

  // Title
  sf::Text title(font_, "PAUSED", 36);
  title.setFillColor(sf::Color(255, 230, 60));
  auto b = title.getLocalBounds();
  title.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
  title.setPosition({cx, cy - 165.f});
  window_.draw(title);

  // Helper: draw a button
  auto drawButton = [&](const std::string& label, float y, sf::Color outline,
                        sf::Color textCol) {
    const float bW = 240.f, bH = 48.f;
    sf::Vector2f mp =
        window_.mapPixelToCoords(sf::Mouse::getPosition(window_), uiView);
    bool hov = (mp.x >= cx - bW / 2.f && mp.x <= cx + bW / 2.f &&
                mp.y >= y - bH / 2.f && mp.y <= y + bH / 2.f);
    sf::RectangleShape btn({bW, bH});
    btn.setFillColor(hov ? sf::Color(50, 40, 80, 220)
                         : sf::Color(28, 22, 48, 200));
    btn.setOutlineColor(hov ? outline
                            : sf::Color(outline.r, outline.g, outline.b, 120));
    btn.setOutlineThickness(hov ? 2.f : 1.f);
    btn.setOrigin({bW / 2.f, bH / 2.f});
    btn.setPosition({cx, y});
    window_.draw(btn);

    sf::Text t(font_, label, 16);
    t.setFillColor(textCol);
    auto tb = t.getLocalBounds();
    t.setOrigin({tb.size.x / 2.f, tb.size.y / 2.f});
    t.setPosition({cx, y});
    window_.draw(t);
  };

  drawButton("TIEP TUC", cy - 80.f, sf::Color(80, 220, 120),
             sf::Color(100, 255, 140));
  drawButton("CAI DAT AM THANH", cy, sf::Color(100, 160, 255),
             sf::Color(140, 190, 255));
  drawButton("VE MENU CHINH", cy + 80.f, sf::Color(220, 160, 60),
             sf::Color(255, 190, 80));
  drawButton("THOAT GAME", cy + 160.f, sf::Color(220, 60, 60),
             sf::Color(255, 90, 90));

  // ── Settings sub-panel (shown when settingsOpen_) ────────
  if (settingsOpen_) {
    const float sW = 280.f, sH = 160.f;
    float sX = cx + panW / 2.f + 20.f, sY = cy - sH / 2.f;

    sf::RectangleShape sp({sW, sH});
    sp.setFillColor(sf::Color(20, 16, 36, 250));
    sp.setOutlineColor(sf::Color(100, 160, 255, 200));
    sp.setOutlineThickness(2.f);
    sp.setPosition({sX, sY});
    window_.draw(sp);

    sf::Text stitle(font_, "AM THANH", 18);
    stitle.setFillColor(sf::Color(140, 190, 255));
    stitle.setPosition({sX + 14.f, sY + 12.f});
    window_.draw(stitle);

    auto& sm = SoundManager::get();
    bool sfxOn = sm.isSfxEnabled();
    bool musicOn = sm.isMusicEnabled();

    const float pillW = 52.f, pillH = 24.f;
    float pillX = sX + sW - pillW - 14.f;

    auto drawToggle = [&](const std::string& label, bool on, float ty) {
      sf::Text lbl(font_, label, 13);
      lbl.setFillColor(sf::Color(200, 200, 200));
      lbl.setPosition({sX + 14.f, ty + 4.f});
      window_.draw(lbl);

      sf::RectangleShape pill({pillW, pillH});
      pill.setFillColor(on ? sf::Color(60, 200, 90) : sf::Color(80, 40, 40));
      pill.setOutlineColor(sf::Color(255, 255, 255, 60));
      pill.setOutlineThickness(1.f);
      pill.setPosition({pillX, ty});
      window_.draw(pill);

      sf::CircleShape knob(10.f);
      knob.setFillColor(sf::Color(240, 240, 240));
      float kx = on ? (pillX + pillW - 22.f) : (pillX + 2.f);
      knob.setPosition({kx, ty + 2.f});
      window_.draw(knob);

      sf::Text onoff(font_, on ? "ON" : "OFF", 10);
      onoff.setFillColor(on ? sf::Color(200, 255, 200)
                            : sf::Color(200, 120, 120));
      auto ob = onoff.getLocalBounds();
      onoff.setOrigin({ob.size.x / 2.f, ob.size.y / 2.f});
      onoff.setPosition({pillX + pillW / 2.f, ty + 12.f});
      window_.draw(onoff);
    };

    drawToggle("HIEU UNG AM THANH (SFX)", sfxOn, sY + 56.f);
    drawToggle("NHAC NEN (MUSIC)", musicOn, sY + 100.f);
  }

  window_.setView(camera_.getView());
}

// ════════════════════════════════════════════════════════════
//  handlePauseMenuClick
// ════════════════════════════════════════════════════════════
void Game::handlePauseMenuClick(sf::Vector2f mouseUI) {
  float cx = WIN_W / 2.f, cy = WIN_H / 2.f;
  const float bW = 240.f, bH = 48.f;

  auto hit = [&](float btnY) -> bool {
    return (mouseUI.x >= cx - bW / 2.f && mouseUI.x <= cx + bW / 2.f &&
            mouseUI.y >= btnY - bH / 2.f && mouseUI.y <= btnY + bH / 2.f);
  };

  // TIEP TUC
  if (hit(cy - 80.f)) {
    pauseMenuOpen_ = false;
    paused_ = false;
    settingsOpen_ = false;
    return;
  }

  // CAI DAT AM THANH
  if (hit(cy)) {
    settingsOpen_ = !settingsOpen_;
    return;
  }

  // VE MENU CHINH
  if (hit(cy + 80.f)) {
    pauseMenuOpen_ = false;
    paused_ = false;
    settingsOpen_ = false;
    gameState_ = GameState::Menu;
    window_.setView(window_.getDefaultView());
    SoundManager::get().stopAll();
    SoundManager::get().playMusic(SoundManager::BGM::MENU_BGM);
    menu_.init();
    return;
  }

  // THOAT GAME
  if (hit(cy + 160.f)) {
    window_.close();
    return;
  }

  // Settings toggles
  if (settingsOpen_) {
    const float panW = 320.f, sW = 280.f, sH = 160.f;
    float sX = cx + panW / 2.f + 20.f, sY = cy - sH / 2.f;
    const float pillW = 52.f, pillH = 24.f;
    float pillX = sX + sW - pillW - 14.f;

    // SFX toggle
    if (mouseUI.x >= pillX && mouseUI.x <= pillX + pillW &&
        mouseUI.y >= sY + 56.f && mouseUI.y <= sY + 56.f + pillH) {
      SoundManager::get().toggleSfx();
      return;
    }
    // Music toggle
    if (mouseUI.x >= pillX && mouseUI.x <= pillX + pillW &&
        mouseUI.y >= sY + 100.f && mouseUI.y <= sY + 100.f + pillH) {
      SoundManager::get().toggleMusic();
      return;
    }
  }
}
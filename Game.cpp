#include "Game.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

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
    : window_(sf::VideoMode({WIN_W, WIN_H}), "Game", sf::Style::Default),
      colMap_(),
      tileMap_(),
      player_(colMap_),
      camera_(window_) {
  std::srand(static_cast<unsigned>(std::time(nullptr)));
  window_.setFramerateLimit(60);

  for (int i = 0; i < MapData::COLLISION_LAYER_COUNT; ++i)
    colMap_.addLayer(MapData::COLLISION_LAYERS[i], MapData::LAYER_SIZE);
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
  monsters_.setSpawnWeight("flyeye", 3);
  monsters_.setSpawnWeight("slime", 0.5);
  monsters_.setSpawnWeight("skeleton", 1);

  for (auto* name :
       {"C:/Windows/Fonts/seguisym.ttf", "arial.ttf",
        "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/consola.ttf"}) {
    if (font_.openFromFile(name)) {
      fontLoaded_ = true;
      break;
    }
  }

  menu_.init();

  std::cout << "=== Game ===\n"
            << "WASD/Arrow : di chuyen\n"
            << "F1         : debug\n"
            << "Escape     : thoat\n\n";
}

// ════════════════════════════════════════════════════════════
//  applyCharacterClass — áp stat + skill khởi đầu theo class
// ════════════════════════════════════════════════════════════
void Game::applyCharacterClass(int charIndex) {
  // Clamp an toàn
  if (charIndex < 0 || charIndex > 2) charIndex = 0;
  const CharClassDef& def = CharacterClass::DEFS[charIndex];

  // ── Áp stat ──────────────────────────────────────────────
  stats_.maxHp = 10 + def.bonusHp;
  stats_.hp = stats_.maxHp;
  stats_.damage = 1 + def.bonusDamage;
  playerSpeed_ = Player::SPEED + def.bonusSpeed;

  // ── Trang bị skill khởi đầu ──────────────────────────────
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

  std::cout << "[Game] Class chon: " << def.name << "  HP=" << stats_.maxHp
            << "  DMG=" << stats_.damage << "  SPD=" << playerSpeed_ << "\n";
}

// ════════════════════════════════════════════════════════════
//  run
// ════════════════════════════════════════════════════════════
void Game::run() {
  while (window_.isOpen()) {
    float dt = clock_.restart().asSeconds();
    if (dt > MAX_DT) dt = MAX_DT;

    while (const auto event = window_.pollEvent()) {
      if (event->is<sf::Event::Closed>()) {
        window_.close();
        return;
      }

      if (inMenu_) {
        menu_.handleEvent(*event);
      } else {
        processEvents(*event);
      }
    }

    // ── Menu loop ─────────────────────────────────────────
    if (inMenu_) {
      menu_.update(dt);
      window_.clear();
      menu_.render();
      window_.display();

      if (menu_.isDone()) {
        auto res = menu_.getResult();
        if (res.quit) return;

        selectedChar_ = res.charIndex;

        // ★ Áp class nhân vật được chọn
        applyCharacterClass(selectedChar_);

        inMenu_ = false;
        monsters_.spawnInitial(camera_, 3);
      }
      continue;
    }

    // ── Game loop ─────────────────────────────────────────
    update(dt);
    render();
  }
}

// ════════════════════════════════════════════════════════════
//  update
// ════════════════════════════════════════════════════════════
void Game::update(float dt) {
  if (paused_) return;

  player_.update(dt);

  sf::Vector2f pPos = player_.getPosition();
  sf::Vector2f facing = player_.getFacingVector();
  sf::Vector2f moveDir = player_.getMoveVector();

  auto liveMonsters = monsters_.getLiveMonsters();
  std::vector<std::pair<sf::Vector2f, void*>> monsterData;
  monsterData.reserve(liveMonsters.size());
  for (auto* m : liveMonsters)
    monsterData.push_back({m->getPosition(), static_cast<void*>(m)});

  auto skillResult =
      skillMgr_.update(pPos, facing, moveDir, dt, stats_.damage, monsterData);

  // Knife → spawn đạn
  bullets_.spawnFromShots(skillResult.shots);

  // Lightning Ring → damage trực tiếp
  for (auto& zap : skillResult.zaps) {
    auto* m = static_cast<IMonster*>(zap.monster);
    if (m && m->isAlive()) {
      m->takeHit(zap.damage);
      if (!m->isAlive()) expManager_.spawnOrb(zap.pos, m->getExpValue());
    }
  }

  // Garlic → damage + knockback + heal
  for (auto& hit : skillResult.garlic) {
    auto* m = static_cast<IMonster*>(hit.monster);
    if (!m || !m->isAlive()) continue;

    m->applyKnockback(hit.knockbackDir, hit.knockbackForce);
    m->takeHit(hit.damage);

    if (!m->isAlive()) {
      expManager_.spawnOrb(hit.monsterPos, m->getExpValue());
      if (skillMgr_.garlicCanHeal())
        stats_.hp = std::min(stats_.hp + 1, stats_.maxHp);
    }
  }

  // Bullet update
  auto kills = bullets_.update(dt, liveMonsters, stats_.damage);
  for (auto& k : kills) expManager_.spawnOrb(k.pos, k.expValue);

  monsters_.update(dt, pPos, camera_);

  int monsterDmg = monsters_.collectPendingDamage();
  if (monsterDmg > 0) stats_.takeDamage(monsterDmg);

  int gained = expManager_.update(dt, pPos);
  if (gained > 0) {
    bool leveledUp = expManager_.addExp(gained);
    if (leveledUp) {
      levelUpTimer_ = 2.5f;
      paused_ = true;
      buildUpgradeOptions();
    }
  }
  if (levelUpTimer_ > 0.f) levelUpTimer_ -= dt;

  camera_.update(dt, pPos);
}

// ════════════════════════════════════════════════════════════
//  processEvents
// ════════════════════════════════════════════════════════════
void Game::processEvents(const sf::Event& event) {
  sf::Vector2i mousePixel = sf::Mouse::getPosition(window_);
  sf::View uiView(sf::FloatRect(
      {0.f, 0.f}, {static_cast<float>(WIN_W), static_cast<float>(WIN_H)}));
  sf::Vector2f mouseUI = window_.mapPixelToCoords(mousePixel, uiView);
  updateHover(sf::Vector2i(mouseUI));

  if (event.is<sf::Event::Closed>()) window_.close();

  if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
    switch (key->code) {
      case sf::Keyboard::Key::Escape:
        window_.close();
        break;
      case sf::Keyboard::Key::R:
        if (!paused_)
          player_.setPosition({MAP_WIDTH * TILE_RENDER_W / 2.f,
                               MAP_HEIGHT * TILE_RENDER_H / 2.f});
        break;
      case sf::Keyboard::Key::F1:
        debugMode_ = !debugMode_;
        break;
      case sf::Keyboard::Key::Num1:
        if (paused_) applyUpgrade(upgradeOptions_[0].type);
        break;
      case sf::Keyboard::Key::Num2:
        if (paused_) applyUpgrade(upgradeOptions_[1].type);
        break;
      case sf::Keyboard::Key::Num3:
        if (paused_) applyUpgrade(upgradeOptions_[2].type);
        break;
      default:
        break;
    }
  }

  if (paused_) {
    if (const auto* click = event.getIf<sf::Event::MouseButtonPressed>()) {
      if (click->button == sf::Mouse::Button::Left && hoveredCard_ >= 0)
        applyUpgrade(upgradeOptions_[hoveredCard_].type);
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
  if (paused_) renderUpgradeScreen();
  window_.display();
}

// ════════════════════════════════════════════════════════════
//  HUD
// ════════════════════════════════════════════════════════════
static void drawBarHUD(sf::RenderWindow& win, sf::Vector2f pos,
                       sf::Vector2f size, float ratio, sf::Color bgCol,
                       sf::Color fillCol, sf::Color glowCol,
                       float cornerR = 6.f) {
  (void)cornerR;
  sf::RectangleShape glow(size + sf::Vector2f(4.f, 4.f));
  glow.setFillColor(sf::Color::Transparent);
  glow.setOutlineThickness(2.f);
  glow.setOutlineColor(sf::Color(glowCol.r, glowCol.g, glowCol.b, 80));
  glow.setPosition(pos - sf::Vector2f(2.f, 2.f));
  win.draw(glow);

  sf::RectangleShape border(size + sf::Vector2f(2.f, 2.f));
  border.setFillColor(sf::Color::Transparent);
  border.setOutlineThickness(1.f);
  border.setOutlineColor(sf::Color(glowCol.r, glowCol.g, glowCol.b, 180));
  border.setPosition(pos - sf::Vector2f(1.f, 1.f));
  win.draw(border);

  sf::RectangleShape bg(size);
  bg.setFillColor(bgCol);
  bg.setPosition(pos);
  win.draw(bg);

  if (ratio > 0.f) {
    float fillW = size.x * std::min(ratio, 1.f);
    sf::RectangleShape fill({fillW, size.y});
    fill.setFillColor(fillCol);
    fill.setPosition(pos);
    win.draw(fill);
    sf::RectangleShape shine({fillW, size.y * 0.35f});
    shine.setFillColor(sf::Color(255, 255, 255, 40));
    shine.setPosition(pos);
    win.draw(shine);
  }
}

void Game::renderHUD() {
  sf::View uiView(sf::FloatRect(
      {0.f, 0.f}, {static_cast<float>(WIN_W), static_cast<float>(WIN_H)}));
  window_.setView(uiView);

  const float margin = 14.f, barW = 280.f, barH = 18.f, gap = 8.f;
  const float hpY = margin, expY = hpY + barH + gap;

  // ── Class badge nhỏ góc trên trái ────────────────────────
  if (fontLoaded_ && selectedChar_ >= 0 && selectedChar_ <= 2) {
    const auto& def = CharacterClass::DEFS[selectedChar_];
    sf::RectangleShape badge({60.f, 18.f});
    badge.setFillColor(
        sf::Color(def.color.r / 4, def.color.g / 4, def.color.b / 4, 200));
    badge.setOutlineColor(
        sf::Color(def.color.r, def.color.g, def.color.b, 160));
    badge.setOutlineThickness(1.f);
    badge.setPosition({margin + barW + 10.f, margin});
    window_.draw(badge);

    sf::Text classTxt(font_, def.name, 11);
    classTxt.setFillColor(def.color);
    classTxt.setPosition({margin + barW + 14.f, margin + 2.f});
    window_.draw(classTxt);
  }

  // HP Bar
  float hpRatio =
      static_cast<float>(stats_.hp) / static_cast<float>(stats_.maxHp);
  sf::Color hpFill = (hpRatio > 0.5f)    ? sf::Color(50, 220, 80)
                     : (hpRatio > 0.25f) ? sf::Color(240, 190, 30)
                                         : sf::Color(220, 40, 40);
  drawBarHUD(window_, {margin, hpY}, {barW, barH}, hpRatio,
             sf::Color(20, 5, 5, 210), hpFill, hpFill);

  if (fontLoaded_) {
    sf::Text icon(font_, "\u2665", 11);
    icon.setFillColor(sf::Color(255, 100, 100));
    icon.setPosition({margin + 4.f, hpY + 2.f});
    window_.draw(icon);

    std::ostringstream ss;
    ss << "HP  " << stats_.hp << " / " << stats_.maxHp;
    sf::Text txt(font_, ss.str(), 11);
    txt.setFillColor(sf::Color(240, 240, 240, 230));
    auto b = txt.getLocalBounds();
    txt.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    txt.setPosition({margin + barW / 2.f, hpY + barH / 2.f});
    window_.draw(txt);
  }

  // EXP Bar
  float expRatio = static_cast<float>(expManager_.getExp()) /
                   static_cast<float>(expManager_.getExpReq());
  drawBarHUD(window_, {margin, expY}, {barW, barH}, expRatio,
             sf::Color(8, 5, 20, 210), sf::Color(140, 70, 255),
             sf::Color(170, 110, 255));

  if (fontLoaded_) {
    std::ostringstream ss;
    ss << "Lv " << expManager_.getLevel() << "   " << expManager_.getExp()
       << " / " << expManager_.getExpReq() << " EXP";
    sf::Text txt(font_, ss.str(), 11);
    txt.setFillColor(sf::Color(210, 190, 255, 230));
    auto b = txt.getLocalBounds();
    txt.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    txt.setPosition({margin + barW / 2.f, expY + barH / 2.f});
    window_.draw(txt);
  }

  // Stats panel
  if (fontLoaded_) {
    float panelY = expY + barH + 8.f;
    sf::RectangleShape panel({barW, 80.f});
    panel.setFillColor(sf::Color(0, 0, 0, 150));
    panel.setOutlineColor(sf::Color(255, 200, 50, 100));
    panel.setOutlineThickness(1.f);
    panel.setPosition({margin, panelY});
    window_.draw(panel);

    auto makeTxt = [&](const std::string& s, sf::Color c, float oy) {
      sf::Text t(font_, s, 11);
      t.setFillColor(c);
      t.setPosition({margin + 6.f, panelY + oy});
      window_.draw(t);
    };

    std::ostringstream a1, a2, a3, a4;
    a1 << "DAMAGE  : " << stats_.damage << "  [Lv" << stats_.damageLevel << "]";
    a2 << "Kills: " << bullets_.killCount()
       << "   Alive: " << monsters_.aliveCount();

    if (skillMgr_.hasKnife()) {
      a3 << "KNIFE Lv" << skillMgr_.getKnifeLevel();
      if (skillMgr_.knifeEvolved()) a3 << " [EVO]";
    }
    if (skillMgr_.hasLightning()) {
      if (!a3.str().empty()) a3 << "  |  ";
      a3 << "LIGHTNING Lv" << skillMgr_.getLightningLevel();
      if (skillMgr_.lightningEvolved()) a3 << " [EVO]";
    }
    if (skillMgr_.hasGarlic()) {
      a4 << "GARLIC Lv" << skillMgr_.getGarlicLevel();
      if (skillMgr_.garlicEvolved()) a4 << " [EVO]";
    }

    makeTxt(a1.str(), sf::Color(255, 160, 160), 6.f);
    makeTxt(a2.str(), sf::Color(200, 200, 200), 22.f);
    if (!a3.str().empty()) makeTxt(a3.str(), sf::Color(255, 230, 80), 38.f);
    if (!a4.str().empty()) makeTxt(a4.str(), sf::Color(120, 200, 255), 54.f);
  }

  window_.setView(camera_.getView());
}

// ════════════════════════════════════════════════════════════
//  Upgrade system
// ════════════════════════════════════════════════════════════
void Game::buildUpgradeOptions() {
  std::vector<UpgradeOption> pool;

  pool.push_back({UpgradeType::Damage, "TANG DAME", "Tang sat thuong +1",
                  sf::Color(220, 60, 60)});

  // Knife
  if (!skillMgr_.hasKnife())
    pool.push_back({UpgradeType::Knife, "MO KNIFE", "Dao bay theo huong",
                    sf::Color(200, 200, 60)});
  else if (skillMgr_.knifeMaxed() && !skillMgr_.knifeEvolved())
    pool.push_back({UpgradeType::Knife, "KNIFE EVOLVE",
                    "Thousand Edge:\nBan lien tuc", sf::Color(255, 220, 0)});
  else if (!skillMgr_.knifeEvolved())
    pool.push_back({UpgradeType::Knife,
                    "KNIFE Lv" + std::to_string(skillMgr_.getKnifeLevel() + 1),
                    "Tang so dao & toc do", sf::Color(200, 200, 60)});

  // Lightning Ring
  if (!skillMgr_.hasLightning())
    pool.push_back({UpgradeType::LightningRing, "MO LIGHTNING",
                    "Set danh quai trong vung", sf::Color(100, 180, 255)});
  else if (skillMgr_.lightningMaxed() && !skillMgr_.lightningEvolved())
    pool.push_back({UpgradeType::LightningRing, "LIGHTNING EVOLVE",
                    "Thunder Loop:\nSet toan man", sf::Color(180, 230, 255)});
  else if (!skillMgr_.lightningEvolved())
    pool.push_back(
        {UpgradeType::LightningRing,
         "LIGHTNING Lv" + std::to_string(skillMgr_.getLightningLevel() + 1),
         "Tang dame & vung set", sf::Color(100, 180, 255)});

  // Garlic
  if (!skillMgr_.hasGarlic())
    pool.push_back({UpgradeType::Garlic, "MO GARLIC", "Vung AoE day lui quai",
                    sf::Color(180, 255, 100)});
  else if (skillMgr_.garlicMaxed() && !skillMgr_.garlicEvolved())
    pool.push_back({UpgradeType::Garlic, "GARLIC EVOLVE",
                    "Soul Eater:\nHut mau quai chet", sf::Color(100, 255, 80)});
  else if (!skillMgr_.garlicEvolved())
    pool.push_back(
        {UpgradeType::Garlic,
         "GARLIC Lv" + std::to_string(skillMgr_.getGarlicLevel() + 1),
         "Tang range & knockback", sf::Color(160, 230, 80)});

  auto rng = std::default_random_engine{std::random_device{}()};
  std::shuffle(pool.begin(), pool.end(), rng);
  for (int i = 0; i < 3 && i < (int)pool.size(); ++i)
    upgradeOptions_[i] = pool[i];
}

void Game::applyUpgrade(UpgradeType t) {
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
//  Upgrade Screen
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
    sf::Text glow(font_, "LEVEL  UP!", 38);
    glow.setFillColor(sf::Color(255, 220, 0, 60));
    auto b = glow.getLocalBounds();
    glow.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    for (int dx = -3; dx <= 3; dx += 3)
      for (int dy = -3; dy <= 3; dy += 3) {
        glow.setPosition({WIN_W / 2.f + dx, WIN_H / 2.f - 180.f + dy});
        window_.draw(glow);
      }
    sf::Text title(font_, "LEVEL  UP!", 38);
    title.setFillColor(sf::Color(255, 230, 60));
    b = title.getLocalBounds();
    title.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    title.setPosition({WIN_W / 2.f, WIN_H / 2.f - 180.f});
    window_.draw(title);

    sf::Text sub(font_, "Choose an upgrade  (1 / 2 / 3)", 14);
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
  sf::String icons[] = {L"\u2694", L"\u26A1", L"\u25B6"};
  const char* keys[] = {"1", "2", "3"};

  for (int i = 0; i < 3; ++i) {
    float cx = startX + i * (cardW + gap);
    bool hovered = (hoveredCard_ == i);
    const auto& opt = upgradeOptions_[i];

    sf::RectangleShape shadow({cardW + 8.f, cardH + 8.f});
    shadow.setFillColor(sf::Color(0, 0, 0, 120));
    shadow.setPosition({cx - 4.f + 6.f, cardY - 4.f + 6.f});
    window_.draw(shadow);

    sf::RectangleShape card({cardW, cardH});
    card.setFillColor(hovered ? sf::Color(40, 40, 60, 240)
                              : sf::Color(18, 18, 30, 230));
    card.setOutlineThickness(hovered ? 2.5f : 1.5f);
    card.setOutlineColor(
        hovered ? sf::Color(opt.color.r, opt.color.g, opt.color.b, 255)
                : sf::Color(opt.color.r, opt.color.g, opt.color.b, 130));
    card.setPosition({cx, cardY});
    window_.draw(card);

    sf::RectangleShape accentBar({cardW, 5.f});
    accentBar.setFillColor(opt.color);
    accentBar.setPosition({cx, cardY});
    window_.draw(accentBar);

    if (!fontLoaded_) continue;

    sf::CircleShape badge(16.f);
    badge.setFillColor(sf::Color(opt.color.r, opt.color.g, opt.color.b, 200));
    badge.setOrigin({16.f, 16.f});
    badge.setPosition({cx + cardW / 2.f, cardY + 38.f});
    window_.draw(badge);

    sf::Text keyTxt(font_, keys[i], 18);
    keyTxt.setFillColor(sf::Color::White);
    auto b = keyTxt.getLocalBounds();
    keyTxt.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    keyTxt.setPosition({cx + cardW / 2.f, cardY + 36.f});
    window_.draw(keyTxt);

    sf::Text iconTxt(font_, icons[i], 32);
    iconTxt.setFillColor(opt.color);
    b = iconTxt.getLocalBounds();
    iconTxt.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    iconTxt.setPosition({cx + cardW / 2.f, cardY + 100.f});
    window_.draw(iconTxt);

    sf::Text titleTxt(font_, opt.title, 16);
    titleTxt.setFillColor(sf::Color::White);
    b = titleTxt.getLocalBounds();
    titleTxt.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
    titleTxt.setPosition({cx + cardW / 2.f, cardY + 155.f});
    window_.draw(titleTxt);

    sf::RectangleShape div({cardW * 0.6f, 1.f});
    div.setFillColor(sf::Color(opt.color.r, opt.color.g, opt.color.b, 100));
    div.setOrigin({cardW * 0.3f, 0.f});
    div.setPosition({cx + cardW / 2.f, cardY + 172.f});
    window_.draw(div);

    float lineY = cardY + 188.f;
    std::string line, desc = opt.desc;
    for (char c : desc + '\n') {
      if (c == '\n') {
        if (!line.empty()) {
          sf::Text dTxt(font_, line, 12);
          dTxt.setFillColor(sf::Color(180, 180, 180));
          b = dTxt.getLocalBounds();
          dTxt.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
          dTxt.setPosition({cx + cardW / 2.f, lineY});
          window_.draw(dTxt);
          lineY += 18.f;
          line.clear();
        }
      } else {
        line += c;
      }
    }

    if (hovered) {
      sf::Text clickHint(font_, "[ CLICK ]", 11);
      clickHint.setFillColor(
          sf::Color(opt.color.r, opt.color.g, opt.color.b, 200));
      b = clickHint.getLocalBounds();
      clickHint.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
      clickHint.setPosition({cx + cardW / 2.f, cardY + cardH - 20.f});
      window_.draw(clickHint);
    }
  }

  window_.setView(camera_.getView());
}
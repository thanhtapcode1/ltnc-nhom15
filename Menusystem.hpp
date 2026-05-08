#pragma once
// ════════════════════════════════════════════════════════════
//  MenuSystem.hpp  —  Menu chính + Chọn nhân vật + Độ khó
// ════════════════════════════════════════════════════════════
#include <SFML/Graphics.hpp>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "CharacterClass.hpp"
#include "ScoreSystem.hpp"  // Difficulty enum
#include "dokho.hpp"
enum class MenuScreen { MainMenu, CharSelect, FadeOut };

class MenuSystem {
 public:
  struct Result {
    int charIndex = 0;
    Difficulty difficulty = Difficulty::Easy;
    bool quit = false;
  };

  MenuSystem(sf::RenderWindow& window, sf::Font& font)
      : window_(window), font_(font) {}

  void init() {
    winW_ = static_cast<float>(window_.getSize().x);
    winH_ = static_cast<float>(window_.getSize().y);
    fadeAlpha_ = 255.f;
    screen_ = MenuScreen::MainMenu;
    active_ = true;
    done_ = false;
    hovered_ = -1;
    selectedChar_ = 0;
    selectedDiff_ = Difficulty::Easy;
    animTime_ = 0.f;
  }

  bool isActive() const { return active_; }
  bool isDone() const { return done_; }
  Result getResult() const { return result_; }

  void handleEvent(const sf::Event& event) {
    if (!active_ || done_) return;

    if (const auto* mm = event.getIf<sf::Event::MouseMoved>()) {
      mousePos_ = {(float)mm->position.x, (float)mm->position.y};
      updateHover();
    }
    if (const auto* mb = event.getIf<sf::Event::MouseButtonPressed>()) {
      if (mb->button == sf::Mouse::Button::Left) {
        mousePos_ = {(float)mb->position.x, (float)mb->position.y};
        handleClick();
      }
    }
    if (const auto* kb = event.getIf<sf::Event::KeyPressed>()) {
      if (kb->code == sf::Keyboard::Key::Escape &&
          screen_ == MenuScreen::CharSelect)
        screen_ = MenuScreen::MainMenu;
    }
  }

  void update(float dt) {
    if (!active_) return;
    animTime_ += dt;
    if (fadeAlpha_ > 0.f && screen_ != MenuScreen::FadeOut)
      fadeAlpha_ = std::max(0.f, fadeAlpha_ - dt * 400.f);
    if (screen_ == MenuScreen::FadeOut) {
      fadeAlpha_ = std::min(255.f, fadeAlpha_ + dt * 300.f);
      if (fadeAlpha_ >= 255.f) {
        active_ = false;
        done_ = true;
      }
    }
  }

  void render() {
    if (!active_) return;
    sf::RectangleShape bg({winW_, winH_});
    bg.setFillColor(sf::Color(15, 12, 20));
    window_.draw(bg);
    drawParticles();
    if (screen_ == MenuScreen::MainMenu)
      drawMainMenu();
    else
      drawCharSelect();
    if (fadeAlpha_ > 0.f) {
      sf::RectangleShape fade({winW_, winH_});
      fade.setFillColor(sf::Color(0, 0, 0, static_cast<uint8_t>(fadeAlpha_)));
      window_.draw(fade);
    }
  }

 private:
  // ── Main Menu ─────────────────────────────────────────────
  void drawMainMenu() {
    float cx = winW_ * 0.5f;
    float titY = winH_ * 0.22f;
    float bob = std::sin(animTime_ * 1.8f) * 5.f;

    sf::CircleShape glow(180.f);
    glow.setFillColor(sf::Color(80, 40, 120, 40));
    glow.setOrigin({180.f, 180.f});
    glow.setPosition({cx, titY + bob});
    window_.draw(glow);

    drawText("VAMPIRE", 54, {cx, titY - 32.f + bob}, sf::Color(220, 80, 80),
             true);
    drawText("SURVIVORS", 36, {cx, titY + 30.f + bob}, sf::Color(180, 140, 200),
             true);
    drawText("Clone", 16, {cx, titY + 68.f + bob}, sf::Color(120, 100, 140),
             true);

    float panW = 280.f, panH = 220.f;
    float panX = cx - panW * 0.5f, panY = winH_ * 0.46f;
    drawPanel(panX, panY, panW, panH);

    struct Btn {
      std::string label;
      int id;
      sf::Color col;
    };
    std::vector<Btn> btns = {
        {"PLAY", 0, sf::Color(220, 120, 60)},
        {"SETTINGS", 1, sf::Color(100, 140, 200)},
        {"QUIT", 2, sf::Color(160, 80, 80)},
    };

    mainMenuRects_.clear();
    float btnW = 200.f, btnH = 44.f;
    float btnX = cx - btnW * 0.5f, startY = panY + 28.f;
    for (int i = 0; i < (int)btns.size(); ++i) {
      float by = startY + i * (btnH + 14.f);
      drawButton(btnX, by, btnW, btnH, btns[i].label, btns[i].col,
                 hovered_ == i);
      mainMenuRects_.push_back({btnX, by, btnW, btnH});
    }
    drawText("Press ESC to quit", 11, {cx, winH_ - 22.f}, sf::Color(80, 70, 90),
             true);
  }

  // ── Char Select ───────────────────────────────────────────
  void drawCharSelect() {
    float cx = winW_ * 0.5f;
    drawText("CHON NHAN VAT", 28, {cx, winH_ * 0.08f}, sf::Color(220, 180, 100),
             true);
    drawText("Chon nhan vat va do kho", 13, {cx, winH_ * 0.08f + 34.f},
             sf::Color(140, 120, 150), true);

    // ── Character cards ──────────────────────────────────
    const int N = 3;
    const float cardW = 180.f, cardH = 270.f, gap = 20.f;
    float totalW = N * cardW + (N - 1) * gap;
    float startX = cx - totalW * 0.5f;
    float cardY = winH_ * 0.18f;

    charCardRects_.clear();
    for (int i = 0; i < N; ++i) {
      float xcx = startX + i * (cardW + gap);
      bool hov = (hovered_ == 100 + i), sel = (selectedChar_ == i);
      drawCharCard(xcx, cardY, cardW, cardH, CharacterClass::DEFS[i], hov, sel);
      charCardRects_.push_back({xcx, cardY, cardW, cardH});
    }

    // ── Difficulty toggle ─────────────────────────────────
    float diffY = cardY + cardH + 22.f;
    drawText("DO KHO:", 14, {cx - 120.f, diffY + 8.f}, sf::Color(180, 180, 180),
             false);

    float btnW = 100.f, btnH = 36.f, bGap = 12.f;
    float diffBtnX = cx - btnW - bGap * 0.5f;

    // Easy
    bool easyHov = (hovered_ == 300);
    bool easySel = (selectedDiff_ == Difficulty::Easy);
    drawDiffButton(diffBtnX, diffY, btnW, btnH, "EASY", sf::Color(80, 220, 80),
                   easyHov, easySel);
    diffEasyRect_ = {diffBtnX, diffY, btnW, btnH};

    // Hard
    float hardBtnX = cx + bGap * 0.5f;
    bool hardHov = (hovered_ == 301);
    bool hardSel = (selectedDiff_ == Difficulty::Hard);
    drawDiffButton(hardBtnX, diffY, btnW, btnH, "HARD", sf::Color(220, 80, 80),
                   hardHov, hardSel);
    diffHardRect_ = {hardBtnX, diffY, btnW, btnH};

    // Mô tả độ khó
    const DifficultyConfig& cfg = DifficultyConfig::get(selectedDiff_);
    drawText(cfg.description, 11, {cx, diffY + btnH + 16.f},
             sf::Color(140, 140, 160), true);

    // ── Start button ──────────────────────────────────────
    float startBtnW = 200.f, startBtnH = 48.f;
    float startBtnX = cx - startBtnW * 0.5f;
    float startBtnY = diffY + btnH + 42.f;
    drawButton(startBtnX, startBtnY, startBtnW, startBtnH, "BAT DAU",
               sf::Color(220, 120, 60), hovered_ == 200, true);
    startBtnRect_ = {startBtnX, startBtnY, startBtnW, startBtnH};

    drawText("< Quay lai  (ESC)", 12, {cx, startBtnY + startBtnH + 18.f},
             sf::Color(100, 90, 110), true);
  }

  void drawDiffButton(float x, float y, float w, float h,
                      const std::string& label, sf::Color col, bool hovered,
                      bool selected) {
    sf::Color bg = selected ? sf::Color(col.r / 2, col.g / 2, col.b / 2, 240)
                            : sf::Color(30, 25, 40, 220);
    sf::RectangleShape btn({w, h});
    btn.setFillColor(bg);
    btn.setOutlineColor(selected || hovered ? col : sf::Color(70, 60, 85));
    btn.setOutlineThickness(selected ? 2.5f : 1.f);
    btn.setPosition({x, y});
    window_.draw(btn);

    if (selected) {
      sf::RectangleShape bar({w, 4.f});
      bar.setFillColor(col);
      bar.setPosition({x, y});
      window_.draw(bar);
    }

    drawText(label, 14, {x + w / 2.f, y + h / 2.f - 8.f},
             selected ? sf::Color::White : sf::Color(180, 180, 180), true);
  }

  void drawCharCard(float x, float y, float w, float h, const CharClassDef& ch,
                    bool hovered, bool selected) {
    float lift = selected ? 8.f : 0.f;
    if (hovered) lift = std::min(lift + 5.f, 12.f);
    float bob = selected ? std::sin(animTime_ * 2.5f) * 3.f : 0.f;
    y -= (lift + bob);

    drawPanel(x, y, w, h,
              selected ? sf::Color(45, 35, 60) : sf::Color(28, 22, 38));

    if (selected || hovered) {
      sf::RectangleShape border({w, h});
      border.setFillColor(sf::Color::Transparent);
      border.setOutlineColor(selected ? ch.color : sf::Color(100, 90, 120));
      border.setOutlineThickness(selected ? 2.5f : 1.f);
      border.setPosition({x, y});
      window_.draw(border);
    }

    float cxc = x + w * 0.5f, iconY = y + 38.f;
    sf::CircleShape iconBg(32.f);
    iconBg.setFillColor(
        sf::Color(ch.color.r / 5, ch.color.g / 5, ch.color.b / 5, 200));
    iconBg.setOutlineColor(sf::Color(ch.color.r, ch.color.g, ch.color.b, 180));
    iconBg.setOutlineThickness(2.f);
    iconBg.setOrigin({32.f, 32.f});
    iconBg.setPosition({cxc, iconY});
    window_.draw(iconBg);
    drawText(ch.icon, 30, {cxc, iconY - 15.f}, ch.color, true);

    drawText(ch.name, 18, {cxc, iconY + 44.f}, sf::Color(230, 220, 240), true);
    drawText(ch.weaponName, 11, {cxc, iconY + 68.f}, ch.color, true);
    drawTextWrapped(ch.description, 11, x + 10.f, iconY + 90.f, w - 20.f,
                    sf::Color(160, 150, 170));

    float statY = y + h - 54.f;
    sf::RectangleShape sb({w - 16.f, 46.f});
    sb.setFillColor(sf::Color(20, 15, 30, 200));
    sb.setPosition({x + 8.f, statY});
    window_.draw(sb);
    drawTextWrapped(ch.statLine, 10, x + 12.f, statY + 5.f, w - 24.f,
                    sf::Color(180, 200, 160));
  }

  // ── Helpers ───────────────────────────────────────────────
  void drawPanel(float x, float y, float w, float h,
                 sf::Color col = sf::Color(28, 22, 38)) {
    sf::RectangleShape shadow({w + 8.f, h + 8.f});
    shadow.setFillColor(sf::Color(0, 0, 0, 80));
    shadow.setPosition({x + 4.f, y + 6.f});
    window_.draw(shadow);
    sf::RectangleShape panel({w, h});
    panel.setFillColor(col);
    panel.setOutlineColor(sf::Color(70, 55, 90, 180));
    panel.setOutlineThickness(1.f);
    panel.setPosition({x, y});
    window_.draw(panel);
  }

  void drawButton(float x, float y, float w, float h, const std::string& label,
                  sf::Color acc, bool hovered, bool big = false) {
    if (hovered) {
      sf::RectangleShape glow({w + 12.f, h + 12.f});
      glow.setFillColor(sf::Color(acc.r, acc.g, acc.b, 40));
      glow.setPosition({x - 6.f, y - 6.f});
      window_.draw(glow);
    }
    sf::Color bg = hovered ? sf::Color(acc.r / 2, acc.g / 2, acc.b / 2, 220)
                           : sf::Color(35, 28, 48, 220);
    sf::RectangleShape btn({w, h});
    btn.setFillColor(bg);
    btn.setOutlineColor(hovered ? acc : sf::Color(70, 60, 85));
    btn.setOutlineThickness(hovered ? 1.5f : 1.f);
    btn.setPosition({x, y});
    window_.draw(btn);
    drawText(label, big ? 18u : 15u,
             {x + w * 0.5f, y + h * 0.5f - (big ? 10.f : 8.f)},
             hovered ? sf::Color::White : sf::Color(200, 190, 210), true);
  }

  void drawText(const std::string& s, unsigned sz, sf::Vector2f pos,
                sf::Color col, bool centered = false) {
    sf::Text t(font_, s, sz);
    t.setFillColor(col);
    if (centered) {
      auto b = t.getLocalBounds();
      t.setOrigin(
          {b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
    }
    t.setPosition(pos);
    window_.draw(t);
  }

  void drawTextWrapped(const std::string& str, unsigned sz, float x, float y,
                       float maxW, sf::Color col) {
    std::vector<std::string> words;
    std::string cur;
    for (char c : str) {
      if (c == ' ' || c == '\n') {
        if (!cur.empty()) {
          words.push_back(cur);
          cur.clear();
        }
        if (c == '\n') words.push_back("\n");
      } else
        cur += c;
    }
    if (!cur.empty()) words.push_back(cur);

    std::string line;
    float lineH = sz * 1.4f;
    int row = 0;
    for (auto& w : words) {
      if (w == "\n") {
        drawText(line, sz, {x, y + row * lineH}, col);
        line.clear();
        ++row;
        continue;
      }
      std::string test = line.empty() ? w : line + " " + w;
      sf::Text tmp(font_, test, sz);
      if (tmp.getLocalBounds().size.x > maxW && !line.empty()) {
        drawText(line, sz, {x, y + row * lineH}, col);
        line = w;
        ++row;
      } else
        line = test;
    }
    if (!line.empty()) drawText(line, sz, {x, y + row * lineH}, col);
  }

  void drawParticles() {
    for (int i = 0; i < 40; ++i) {
      float phase = (float)i / 40.f;
      float t = std::fmod(animTime_ * 0.3f + phase, 1.f);
      float px = winW_ * (0.1f + phase * 0.82f);
      float py = winH_ * (1.f - t);
      float a = std::sin(t * 3.14159f) * 120.f;
      float r = 1.5f + std::sin(phase * 7.3f + animTime_) * 1.f;
      sf::CircleShape p(r);
      p.setFillColor(sf::Color(180, 140, 220, static_cast<uint8_t>(a)));
      p.setPosition({px, py});
      window_.draw(p);
    }
  }

  bool inRect(float mx, float my, float rx, float ry, float rw, float rh) {
    return mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh;
  }

  void updateHover() {
    hovered_ = -1;
    float mx = mousePos_.x, my = mousePos_.y;
    if (screen_ == MenuScreen::MainMenu) {
      for (int i = 0; i < (int)mainMenuRects_.size(); ++i) {
        auto& r = mainMenuRects_[i];
        if (inRect(mx, my, r[0], r[1], r[2], r[3])) {
          hovered_ = i;
          break;
        }
      }
    } else {
      for (int i = 0; i < (int)charCardRects_.size(); ++i) {
        auto& r = charCardRects_[i];
        if (inRect(mx, my, r[0], r[1], r[2], r[3])) {
          hovered_ = 100 + i;
          break;
        }
      }
      auto& sb = startBtnRect_;
      if (inRect(mx, my, sb[0], sb[1], sb[2], sb[3])) hovered_ = 200;
      if (inRect(mx, my, diffEasyRect_[0], diffEasyRect_[1], diffEasyRect_[2],
                 diffEasyRect_[3]))
        hovered_ = 300;
      if (inRect(mx, my, diffHardRect_[0], diffHardRect_[1], diffHardRect_[2],
                 diffHardRect_[3]))
        hovered_ = 301;
    }
  }

  void handleClick() {
    float mx = mousePos_.x, my = mousePos_.y;
    if (screen_ == MenuScreen::MainMenu) {
      for (int i = 0; i < (int)mainMenuRects_.size(); ++i) {
        auto& r = mainMenuRects_[i];
        if (!inRect(mx, my, r[0], r[1], r[2], r[3])) continue;
        if (i == 0) {
          screen_ = MenuScreen::CharSelect;
          hovered_ = -1;
        } else if (i == 2) {
          result_.quit = true;
          window_.close();
        }
        break;
      }
    } else {
      // Card chọn nhân vật
      for (int i = 0; i < (int)charCardRects_.size(); ++i) {
        auto& r = charCardRects_[i];
        if (inRect(mx, my, r[0], r[1], r[2], r[3])) {
          selectedChar_ = i;
          break;
        }
      }
      // Difficulty
      if (inRect(mx, my, diffEasyRect_[0], diffEasyRect_[1], diffEasyRect_[2],
                 diffEasyRect_[3]))
        selectedDiff_ = Difficulty::Easy;
      if (inRect(mx, my, diffHardRect_[0], diffHardRect_[1], diffHardRect_[2],
                 diffHardRect_[3]))
        selectedDiff_ = Difficulty::Hard;
      // Start
      auto& sb = startBtnRect_;
      if (inRect(mx, my, sb[0], sb[1], sb[2], sb[3])) {
        result_.charIndex = selectedChar_;
        result_.difficulty = selectedDiff_;
        screen_ = MenuScreen::FadeOut;
      }
    }
  }

  // ── Members ───────────────────────────────────────────────
  sf::RenderWindow& window_;
  sf::Font& font_;

  float winW_ = 0.f, winH_ = 0.f;
  float animTime_ = 0.f, fadeAlpha_ = 255.f;
  bool active_ = false, done_ = false;
  int hovered_ = -1, selectedChar_ = 0;
  Difficulty selectedDiff_ = Difficulty::Easy;

  MenuScreen screen_ = MenuScreen::MainMenu;
  Result result_;
  sf::Vector2f mousePos_;

  std::vector<std::array<float, 4>> mainMenuRects_;
  std::vector<std::array<float, 4>> charCardRects_;
  std::array<float, 4> startBtnRect_ = {};
  std::array<float, 4> diffEasyRect_ = {};
  std::array<float, 4> diffHardRect_ = {};
};
#pragma once
// ════════════════════════════════════════════════════════════
//  MenuSystem.hpp  —  GUI Menu chính + Chọn nhân vật
//  Dùng data từ CharacterClass.hpp (không hardcode)
// ════════════════════════════════════════════════════════════
#include <SFML/Graphics.hpp>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "CharacterClass.hpp"

// ── Thông tin nhân vật — lấy từ CharacterClass::DEFS ────────
// (không dùng struct CharInfo riêng nữa)

// ── Màn hình hiện tại ─────────────────────────────────────────
enum class MenuScreen { MainMenu, CharSelect, FadeOut };

class MenuSystem {
 public:
  // Kết quả trả về khi player chọn xong
  struct Result {
    int charIndex = 0;  // 0=Warrior, 1=Mage, 2=Rogue
    bool quit = false;
  };

  MenuSystem(sf::RenderWindow& window, sf::Font& font)
      : window_(window), font_(font) {}

  // ── Khởi tạo ─────────────────────────────────────────────
  void init() {
    winW_ = static_cast<float>(window_.getSize().x);
    winH_ = static_cast<float>(window_.getSize().y);
    fadeAlpha_ = 255.f;
    screen_ = MenuScreen::MainMenu;
    active_ = true;
    done_ = false;
    hovered_ = -1;
    selectedChar_ = 0;
    animTime_ = 0.f;
    // chars_ lấy thẳng từ CharacterClass::DEFS — không cần init thêm
  }

  bool isActive() const { return active_; }
  bool isDone() const { return done_; }
  Result getResult() const { return result_; }

  // ── Xử lý sự kiện ────────────────────────────────────────
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
      if (kb->code == sf::Keyboard::Key::Escape) {
        if (screen_ == MenuScreen::CharSelect) screen_ = MenuScreen::MainMenu;
      }
    }
  }

  // ── Update (animation) ────────────────────────────────────
  void update(float dt) {
    if (!active_) return;
    animTime_ += dt;

    // Fade in khi mới vào
    if (fadeAlpha_ > 0.f && screen_ != MenuScreen::FadeOut) {
      fadeAlpha_ = std::max(0.f, fadeAlpha_ - dt * 400.f);
    }

    // Fade out trước khi bắt đầu game
    if (screen_ == MenuScreen::FadeOut) {
      fadeAlpha_ = std::min(255.f, fadeAlpha_ + dt * 300.f);
      if (fadeAlpha_ >= 255.f) {
        active_ = false;
        done_ = true;
      }
    }
  }

  // ── Render ────────────────────────────────────────────────
  void render() {
    if (!active_) return;

    // Nền tối toàn màn
    sf::RectangleShape bg({winW_, winH_});
    bg.setFillColor(sf::Color(15, 12, 20));
    window_.draw(bg);

    // Hạt bụi trang trí (particles giả)
    drawParticles();

    if (screen_ == MenuScreen::MainMenu)
      drawMainMenu();
    else if (screen_ == MenuScreen::CharSelect ||
             screen_ == MenuScreen::FadeOut)
      drawCharSelect();

    // Fade overlay
    if (fadeAlpha_ > 0.f) {
      sf::RectangleShape fade({winW_, winH_});
      fade.setFillColor(sf::Color(0, 0, 0, static_cast<uint8_t>(fadeAlpha_)));
      window_.draw(fade);
    }
  }

 private:
  // ════════════════════════════════════════════════════════
  //  MAIN MENU
  // ════════════════════════════════════════════════════════
  void drawMainMenu() {
    const float cx = winW_ * 0.5f;

    // ── Tiêu đề game ────────────────────────────────────
    float titleY = winH_ * 0.22f;
    float titleBob = std::sin(animTime_ * 1.8f) * 5.f;

    // Glow sau tiêu đề
    sf::CircleShape glow(180.f);
    glow.setFillColor(sf::Color(80, 40, 120, 40));
    glow.setOrigin({180.f, 180.f});
    glow.setPosition({cx, titleY + titleBob});
    window_.draw(glow);

    drawText("VAMPIRE", 54, {cx, titleY - 32.f + titleBob},
             sf::Color(220, 80, 80), true);
    drawText("SURVIVORS", 36, {cx, titleY + 30.f + titleBob},
             sf::Color(180, 140, 200), true);
    drawText("Clone", 16, {cx, titleY + 68.f + titleBob},
             sf::Color(120, 100, 140), true);

    // ── Panel nút ────────────────────────────────────────
    float panelW = 280.f, panelH = 220.f;
    float panelX = cx - panelW * 0.5f;
    float panelY = winH_ * 0.46f;
    drawPanel(panelX, panelY, panelW, panelH);

    // ── Các nút ──────────────────────────────────────────
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
    float btnX = cx - btnW * 0.5f;
    float startY = panelY + 28.f;

    for (int i = 0; i < (int)btns.size(); i++) {
      float by = startY + i * (btnH + 14.f);
      bool hov = (hovered_ == i);
      drawButton(btnX, by, btnW, btnH, btns[i].label, btns[i].col, hov);
      mainMenuRects_.push_back({btnX, by, btnW, btnH});
    }

    // ── Footer ───────────────────────────────────────────
    drawText("Press ESC to quit", 11, {cx, winH_ - 22.f}, sf::Color(80, 70, 90),
             true);
  }

  void drawCharSelect() {
    const float cx = winW_ * 0.5f;
    const int N = 3;  // CharacterClass::DEFS count

    drawText("CHON NHAN VAT", 28, {cx, winH_ * 0.1f}, sf::Color(220, 180, 100),
             true);
    drawText("Chon mot nhan vat de bat dau hanh trinh", 13,
             {cx, winH_ * 0.1f + 36.f}, sf::Color(140, 120, 150), true);

    const float cardW = 190.f, cardH = 290.f, gap = 24.f;
    const float totalW = N * cardW + (N - 1) * gap;
    float startX = cx - totalW * 0.5f;
    float cardY = winH_ * 0.5f - cardH * 0.5f + 10.f;

    charCardRects_.clear();
    for (int i = 0; i < N; i++) {
      float cx_card = startX + i * (cardW + gap);
      bool hov = (hovered_ == 100 + i);
      bool sel = (selectedChar_ == i);
      drawCharCard(cx_card, cardY, cardW, cardH, CharacterClass::DEFS[i], hov,
                   sel);
      charCardRects_.push_back({cx_card, cardY, cardW, cardH});
    }

    float btnW = 200.f, btnH = 48.f;
    float btnX = cx - btnW * 0.5f;
    float btnY = cardY + cardH + 28.f;
    drawButton(btnX, btnY, btnW, btnH, "BAT DAU", sf::Color(220, 120, 60),
               (hovered_ == 200), true);
    startBtnRect_ = {btnX, btnY, btnW, btnH};

    drawText("< Quay lai  (ESC)", 12, {cx, btnY + btnH + 20.f},
             sf::Color(100, 90, 110), true);
  }

  void drawCharCard(float x, float y, float w, float h, const CharClassDef& ch,
                    bool hovered, bool selected) {
    float lift = 0.f;
    if (selected) lift = 8.f;
    if (hovered) lift = std::min(lift + 5.f, 12.f);
    float bob = selected ? std::sin(animTime_ * 2.5f) * 3.f : 0.f;
    y -= (lift + bob);

    sf::Color panelCol =
        selected ? sf::Color(45, 35, 60) : sf::Color(28, 22, 38);
    drawPanel(x, y, w, h, panelCol);

    if (selected || hovered) {
      sf::Color borderCol = selected ? ch.color : sf::Color(100, 90, 120);
      sf::RectangleShape border({w, h});
      border.setFillColor(sf::Color::Transparent);
      border.setOutlineColor(borderCol);
      border.setOutlineThickness(selected ? 2.5f : 1.f);
      border.setPosition({x, y});
      window_.draw(border);
    }

    float cxCard = x + w * 0.5f;

    // Icon vòng tròn
    float iconY = y + 42.f;
    sf::CircleShape iconBg(36.f);
    iconBg.setFillColor(
        sf::Color(ch.color.r / 5, ch.color.g / 5, ch.color.b / 5, 200));
    iconBg.setOutlineColor(sf::Color(ch.color.r, ch.color.g, ch.color.b, 180));
    iconBg.setOutlineThickness(2.f);
    iconBg.setOrigin({36.f, 36.f});
    iconBg.setPosition({cxCard, iconY});
    window_.draw(iconBg);
    drawText(ch.icon, 34, {cxCard, iconY - 17.f}, ch.color, true);

    // Tên
    drawText(ch.name, 18, {cxCard, iconY + 50.f}, sf::Color(230, 220, 240),
             true);

    // Vũ khí khởi đầu — highlight
    sf::RectangleShape wpnBg({w - 20.f, 24.f});
    wpnBg.setFillColor(
        sf::Color(ch.color.r / 6, ch.color.g / 6, ch.color.b / 6, 180));
    wpnBg.setPosition({x + 10.f, iconY + 74.f});
    window_.draw(wpnBg);
    drawText(ch.weaponName, 11, {cxCard, iconY + 82.f}, ch.color, true);

    // Mô tả
    drawTextWrapped(ch.description, 11, x + 12.f, iconY + 108.f, w - 24.f,
                    sf::Color(160, 150, 170));

    // Stats
    float statY = y + h - 58.f;
    sf::RectangleShape statBg({w - 20.f, 50.f});
    statBg.setFillColor(sf::Color(20, 15, 30, 200));
    statBg.setPosition({x + 10.f, statY});
    window_.draw(statBg);
    drawTextWrapped(ch.statLine, 10, x + 14.f, statY + 6.f, w - 28.f,
                    sf::Color(180, 200, 160));
  }

  // ════════════════════════════════════════════════════════
  //  HELPERS
  // ════════════════════════════════════════════════════════
  void drawPanel(float x, float y, float w, float h,
                 sf::Color col = sf::Color(28, 22, 38)) {
    // Shadow
    sf::RectangleShape shadow({w + 8.f, h + 8.f});
    shadow.setFillColor(sf::Color(0, 0, 0, 80));
    shadow.setPosition({x + 4.f, y + 6.f});
    window_.draw(shadow);

    // Panel
    sf::RectangleShape panel({w, h});
    panel.setFillColor(col);
    panel.setOutlineColor(sf::Color(70, 55, 90, 180));
    panel.setOutlineThickness(1.f);
    panel.setPosition({x, y});
    window_.draw(panel);
  }

  void drawButton(float x, float y, float w, float h, const std::string& label,
                  sf::Color accentCol, bool hovered, bool big = false) {
    // Glow khi hover
    if (hovered) {
      sf::RectangleShape glow({w + 12.f, h + 12.f});
      glow.setFillColor(sf::Color(accentCol.r, accentCol.g, accentCol.b, 40));
      glow.setPosition({x - 6.f, y - 6.f});
      window_.draw(glow);
    }

    // Background
    sf::Color bgCol = hovered ? sf::Color(accentCol.r / 2, accentCol.g / 2,
                                          accentCol.b / 2, 220)
                              : sf::Color(35, 28, 48, 220);
    sf::RectangleShape btn({w, h});
    btn.setFillColor(bgCol);
    btn.setOutlineColor(hovered ? accentCol : sf::Color(70, 60, 85));
    btn.setOutlineThickness(hovered ? 1.5f : 1.f);
    btn.setPosition({x, y});
    window_.draw(btn);

    // Label
    sf::Color txtCol = hovered ? sf::Color::White : sf::Color(200, 190, 210);
    unsigned sz = big ? 18u : 15u;
    drawText(label, sz, {x + w * 0.5f, y + h * 0.5f - (big ? 10.f : 8.f)},
             txtCol, true);
  }

  void drawText(const std::string& str, unsigned size, sf::Vector2f pos,
                sf::Color col, bool centered = false) {
    sf::Text txt(font_, str, size);
    txt.setFillColor(col);
    if (centered) {
      auto b = txt.getLocalBounds();
      txt.setOrigin(
          {b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
    }
    txt.setPosition(pos);
    window_.draw(txt);
  }

  void drawTextWrapped(const std::string& str, unsigned size, float x, float y,
                       float maxW, sf::Color col) {
    // Tách từng từ, xuống dòng nếu vượt maxW
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
    float lineH = size * 1.4f;
    int row = 0;
    for (auto& w : words) {
      if (w == "\n") {
        drawText(line, size, {x, y + row * lineH}, col);
        line.clear();
        row++;
        continue;
      }
      std::string test = line.empty() ? w : line + " " + w;
      sf::Text tmp(font_, test, size);
      if (tmp.getLocalBounds().size.x > maxW && !line.empty()) {
        drawText(line, size, {x, y + row * lineH}, col);
        line = w;
        row++;
      } else
        line = test;
    }
    if (!line.empty()) drawText(line, size, {x, y + row * lineH}, col);
  }

  // Hạt bụi lấp lánh nền
  void drawParticles() {
    constexpr int N = 40;
    for (int i = 0; i < N; i++) {
      float phase = (float)i / N;
      float t = std::fmod(animTime_ * 0.3f + phase, 1.f);
      float px = winW_ * (0.1f + phase * 0.82f);
      float py = winH_ * (1.f - t);
      float alpha = std::sin(t * 3.14159f) * 120.f;
      float r = 1.5f + std::sin(phase * 7.3f + animTime_) * 1.f;
      sf::CircleShape p(r);
      p.setFillColor(sf::Color(180, 140, 220, static_cast<uint8_t>(alpha)));
      p.setPosition({px, py});
      window_.draw(p);
    }
  }

  // ── Click / Hover ────────────────────────────────────────
  bool inRect(float mx, float my, float rx, float ry, float rw, float rh) {
    return mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh;
  }

  void updateHover() {
    hovered_ = -1;
    float mx = mousePos_.x, my = mousePos_.y;

    if (screen_ == MenuScreen::MainMenu) {
      for (int i = 0; i < (int)mainMenuRects_.size(); i++) {
        auto& r = mainMenuRects_[i];
        if (inRect(mx, my, r[0], r[1], r[2], r[3])) {
          hovered_ = i;
          break;
        }
      }
    } else if (screen_ == MenuScreen::CharSelect) {
      for (int i = 0; i < (int)charCardRects_.size(); i++) {
        auto& r = charCardRects_[i];
        if (inRect(mx, my, r[0], r[1], r[2], r[3])) {
          hovered_ = 100 + i;
          break;
        }
      }
      auto& sb = startBtnRect_;
      if (inRect(mx, my, sb[0], sb[1], sb[2], sb[3])) hovered_ = 200;
    }
  }

  void handleClick() {
    float mx = mousePos_.x, my = mousePos_.y;

    if (screen_ == MenuScreen::MainMenu) {
      for (int i = 0; i < (int)mainMenuRects_.size(); i++) {
        auto& r = mainMenuRects_[i];
        if (!inRect(mx, my, r[0], r[1], r[2], r[3])) continue;
        if (i == 0) {  // PLAY
          screen_ = MenuScreen::CharSelect;
          hovered_ = -1;
        } else if (i == 1) {  // SETTINGS (stub)
          // TODO: settings screen
        } else if (i == 2) {  // QUIT
          result_.quit = true;
          window_.close();
        }
        break;
      }
    } else if (screen_ == MenuScreen::CharSelect) {
      // Chọn card nhân vật
      for (int i = 0; i < (int)charCardRects_.size(); i++) {
        auto& r = charCardRects_[i];
        if (inRect(mx, my, r[0], r[1], r[2], r[3])) {
          selectedChar_ = i;
          break;
        }
      }
      // Nút START
      auto& sb = startBtnRect_;
      if (inRect(mx, my, sb[0], sb[1], sb[2], sb[3])) {
        result_.charIndex = selectedChar_;
        screen_ = MenuScreen::FadeOut;  // trigger fade out → game start
      }
    }
  }

  // ── Members ──────────────────────────────────────────────
  sf::RenderWindow& window_;
  sf::Font& font_;

  float winW_ = 0.f, winH_ = 0.f;
  float animTime_ = 0.f;
  float fadeAlpha_ = 255.f;
  bool active_ = false;
  bool done_ = false;
  int hovered_ = -1;
  int selectedChar_ = 0;

  MenuScreen screen_ = MenuScreen::MainMenu;
  Result result_;

  std::vector<CharInfo> chars_;
  sf::Vector2f mousePos_;

  // Hit rects: [x, y, w, h]
  std::vector<std::array<float, 4>> mainMenuRects_;
  std::vector<std::array<float, 4>> charCardRects_;
  std::array<float, 4> startBtnRect_ = {};
};
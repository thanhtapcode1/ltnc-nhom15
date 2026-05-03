#pragma once
// ════════════════════════════════════════════════════════════
//  WaveManager.hpp  —  Hệ thống wave quái kiểu Vampire Survivors
//
//  Cách hoạt động:
//    - Theo dõi gameTime (giây)
//    - Mỗi mốc thời gian → kích hoạt 1 WaveEvent đặc biệt
//    - WaveEvent có thể là: horde (đàn lớn), elite (boss nhỏ),
//      surround (bao vây), cross (chữ thập), ring (vòng tròn)
//
//  CÁCH DÙNG trong Game.cpp:
//    waveMgr_.update(dt, gameTime, playerPos, camera_, monsters_);
//    std::string msg = waveMgr_.popMessage();  // hiện thông báo HUD
//
//  THÊM WAVE MỚI: chỉ cần push vào waveScript_ trong buildScript()
// ════════════════════════════════════════════════════════════
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "MonsterManager.hpp"

// ── Kiểu spawn pattern ────────────────────────────────────────
enum class SpawnPattern {
  Horde,     // Nhiều quái từ 1 phía đổ vào
  Surround,  // Bao vây xung quanh player
  Cross,     // 4 cánh chữ thập
  Ring,      // Vòng tròn khép kín
  Line,      // Hàng ngang/dọc đổ vào
};

// ── Định nghĩa 1 wave event ───────────────────────────────────
struct WaveEvent {
  float triggerTime;          // giây từ đầu game
  std::string monsterId;      // "flyeye", "skeleton", ...
  int count;                  // số lượng
  SpawnPattern pattern;       // thông báo hiện trên HUD ("Horde incoming!")
  float spawnRadius = 550.f;  // bán kính spawn so với player
};

class WaveManager {
 public:
  // ── Khởi tạo: đăng ký danh sách monster IDs có trong game ──
  // Gọi sau khi đã registerFactory trong MonsterManager
  void init(const std::vector<std::string>& monsterIds) {
    monsterIds_ = monsterIds;
    buildScript();
  }

  // ── Gọi mỗi frame ────────────────────────────────────────────
  void update(float dt, sf::Vector2f playerPos, const Camera& cam,
              MonsterManager& mm) {
    gameTime_ += dt;
    while (nextWave_ < waveScript_.size() &&
           gameTime_ >= waveScript_[nextWave_].triggerTime) {
      executeWave(waveScript_[nextWave_], playerPos, cam, mm);
      nextWave_++;
    }
  }

  float getGameTime() const { return gameTime_; }

  // Còn bao nhiêu giây đến wave tiếp theo
  std::optional<float> nextWaveIn() const {
    if (nextWave_ >= waveScript_.size()) return std::nullopt;
    float t = waveScript_[nextWave_].triggerTime - gameTime_;
    return t > 0.f ? std::optional<float>(t) : std::nullopt;
  }

  // Số wave đã trigger
  int wavesTriggered() const { return static_cast<int>(nextWave_); }

 private:
  // ════════════════════════════════════════════════════════
  //  WAVE SCRIPT — Chỉnh tại đây để thay đổi timeline
  // ════════════════════════════════════════════════════════
  void buildScript() {
    waveScript_.clear();

    // ── Phút 0: Khởi động nhẹ nhàng ─────────────────────
    push(5, "flyeye", 6, SpawnPattern::Horde, 900.f);
    push(20, "flyeye", 8, SpawnPattern::Surround, 1500.f);
    push(35, "skeleton", 5, SpawnPattern::Line, 900.f);

    // ── Phút 1: Tăng áp lực ──────────────────────────────
    push(60, "flyeye", 12, SpawnPattern::Ring);
    push(75, "skeleton", 8, SpawnPattern::Horde);
    push(90, "flyeye", 10, SpawnPattern::Cross);

    // ── Phút 2: Mini-boss rush ───────────────────────────
    push(120, "skeleton", 15, SpawnPattern::Surround);
    push(135, "flyeye", 15, SpawnPattern::Ring);
    push(150, "skeleton", 10, SpawnPattern::Cross);

    // ── Phút 3: Chaos ────────────────────────────────────
    push(180, "flyeye", 20, SpawnPattern::Horde);
    push(195, "skeleton", 18, SpawnPattern::Ring);
    push(210, "flyeye", 15, SpawnPattern::Cross);
    push(225, "skeleton", 20, SpawnPattern::Surround);

    // ── Phút 4+: Không ngừng nghỉ ───────────────────────
    push(240, "flyeye", 25, SpawnPattern::Ring);
    push(270, "skeleton", 25, SpawnPattern::Horde);
    push(300, "flyeye", 30, SpawnPattern::Surround);

    // Sắp xếp theo thời gian (đề phòng nhập sai thứ tự)
    std::sort(waveScript_.begin(), waveScript_.end(),
              [](const WaveEvent& a, const WaveEvent& b) {
                return a.triggerTime < b.triggerTime;
              });
  }

  void push(float t, const std::string& id, int count, SpawnPattern pat,
            float radius = 320.f) {
    waveScript_.push_back({t, id, count, pat, radius});
  }

  // ── Thực thi 1 wave event ────────────────────────────────
  void executeWave(const WaveEvent& ev, sf::Vector2f playerPos,
                   const Camera& cam, MonsterManager& mm) {
    auto positions = calcPositions(ev, playerPos, cam);
    for (auto& pos : positions) mm.spawn(ev.monsterId, pos);
  }

  // ── Tính vị trí spawn theo pattern ───────────────────────
  std::vector<sf::Vector2f> calcPositions(const WaveEvent& ev,
                                          sf::Vector2f center,
                                          const Camera& cam) const {
    std::vector<sf::Vector2f> pts;
    pts.reserve(ev.count);
    const float R = ev.spawnRadius;
    const int N = ev.count;
    const float PI = 3.14159265f;

    switch (ev.pattern) {
      // ── Horde: đám đông từ 1 cạnh ngẫu nhiên ───────────
      case SpawnPattern::Horde: {
        int side = std::rand() % 4;
        for (int i = 0; i < N; i++) {
          float spread = R * 0.8f;
          float t = (float(i) / N - 0.5f) * spread;
          sf::Vector2f p;
          switch (side) {
            case 0:
              p = {center.x + t, center.y - R};
              break;  // trên
            case 1:
              p = {center.x + t, center.y + R};
              break;  // dưới
            case 2:
              p = {center.x - R, center.y + t};
              break;  // trái
            default:
              p = {center.x + R, center.y + t};
              break;  // phải
          }
          pts.push_back(jitter(p, 20.f));
        }
        break;
      }

      // ── Surround: bao vây ngẫu nhiên khắp xung quanh ───
      case SpawnPattern::Surround: {
        for (int i = 0; i < N; i++) {
          float angle =
              (float(i) / N) * 2.f * PI + randF() * (PI / N);  // jitter góc
          float r = R * (0.85f + randF() * 0.3f);
          pts.push_back(
              {center.x + std::cos(angle) * r, center.y + std::sin(angle) * r});
        }
        break;
      }

      // ── Ring: vòng tròn đều nhau (đáng sợ hơn Surround) ─
      case SpawnPattern::Ring: {
        for (int i = 0; i < N; i++) {
          float angle = (float(i) / N) * 2.f * PI;
          pts.push_back(
              {center.x + std::cos(angle) * R, center.y + std::sin(angle) * R});
        }
        break;
      }

      // ── Cross: 4 cánh chữ thập ──────────────────────────
      case SpawnPattern::Cross: {
        int perArm = std::max(1, N / 4);
        float spacing = R / perArm;
        for (int arm = 0; arm < 4; arm++) {
          sf::Vector2f dir;
          switch (arm) {
            case 0:
              dir = {1.f, 0.f};
              break;  // phải
            case 1:
              dir = {-1.f, 0.f};
              break;  // trái
            case 2:
              dir = {0.f, 1.f};
              break;  // xuống
            default:
              dir = {0.f, -1.f};
              break;  // lên
          }
          for (int k = 0; k < perArm; k++) {
            float dist = spacing * (k + 1) * 0.8f + R * 0.3f;
            sf::Vector2f p = {center.x + dir.x * dist + jitterV().x,
                              center.y + dir.y * dist + jitterV().y};
            pts.push_back(p);
          }
        }
        break;
      }

      // ── Line: hàng ngang hoặc dọc tràn vào ──────────────
      case SpawnPattern::Line: {
        bool horizontal = (std::rand() % 2 == 0);
        float fromSide = (std::rand() % 2 == 0) ? -R : R;
        float spacing = R * 1.6f / N;
        float start = -R * 0.8f;
        for (int i = 0; i < N; i++) {
          float offset = start + spacing * i;
          sf::Vector2f p;
          if (horizontal)
            p = {center.x + fromSide, center.y + offset};
          else
            p = {center.x + offset, center.y + fromSide};
          pts.push_back(jitter(p, 15.f));
        }
        break;
      }
    }
    return pts;
  }

  // ── Utilities ─────────────────────────────────────────────
  static float randF() { return static_cast<float>(std::rand()) / RAND_MAX; }
  static sf::Vector2f jitterV(float range = 25.f) {
    return {(randF() - 0.5f) * range * 2.f, (randF() - 0.5f) * range * 2.f};
  }
  static sf::Vector2f jitter(sf::Vector2f p, float range) {
    return p + jitterV(range);
  }

  // ── State ─────────────────────────────────────────────────
  std::vector<WaveEvent> waveScript_;
  std::vector<std::string> monsterIds_;
  size_t nextWave_ = 0;
  float gameTime_ = 0.f;
};
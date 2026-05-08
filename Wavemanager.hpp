#pragma once
// ════════════════════════════════════════════════════════════
//  WaveManager.hpp  —  Hệ thống wave giống Vampire Survivors
//
//  Cơ chế:
//    - Mỗi phút chuyển sang 1 wave mới (thay đổi bộ quái + quota)
//    - Wave định nghĩa danh sách WaveEntry: { id, minCount, spawnInterval }
//    - MonsterManager tự lo việc duy trì quota theo frame
//
//  Boss:
//    - Spawn đặc biệt theo mốc thời gian cụ thể
//    - Không despawn, bị teleport lại nếu player chạy xa
//    - Có thể drop Treasure Chest (flag isBoss trong KillInfo)
//
//  Map Events (sự kiện bản đồ):
//    - Spawn nhóm lớn ngoài chu kỳ wave bình thường
//    - Dùng pattern: Horde / Surround / Ring / Cross / Line / Sweep
//    - Sweep: đàn quái quét ngang màn hình, chỉ tồn tại ngắn hạn
//
//  CÁCH DÙNG:
//    // Trong Game.cpp constructor:
//    waveMgr_.init(monsters_);
//
//    // Trong Game::update():
//    waveMgr_.update(dt, player_.getPosition(), camera_, monsters_);
//    if (auto msg = waveMgr_.popMessage()) hudMsg_ = *msg;
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

// ── Spawn pattern cho Map Event ──────────────────────────────
enum class MapEventPattern {
  Horde,     // Đám đông từ 1 cạnh màn hình
  Surround,  // Bao vây xung quanh player
  Ring,      // Vòng tròn đều nhau
  Cross,     // 4 cánh chữ thập
  Line,      // Hàng ngang / dọc
  Sweep,     // Làn sóng quét ngang màn hình (di chuyển nhanh)
};

// ── Định nghĩa 1 Map Event ────────────────────────────────────
struct MapEvent {
  float triggerTime;  // giây từ đầu game
  std::string monsterId;
  int count;
  MapEventPattern pattern;
  float radius = 550.f;
  std::string message;  // hiện trên HUD, "" = không hiện
};

// ── Định nghĩa 1 Boss Spawn ───────────────────────────────────
struct BossSpawn {
  float triggerTime;
  std::string bossId;
  sf::Vector2f offset = {0.f, -400.f};  // offset so với player
  std::string message = "BOSS INCOMING!";
};

// ── Định nghĩa 1 Wave (1 phút) ────────────────────────────────
struct WaveDef {
  float startTime;                 // giây bắt đầu wave
  std::vector<WaveEntry> entries;  // bộ quái + quota
  std::string name;                // tên hiện HUD
};

class WaveManager {
 public:
  // ── Khởi tạo: truyền MonsterManager để set wave đầu tiên ──
  void init(MonsterManager& mm) {
    buildWaves();
    buildBossScript();
    buildMapEvents();
    if (!waves_.empty()) mm.setWave(waves_[0].entries);
  }

  // ── Gọi mỗi frame ─────────────────────────────────────────
  void update(float dt, sf::Vector2f playerPos, const Camera& cam,
              MonsterManager& mm) {
    gameTime_ += dt;

    // 1. Chuyển wave theo thời gian
    while (nextWave_ < waves_.size() &&
           gameTime_ >= waves_[nextWave_].startTime) {
      mm.setWave(waves_[nextWave_].entries);
      pendingMessage_ = "Wave: " + waves_[nextWave_].name;
      nextWave_++;
    }

    // 2. Boss spawns
    while (nextBoss_ < bossScript_.size() &&
           gameTime_ >= bossScript_[nextBoss_].triggerTime) {
      const auto& bs = bossScript_[nextBoss_];
      sf::Vector2f bossPos = playerPos + bs.offset;
      mm.spawnDirect(bs.bossId, bossPos, /*isBoss=*/true);
      pendingMessage_ = bs.message;
      nextBoss_++;
    }

    // 3. Map events
    while (nextEvent_ < mapEvents_.size() &&
           gameTime_ >= mapEvents_[nextEvent_].triggerTime) {
      const auto& ev = mapEvents_[nextEvent_];
      // Map event KHÔNG bị chặn bởi MAX_MONSTERS cap (ngoại trừ boss)
      auto positions = calcPositions(ev, playerPos, cam);
      for (auto& pos : positions)
        mm.spawnDirect(ev.monsterId, pos, /*isBoss=*/false);
      if (!ev.message.empty()) pendingMessage_ = ev.message;
      nextEvent_++;
    }
  }

  // Lấy message HUD (consume 1 lần)
  std::optional<std::string> popMessage() {
    if (pendingMessage_.empty()) return std::nullopt;
    auto msg = pendingMessage_;
    pendingMessage_.clear();
    return msg;
  }

  float getGameTime() const { return gameTime_; }

  // Tên wave hiện tại
  std::string currentWaveName() const {
    if (nextWave_ == 0) return "Start";
    return waves_[nextWave_ - 1].name;
  }

  // Giây đến boss tiếp theo
  std::optional<float> nextBossIn() const {
    if (nextBoss_ >= bossScript_.size()) return std::nullopt;
    float t = bossScript_[nextBoss_].triggerTime - gameTime_;
    return t > 0.f ? std::optional<float>(t) : std::nullopt;
  }

 private:
  // ════════════════════════════════════════════════════════
  //  WAVE SCRIPT — Chỉnh tại đây
  //  Mỗi wave: { startTime, { {id, minCount, spawnInterval}, ... }, name }
  //
  //  minCount  = số tối thiểu phải có trong màn tại mọi thời điểm
  //  spawnInterval = giây giữa mỗi lần manager check spawn loại này
  // ════════════════════════════════════════════════════════
  void buildWaves() {
    waves_.clear();

    // Phút 0: Khởi động — chỉ flyeye, nhẹ nhàng
    pushWave(0.f, "Minute 0",
             {
                 {"flyeye", 5, 1.5f},
             });

    // Phút 1: Thêm skeleton
    pushWave(60.f, "Minute 1",
             {
                 {"flyeye", 8, 1.2f},
                 {"skeleton", 4, 2.0f},
             });

    // Phút 2: Tăng áp lực
    pushWave(120.f, "Minute 2",
             {
                 {"flyeye", 10, 1.0f},
                 {"skeleton", 8, 1.5f},
                 {"slime", 4, 2.5f},
             });

    // Phút 3: Chaos
    pushWave(180.f, "Minute 3",
             {
                 {"flyeye", 15, 0.8f},
                 {"skeleton", 12, 1.0f},
                 {"slime", 8, 1.5f},
             });

    // Phút 4: Không ngừng nghỉ
    pushWave(240.f, "Minute 4",
             {
                 {"flyeye", 20, 0.6f},
                 {"skeleton", 18, 0.8f},
                 {"slime", 12, 1.0f},
             });

    // Phút 5+: Đỉnh điểm (gần MAX_MONSTERS)
    pushWave(300.f, "Minute 5+",
             {
                 {"flyeye", 30, 0.5f},
                 {"skeleton", 25, 0.6f},
                 {"slime", 20, 0.8f},
             });

    std::sort(waves_.begin(), waves_.end(),
              [](const WaveDef& a, const WaveDef& b) {
                return a.startTime < b.startTime;
              });
  }

  // ════════════════════════════════════════════════════════
  //  BOSS SCRIPT
  // ════════════════════════════════════════════════════════
  void buildBossScript() {
    bossScript_.clear();

    // Boss skeleton mạnh xuất hiện mỗi 2 phút
    // (dùng id riêng nếu có, hoặc tái dùng "skeleton" với flag boss)
    bossScript_.push_back(
        {120.f, "skeleton", {0.f, -450.f}, "BOSS: SKELETON KING!"});
    bossScript_.push_back({240.f, "flyeye", {0.f, -450.f}, "BOSS: GIANT EYE!"});
    bossScript_.push_back(
        {360.f, "skeleton", {0.f, -450.f}, "BOSS: DEATH KNIGHT!"});

    std::sort(bossScript_.begin(), bossScript_.end(),
              [](const BossSpawn& a, const BossSpawn& b) {
                return a.triggerTime < b.triggerTime;
              });
  }

  // ════════════════════════════════════════════════════════
  //  MAP EVENTS — Sự kiện bản đồ ngoài chu kỳ thường
  // ════════════════════════════════════════════════════════
  void buildMapEvents() {
    mapEvents_.clear();

    // Phút 0
    pushEvent(5.f, "flyeye", 6, MapEventPattern::Horde, 900.f, "");
    pushEvent(20.f, "flyeye", 8, MapEventPattern::Surround, 1500.f,
              "Surrounded!");
    pushEvent(35.f, "skeleton", 5, MapEventPattern::Line, 900.f, "");

    // Phút 1
    pushEvent(60.f, "flyeye", 12, MapEventPattern::Ring, 600.f,
              "Ring of Eyes!");
    pushEvent(75.f, "skeleton", 8, MapEventPattern::Horde, 700.f, "");
    pushEvent(90.f, "flyeye", 10, MapEventPattern::Cross, 600.f, "");

    // Phút 2
    pushEvent(130.f, "skeleton", 15, MapEventPattern::Surround, 700.f,
              "Encircled!");
    pushEvent(150.f, "flyeye", 15, MapEventPattern::Ring, 600.f, "");

    // Phút 3
    pushEvent(180.f, "flyeye", 20, MapEventPattern::Sweep, 800.f,
              "SWARM INCOMING!");
    pushEvent(210.f, "skeleton", 18, MapEventPattern::Ring, 700.f,
              "Death Ring!");

    // Phút 4+
    pushEvent(240.f, "flyeye", 25, MapEventPattern::Sweep, 900.f,
              "MEGA SWARM!");
    pushEvent(270.f, "skeleton", 25, MapEventPattern::Horde, 800.f, "");
    pushEvent(300.f, "flyeye", 30, MapEventPattern::Surround, 900.f,
              "TOTAL SIEGE!");

    std::sort(mapEvents_.begin(), mapEvents_.end(),
              [](const MapEvent& a, const MapEvent& b) {
                return a.triggerTime < b.triggerTime;
              });
  }

  void pushWave(float t, const std::string& name,
                std::vector<WaveEntry> entries) {
    waves_.push_back({t, std::move(entries), name});
  }

  void pushEvent(float t, const std::string& id, int count, MapEventPattern pat,
                 float radius, const std::string& msg) {
    mapEvents_.push_back({t, id, count, pat, radius, msg});
  }

  // ── Tính vị trí spawn theo pattern ───────────────────────
  std::vector<sf::Vector2f> calcPositions(const MapEvent& ev,
                                          sf::Vector2f center,
                                          const Camera& cam) const {
    std::vector<sf::Vector2f> pts;
    pts.reserve(ev.count);
    const float R = ev.radius;
    const int N = ev.count;
    const float PI = 3.14159265f;

    switch (ev.pattern) {
      case MapEventPattern::Horde: {
        int side = std::rand() % 4;
        for (int i = 0; i < N; i++) {
          float spread = R * 0.8f;
          float t = (float(i) / N - 0.5f) * spread;
          sf::Vector2f p;
          switch (side) {
            case 0:
              p = {center.x + t, center.y - R};
              break;
            case 1:
              p = {center.x + t, center.y + R};
              break;
            case 2:
              p = {center.x - R, center.y + t};
              break;
            default:
              p = {center.x + R, center.y + t};
              break;
          }
          pts.push_back(jitter(p, 20.f));
        }
        break;
      }

      case MapEventPattern::Surround: {
        for (int i = 0; i < N; i++) {
          float angle = (float(i) / N) * 2.f * PI + randF() * (PI / N);
          float r = R * (0.85f + randF() * 0.3f);
          pts.push_back(
              {center.x + std::cos(angle) * r, center.y + std::sin(angle) * r});
        }
        break;
      }

      case MapEventPattern::Ring: {
        for (int i = 0; i < N; i++) {
          float angle = (float(i) / N) * 2.f * PI;
          pts.push_back(
              {center.x + std::cos(angle) * R, center.y + std::sin(angle) * R});
        }
        break;
      }

      case MapEventPattern::Cross: {
        int perArm = std::max(1, N / 4);
        float spacing = R / perArm;
        sf::Vector2f dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int arm = 0; arm < 4; arm++) {
          for (int k = 0; k < perArm; k++) {
            float dist = spacing * (k + 1) * 0.8f + R * 0.3f;
            pts.push_back({center.x + dirs[arm].x * dist + jitterV().x,
                           center.y + dirs[arm].y * dist + jitterV().y});
          }
        }
        break;
      }

      case MapEventPattern::Line: {
        bool horiz = (std::rand() % 2 == 0);
        float fromSide = (std::rand() % 2 == 0) ? -R : R;
        float spacing = R * 1.6f / N;
        float start = -R * 0.8f;
        for (int i = 0; i < N; i++) {
          float off = start + spacing * i;
          sf::Vector2f p =
              horiz ? sf::Vector2f{center.x + fromSide, center.y + off}
                    : sf::Vector2f{center.x + off, center.y + fromSide};
          pts.push_back(jitter(p, 15.f));
        }
        break;
      }

      // Sweep: hàng dày từ 1 cạnh, quét nhanh qua màn hình
      // Quái spawn dày sát nhau ở 1 phía, di chuyển sang phía bên kia
      case MapEventPattern::Sweep: {
        int side = std::rand() % 2;  // 0 = trái→phải, 1 = trên→dưới
        float fromSide = -R;
        float spacing = (R * 2.f) / N;
        for (int i = 0; i < N; i++) {
          float off = -R + spacing * i;
          sf::Vector2f p =
              (side == 0) ? sf::Vector2f{center.x + fromSide, center.y + off}
                          : sf::Vector2f{center.x + off, center.y + fromSide};
          pts.push_back(jitter(p, 10.f));
        }
        break;
      }
    }
    return pts;
  }

  // ── Utilities ─────────────────────────────────────────────
  static float randF() { return static_cast<float>(std::rand()) / RAND_MAX; }
  static sf::Vector2f jitterV(float r = 25.f) {
    return {(randF() - 0.5f) * r * 2.f, (randF() - 0.5f) * r * 2.f};
  }
  static sf::Vector2f jitter(sf::Vector2f p, float r) { return p + jitterV(r); }

  // ── State ─────────────────────────────────────────────────
  std::vector<WaveDef> waves_;
  std::vector<BossSpawn> bossScript_;
  std::vector<MapEvent> mapEvents_;

  size_t nextWave_ = 0;
  size_t nextBoss_ = 0;
  size_t nextEvent_ = 0;

  float gameTime_ = 0.f;
  std::string pendingMessage_;
};
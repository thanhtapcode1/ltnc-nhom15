#pragma once
// ════════════════════════════════════════════════════════════
//  MonsterManager.hpp  —  Quản lý toàn bộ quái vật
//
//  Thiết kế: Factory pattern
//    - registerFactory(id, fn) — đăng ký loại quái mới
//    - spawn(id, pos)          — tạo quái theo id
//    - spawnRandom(cam)        — spawn quái ngẫu nhiên trong pool
//
//  CÁCH THÊM QUÁI MỚI (chỉ 2 bước):
//    1. #include "MyNewMonster.hpp"
//    2. monsters_.registerFactory("mynew",
//           [](sf::Vector2f p){ return std::make_unique<MyNewMonster>(p); });
//
//  KHÔNG cần sửa MonsterManager hay bất kỳ class nào khác.
// ════════════════════════════════════════════════════════════
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "IMonster.hpp"

// Forward declare Camera để tránh include nặng
// (thay bằng #include "Camera.hpp" nếu cần)
class Camera;

// KillInfo: thông tin quái vừa chết (dùng để spawn exp orb)
struct KillInfo {
  sf::Vector2f pos;
  int expValue;
  std::string typeId;
};

// Factory function: nhận vị trí, trả về unique_ptr<IMonster>
using MonsterFactory = std::function<std::unique_ptr<IMonster>(sf::Vector2f)>;

class MonsterManager {
 public:
  static constexpr size_t MAX_MONSTERS = 100;
  static constexpr float SPAWN_MARGIN = 80.f;
  static constexpr float SPAWN_INTERVAL_MIN = 0.6f;

  // ── Đăng ký factory ─────────────────────────────────────
  // Ví dụ:
  //   mm.registerFactory("flyeye",
  //       [](sf::Vector2f p){ return std::make_unique<FlyEye>(p); });
  void registerFactory(const std::string& id, MonsterFactory fn) {
    factories_[id] = std::move(fn);
    spawnPool_.push_back(id);  // thêm vào pool spawn ngẫu nhiên
  }

  // Điều chỉnh tỉ lệ xuất hiện (weight) — mặc định = 1
  void setSpawnWeight(const std::string& id, int weight) {
    spawnWeights_[id] = weight;
    rebuildWeightedPool();
  }

  // Giới hạn số lượng tối đa cho từng loại quái (0 = không giới hạn)
  // Ví dụ: mm.setSpawnLimit("flyeye", 10);
  void setSpawnLimit(const std::string& id, int maxCount) {
    maxPerType_[id] = maxCount;
  }

  // Đếm số quái còn sống theo loại
  int countAliveOfType(const std::string& id) const {
    int n = 0;
    for (auto& m : monsters_)
      if (!m->isDead() && m->getTypeId() == id) ++n;
    return n;
  }

  // ── Spawn ────────────────────────────────────────────────
  void spawnInitial(const Camera& cam, int count = 3);

  // Spawn quái cụ thể theo id
  bool spawn(const std::string& id, sf::Vector2f pos) {
    auto it = factories_.find(id);
    if (it == factories_.end()) return false;

    // Kiểm tra giới hạn theo loại
    auto limitIt = maxPerType_.find(id);
    if (limitIt != maxPerType_.end() && limitIt->second > 0) {
      if (countAliveOfType(id) >= limitIt->second) return false;
    }

    monsters_.push_back(it->second(pos));
    return true;
  }

  // ── Update: trả về danh sách quái vừa chết ──────────────
  std::vector<KillInfo> update(float dt, sf::Vector2f playerPos,
                               const Camera& cam);

  // ── Draw ─────────────────────────────────────────────────
  void draw(sf::RenderTarget& target) const;
  void drawDebug(sf::RenderTarget& target) const;

  // ── BulletManager dùng để kiểm tra va chạm ──────────────
  std::vector<IMonster*> getLiveMonsters();

  // ── Player damage: tổng damage từ quái trong frame ──────
  int collectPendingDamage() {
    int total = 0;
    for (auto& m : monsters_) total += m->getAndResetPendingDamage();
    return total;
  }

  // ── Stats ────────────────────────────────────────────────
  int aliveCount() const {
    int n = 0;
    for (auto& m : monsters_)
      if (!m->isDead()) ++n;
    return n;
  }

 private:
  void applySeparation();  // xu li quai chồng lên nhau
  void spawnOne(const Camera& cam);
  void updateSpawner(float dt, const Camera& cam);
  sf::Vector2f randomSpawnPos(const Camera& cam) const;

  void rebuildWeightedPool() {
    weightedPool_.clear();
    for (auto& id : spawnPool_) {
      int w = 1;
      auto wit = spawnWeights_.find(id);
      if (wit != spawnWeights_.end()) w = wit->second;
      for (int i = 0; i < w; ++i) weightedPool_.push_back(id);
    }
  }

  std::unordered_map<std::string, MonsterFactory> factories_;
  std::unordered_map<std::string, int> spawnWeights_;
  std::unordered_map<std::string, int> maxPerType_;  // giới hạn theo loại
  std::vector<std::string> spawnPool_;
  std::vector<std::string> weightedPool_;

  std::vector<std::unique_ptr<IMonster>> monsters_;

  float spawnTimer_ = 0.f;
  float spawnInterval_ = 3.0f;
  float gameTime_ = 0.f;
  int spawnCount_ = 1;
};
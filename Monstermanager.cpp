#include "Monstermanager.hpp"

#include <algorithm>
#include <cstdlib>

#include "Camera.hpp"
#include "Constants.hpp"
#include "MonsterManager.hpp"

using namespace Constants;

// ── Spawn helpers ─────────────────────────────────────────────
sf::Vector2f MonsterManager::randomSpawnPos(const Camera& cam) const {
  const sf::View& view = cam.getView();
  sf::Vector2f center = view.getCenter();
  sf::Vector2f half = view.getSize() / 2.f;

  float left = center.x - half.x - SPAWN_MARGIN;
  float right = center.x + half.x + SPAWN_MARGIN;
  float top = center.y - half.y - SPAWN_MARGIN;
  float bottom = center.y + half.y + SPAWN_MARGIN;

  float mapW = MAP_WIDTH * TILE_RENDER_W;
  float mapH = MAP_HEIGHT * TILE_RENDER_H;
  left = std::max(10.f, left);
  right = std::min(mapW - 10.f, right);
  top = std::max(10.f, top);
  bottom = std::min(mapH - 10.f, bottom);

  int side = std::rand() % 4;
  float x, y;
  switch (side) {
    case 0:
      x = left + (float)(std::rand() % (int)(right - left + 1));
      y = top;
      break;
    case 1:
      x = left + (float)(std::rand() % (int)(right - left + 1));
      y = bottom;
      break;
    case 2:
      x = left;
      y = top + (float)(std::rand() % (int)(bottom - top + 1));
      break;
    default:
      x = right;
      y = top + (float)(std::rand() % (int)(bottom - top + 1));
      break;
  }
  return {x, y};
}

void MonsterManager::spawnOne(const Camera& cam) {
  // Chọn loại quái ngẫu nhiên từ weightedPool_
  if (weightedPool_.empty()) return;
  const std::string& id = weightedPool_[std::rand() % weightedPool_.size()];
  spawn(id, randomSpawnPos(cam));
}

void MonsterManager::spawnInitial(const Camera& cam, int count) {
  for (int i = 0; i < count; ++i) spawnOne(cam);
}

// ── Update ───────────────────────────────────────────────────
void MonsterManager::updateSpawner(float dt, const Camera& cam) {
  gameTime_ += dt;
  spawnTimer_ += dt;
  spawnInterval_ = std::max(SPAWN_INTERVAL_MIN, 3.0f - gameTime_ * 0.01f);

  int calc = 1 + static_cast<int>(gameTime_ / 40.f);
  spawnCount_ = std::min(calc, 5);

  if (spawnTimer_ >= spawnInterval_) {
    spawnTimer_ = 0.f;
    if (monsters_.size() < MAX_MONSTERS) {
      int space = static_cast<int>(MAX_MONSTERS - monsters_.size());
      int numToSpawn = std::min(spawnCount_, space);
      for (int i = 0; i < numToSpawn; ++i) spawnOne(cam);
    }
  }
}

std::vector<KillInfo> MonsterManager::update(float dt, sf::Vector2f playerPos,
                                             const Camera& cam) {
  std::vector<KillInfo> kills;

  for (auto& m : monsters_) {
    bool wasAlive = m->isAlive();
    m->update(dt, playerPos);
    (void)wasAlive;
  }

  // Đẩy quái không chồng nhau — dùng applyOffset trực tiếp
  applySeparation();

  // Xóa quái đã dead hoàn toàn
  monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
                                 [](const std::unique_ptr<IMonster>& m) {
                                   return m->isDead();
                                 }),
                  monsters_.end());

  updateSpawner(dt, cam);
  return kills;
}
void MonsterManager::applySeparation() {
  // ── Vampire Survivors–style flocking separation ──────────────────────────
  // Thay vì đẩy vị trí trực tiếp (gây rung/jitter), ta tích lũy lực vào
  // separationForce_ của mỗi quái. Lực giảm dần theo khoảng cách (linear
  // falloff) và được áp qua flushSeparation() ở cuối frame, kết hợp với
  // velocity damping → quái trượt ra nhẹ nhàng, không giật cục.

  constexpr float DESIRED_DIST = 80.f;  // khoảng cách mong muốn giữa 2 quái
  constexpr float FORCE_MAX = 140.f;    // lực đẩy tối đa (pixels/s)

  auto& list = monsters_;
  for (size_t i = 0; i < list.size(); ++i) {
    if (!list[i]->isAlive()) continue;
    for (size_t j = i + 1; j < list.size(); ++j) {
      if (!list[j]->isAlive()) continue;

      sf::Vector2f d = list[i]->getPosition() - list[j]->getPosition();
      float dist2 = d.x * d.x + d.y * d.y;
      if (dist2 >= DESIRED_DIST * DESIRED_DIST || dist2 < 0.0001f) continue;

      float dist = std::sqrt(dist2);
      // Lực tỉ lệ nghịch với khoảng cách: gần nhau → đẩy mạnh hơn
      float t = 1.f - (dist / DESIRED_DIST);  // [0,1], 1 = chạm nhau
      float forceMag = FORCE_MAX * t * t;     // quadratic falloff → mượt hơn
      sf::Vector2f push = (d / dist) * forceMag;

      list[i]->addSeparationForce(push);
      list[j]->addSeparationForce(-push);
    }
  }

  // Áp separation + integrate velocity cho toàn bộ quái
  // (mỗi quái tự biết maxSpeed của mình qua virtual, nhưng đơn giản ta
  //  dùng 1 giá trị chung — subclass override nếu cần)
  constexpr float DEFAULT_MAX_SPEED = 90.f;
  constexpr float DT_FLUSH = 1.f / 60.f;  // dùng fixed timestep nhỏ để ổn định
  for (auto& m : list) {
    if (!m->isAlive()) continue;
    m->flushSeparation(DT_FLUSH, DEFAULT_MAX_SPEED);
  }
}
// ── Draw ──────────────────────────────────────────────────────
void MonsterManager::draw(sf::RenderTarget& target) const {
  for (auto& m : monsters_)
    if (!m->isDead()) m->draw(target);
}

void MonsterManager::drawDebug(sf::RenderTarget& target) const {
  for (auto& m : monsters_)
    if (!m->isDead()) m->drawDebug(target);
}

std::vector<IMonster*> MonsterManager::getLiveMonsters() {
  std::vector<IMonster*> result;
  result.reserve(monsters_.size());
  for (auto& m : monsters_)
    if (!m->isDead()) result.push_back(m.get());
  return result;
}
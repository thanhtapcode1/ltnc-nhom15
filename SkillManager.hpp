#pragma once
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "GarlicSkill.hpp"
#include "ISkill.hpp"
#include "KnifeSkill.hpp"
#include "LightningRingSkill.hpp"

enum class SkillUpgradeType {
  AttackSpeed,
  Damage,
  Knife,
  LightningRing,
  Garlic,
};

struct SkillUpdateResult {
  std::vector<ShotData> shots;
  std::vector<ZapInfo> zaps;
  std::vector<GarlicHitInfo> garlic;
};

class SkillManager {
 public:
  // ★ Constructor rỗng — KHÔNG tạo sẵn knife nữa
  //   Game::applyCharacterClass() sẽ gọi applyUpgrade() để trang bị đúng skill
  SkillManager() = default;

  SkillUpdateResult update(
      sf::Vector2f playerPos, sf::Vector2f facingDir, sf::Vector2f moveDir,
      float dt, int baseDamage,
      const std::vector<std::pair<sf::Vector2f, void*>>& monsters) {
    SkillUpdateResult result;

    if (knife_) {
      sf::Vector2f dir =
          (moveDir.x != 0.f || moveDir.y != 0.f) ? moveDir : facingDir;
      knife_->setFacingDir(dir);
      auto shots = knife_->tryFire(playerPos, facingDir, dt);
      for (auto& s : shots) s.damageMultiplier = baseDamage;
      result.shots.insert(result.shots.end(), shots.begin(), shots.end());
    }

    if (lightning_) {
      lightning_->tryFire(playerPos, facingDir, dt);
      auto zaps = lightning_->zapMonsters(playerPos, monsters, baseDamage);
      result.zaps.insert(result.zaps.end(), zaps.begin(), zaps.end());
    }

    if (garlic_) {
      garlic_->tryFire(playerPos, facingDir, dt);
      auto hits = garlic_->tickDamage(playerPos, monsters, baseDamage);
      result.garlic.insert(result.garlic.end(), hits.begin(), hits.end());
    }

    return result;
  }

  void drawEffects(sf::RenderTarget& target, sf::Vector2f playerPos) const {
    if (lightning_) lightning_->drawEffects(target);
    if (garlic_) garlic_->drawEffect(target, playerPos);
  }

  void drawDebugRanges(sf::RenderTarget& target, sf::Vector2f pos) const {
    if (lightning_) lightning_->drawRange(target, pos);
    if (garlic_) garlic_->drawDebugRange(target, pos);
  }

  bool applyUpgrade(SkillUpgradeType type) {
    switch (type) {
      case SkillUpgradeType::AttackSpeed:
        if (knife_) knife_->setCooldown(knife_->getCooldown() * 0.85f);
        if (lightning_)
          lightning_->setCooldown(lightning_->getCooldown() * 0.85f);
        if (garlic_) garlic_->setCooldown(garlic_->getCooldown() * 0.85f);
        return true;

      case SkillUpgradeType::Damage:
        return true;

      case SkillUpgradeType::Knife:
        if (!knife_) {
          knife_ = std::make_unique<KnifeSkill>();
          return true;
        }
        if (knife_->isMaxLevel()) return knife_->evolve();
        return knife_->upgrade();

      case SkillUpgradeType::LightningRing:
        if (!lightning_) {
          lightning_ = std::make_unique<LightningRingSkill>();
          return true;
        }
        if (lightning_->isMaxLevel()) return lightning_->evolve();
        return lightning_->upgrade();

      case SkillUpgradeType::Garlic:
        if (!garlic_) {
          garlic_ = std::make_unique<GarlicSkill>();
          return true;
        }
        if (garlic_->isMaxLevel()) return garlic_->evolve();
        return garlic_->upgrade();
    }
    return false;
  }

  bool hasKnife() const { return knife_ != nullptr; }
  bool hasLightning() const { return lightning_ != nullptr; }
  bool hasGarlic() const { return garlic_ != nullptr; }

  int getKnifeLevel() const { return knife_ ? knife_->getInfo().level + 1 : 0; }
  int getLightningLevel() const {
    return lightning_ ? lightning_->getInfo().level + 1 : 0;
  }
  int getGarlicLevel() const {
    return garlic_ ? garlic_->getInfo().level + 1 : 0;
  }

  bool knifeEvolved() const { return knife_ && knife_->isEvolved(); }
  bool lightningEvolved() const {
    return lightning_ && lightning_->isEvolved();
  }
  bool garlicEvolved() const { return garlic_ && garlic_->isEvolved(); }

  bool knifeMaxed() const { return knife_ && knife_->isMaxLevel(); }
  bool lightningMaxed() const { return lightning_ && lightning_->isMaxLevel(); }
  bool garlicMaxed() const { return garlic_ && garlic_->isMaxLevel(); }

  bool garlicCanHeal() const { return garlic_ && garlic_->canHeal(); }

 private:
  std::unique_ptr<KnifeSkill> knife_;
  std::unique_ptr<LightningRingSkill> lightning_;
  std::unique_ptr<GarlicSkill> garlic_;
};
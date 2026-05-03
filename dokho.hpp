#pragma once
// ════════════════════════════════════════════════════════════
//  2 cấp độ khó: Easy / Hard
//
//  moi do kho anh huong:
//    - Số lượng quái tối đa
//    - Tốc độ spawn
//    - HP và damage của quái (hệ số nhân)
//    - Hệ số điểm (ScoreSystem dùng)
// ════════════════════════════════════════════════════════════
#include <string>

#include "ScoreSystem.hpp"  // Difficulty enum

struct DifficultyConfig {
  std::string name;  // "Easy" / "Hard"
  std::string description;

  // Monster spawner
  int maxMonsters;          // tối đa bao nhiêu quái cùng lúc
  float spawnIntervalBase;  // giây giữa mỗi đợt spawn ban đầu
  float spawnIntervalMin;   // giới hạn tốc độ spawn nhanh nhất

  // Monster stat multiplier
  float monsterHpMult;      // nhân HP quái
  float monsterDamageMult;  // nhân damage quái
  float monsterSpeedMult;   // nhân tốc độ quái

  // Player stat
  int playerStartHp;  // HP khởi đầu (trước bonus class)

  // ── 2 preset cố định ─────────────────────────────────────
  static const DifficultyConfig& get(Difficulty d) {
    static const DifficultyConfig EASY{
        "Easy",
        "Danh cho nguoi moi choi. Quai it, chay cham.",
        /*maxMonsters*/ 80,
        /*spawnIntervalBase*/ 3.5f,
        /*spawnIntervalMin*/ 0.8f,
        /*monsterHpMult*/ 1.0f,
        /*monsterDamageMult*/ 1.0f,
        /*monsterSpeedMult*/ 1.0f,
        /*playerStartHp*/ 10,
    };
    static const DifficultyConfig HARD{
        "Hard",
        "Thu thach that su. Quai nhieu, manh va nhanh hon.",
        /*maxMonsters*/ 150,
        /*spawnIntervalBase*/ 2.0f,
        /*spawnIntervalMin*/ 0.4f,
        /*monsterHpMult*/ 1.8f,
        /*monsterDamageMult*/ 1.5f,
        /*monsterSpeedMult*/ 1.3f,
        /*playerStartHp*/ 10,
    };
    return (d == Difficulty::Hard) ? HARD : EASY;
  }
};
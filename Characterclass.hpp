#pragma once
// ════════════════════════════════════════════════════════════
//  CharacterClass.hpp  —  Định nghĩa 3 class nhân vật
//
//  Rogue  : Dao (Knife)         — tốc độ cao
//  Mage   : Sấm (Lightning)     — damage cao
//  Druid  : Tỏi (Garlic)        — HP cao
//
//  MenuSystem đọc DEFS[] để hiển thị card chọn nhân vật.
//  Game::applyCharacterClass() áp stat + skill khởi đầu.
// ════════════════════════════════════════════════════════════
#include <SFML/Graphics.hpp>
#include <string>

// ── Loại skill khởi đầu ──────────────────────────────────────
enum class StartingSkill { Knife, Lightning, Garlic };

// ── Dữ liệu 1 class nhân vật ─────────────────────────────────
struct CharClassDef {
  std::string name;         // Tên hiển thị
  std::string icon;         // Emoji / ký tự icon
  std::string weaponName;   // Tên vũ khí khởi đầu
  std::string description;  // Mô tả ngắn
  std::string statLine;     // Dòng stat hiển thị trong card
  sf::Color color;          // Màu chủ đạo của class

  StartingSkill startSkill;  // Skill được trang bị ngay từ đầu

  // Stat bonus so với base (PlayerStats)
  int bonusHp;       // +HP tối đa
  int bonusDamage;   // +Damage
  float bonusSpeed;  // +Speed (pixels/s)
};

// ── Struct tương thích với MenuSystem cũ ─────────────────────
// (MenuSystem.hpp dùng CharInfo — giữ alias để không cần sửa)
using CharInfo = CharClassDef;

// ════════════════════════════════════════════════════════════
//  DEFS — 3 class cố định, index khớp với selectedChar_
//    0 = Rogue   (Knife)
//    1 = Mage    (Lightning)
//    2 = Druid   (Garlic)
// ════════════════════════════════════════════════════════════
namespace CharacterClass {

inline const CharClassDef DEFS[3] = {
    // ── 0: Rogue ─────────────────────────────────────────
    {
        "Rogue",
        "",
        "  Phong Dao",
        "Sat thu nhanh nhen.\nPhong dao xuyen qua ke thu,\nvut vao bong toi.",
        " HP  8    DMG 1    SPD +30",
        sf::Color(220, 180, 60),  // vàng đồng
        StartingSkill::Knife,
        /*bonusHp=*/-2,
        /*bonusDamage=*/0,
        /*bonusSpeed=*/30.f,
    },

    // ── 1: Mage ──────────────────────────────────────────
    {
        "Mage",
        "",
        "  Set Sam",
        "Phap su thieu dot.\nTia set danh bai ke dich\ntrong mot no phap.",
        " HP 10    DMG 2    SPD  0",
        sf::Color(100, 160, 255),  // xanh điện
        StartingSkill::Lightning,
        /*bonusHp=*/0,
        /*bonusDamage=*/1,
        /*bonusSpeed=*/0.f,
    },

    // ── 2: Druid ─────────────────────────────────────────
    {
        "Druid",
        "",
        "  Vong Toi",
        "Phap su tu nhien.\nMui toi quet sach quan thu\nxung quanh nguoi.",
        " HP 15    DMG 1    SPD -20",
        sf::Color(100, 220, 100),  // xanh lá
        StartingSkill::Garlic,
        /*bonusHp=*/5,
        /*bonusDamage=*/0,
        /*bonusSpeed=*/-20.f,
    },
};

}  // namespace CharacterClass
#pragma once
#include <cstdint>

namespace MapData {

struct TilesetDef {
  const char* filename;
  int firstGid;
};

// co.tsx  → TilesetFloor.png, firstGid=1,   22 cols, 16x16
// TilesetNature.tsx → TilesetNature.png, firstGid=573, 24 cols, 16x16
inline const TilesetDef TILESETS[] = {
    {"TilesetFloor.png", 1},
    {"TilesetNature.png", 573},
};
inline constexpr int TILESET_COUNT = 2;

inline constexpr const char* LAYER_NAMES[] = {"floor", "co"};
inline constexpr int LAYER_COUNT = 2;

inline constexpr int LAYER_IDX_FLOOR = 0;
inline constexpr int LAYER_IDX_CO = 1;

// Map 100×100 = 10000 tiles
inline constexpr int LAYER_SIZE = 100 * 100;

extern const uint32_t LAYER_FLOOR[];
extern const uint32_t LAYER_CO[];

// Không có vật cản
inline const uint32_t* COLLISION_LAYERS[] = {};
inline constexpr int COLLISION_LAYER_COUNT = 0;

}  // namespace MapData
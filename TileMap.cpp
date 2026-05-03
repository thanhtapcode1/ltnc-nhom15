#include "TileMap.hpp"

#include <algorithm>
#include <iostream>

using namespace Constants;

void TileMap::addTileset(const std::string& filename, int firstGid) {
  TilesetInfo info;
  info.filename = filename;
  info.firstGid = firstGid;
  tilesets_.push_back(std::move(info));
}

void TileMap::addLayer(const std::string& name, const uint32_t* data,
                       int size) {
  layers_.push_back({name, data, size, true});
}

bool TileMap::loadTilesets() {
  bool allOk = true;
  for (auto& ts : tilesets_) {
    if (!ts.texture.loadFromFile(ts.filename)) {
      std::cerr << "[TileMap] WARN: cannot load " << ts.filename << '\n';
      ts.columns = 1;
      ts.tileCount = 1;
      allOk = false;
    } else {
      auto sz = ts.texture.getSize();
      // Dùng TILE_W/H (16) để tính số cột trong PNG
      ts.columns = static_cast<int>(sz.x / TILE_W);
      ts.tileCount = ts.columns * static_cast<int>(sz.y / TILE_H);
      std::cout << "[TileMap] Loaded " << ts.filename << "  " << sz.x << "x"
                << sz.y << "  cols=" << ts.columns << "  tiles=" << ts.tileCount
                << '\n';
    }
  }
  return allOk;
}

int TileMap::findTilesetIndex(int gid) const {
  int lo = 0, hi = static_cast<int>(tilesets_.size()) - 1, result = -1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (tilesets_[mid].firstGid <= gid) {
      result = mid;
      lo = mid + 1;
    } else
      hi = mid - 1;
  }
  return result;
}

void TileMap::drawTile(sf::RenderTarget& target, uint32_t rawGid, float destX,
                       float destY) const {
  if (rawGid == 0) return;

  const bool flipH = (rawGid & FLIP_H) != 0;
  const bool flipV = (rawGid & FLIP_V) != 0;
  const bool flipAD = (rawGid & FLIP_AD) != 0;
  const int gid = static_cast<int>(rawGid & TILE_ID_MASK);
  if (gid == 0) return;

  const int tsIdx = findTilesetIndex(gid);
  if (tsIdx < 0) return;

  const TilesetInfo& ts = tilesets_[tsIdx];
  if (ts.columns <= 0) return;

  const int localId = gid - ts.firstGid;
  const int srcCol = localId % ts.columns;
  const int srcRow = localId / ts.columns;

  sf::Sprite sprite(ts.texture);
  // Cắt tile 16x16 từ PNG
  sprite.setTextureRect(
      sf::IntRect(sf::Vector2i(srcCol * TILE_W, srcRow * TILE_H),
                  sf::Vector2i(TILE_W, TILE_H)));

  // Scale x2: tile 16x16 → hiển thị 32x32 trên map
  const float scaleXY =
      static_cast<float>(TILE_RENDER_W) / static_cast<float>(TILE_W);

  float sx = scaleXY, sy = scaleXY, ox = 0.f, oy = 0.f, rot = 0.f;

  if (flipAD) {
    rot = 90.f;
    if (flipH) {
      sy = -scaleXY;
      oy = static_cast<float>(TILE_RENDER_H);
    }
    if (flipV) {
      sx = -scaleXY;
      ox = -static_cast<float>(TILE_RENDER_W);
    }
  } else {
    if (flipH) {
      sx = -scaleXY;
      ox = static_cast<float>(TILE_RENDER_W);
    }
    if (flipV) {
      sy = -scaleXY;
      oy = static_cast<float>(TILE_RENDER_H);
    }
  }

  sprite.setScale({sx, sy});
  sprite.setPosition({destX + ox, destY + oy});

  if (rot != 0.f) {
    sprite.setOrigin({0.f, 0.f});
    sprite.setRotation(sf::degrees(rot));
    sprite.setPosition({destX + static_cast<float>(TILE_RENDER_W), destY});
    sprite.setScale({sx, sy});
  }

  target.draw(sprite);
}

void TileMap::renderLayers(sf::RenderTarget& target, const sf::View& view,
                           int startLayer, int endLayer) const {
  const sf::Vector2f center = view.getCenter();
  const sf::Vector2f size = view.getSize();
  const float left = center.x - size.x / 2.f;
  const float top = center.y - size.y / 2.f;
  const float right = left + size.x;
  const float bottom = top + size.y;

  // Frustum culling dùng TILE_RENDER_W/H (32) cho world coords
  const int tileLeft = std::max(0, static_cast<int>(left / TILE_RENDER_W) - 1);
  const int tileTop = std::max(0, static_cast<int>(top / TILE_RENDER_H) - 1);
  const int tileRight =
      std::min(MAP_WIDTH - 1, static_cast<int>(right / TILE_RENDER_W) + 1);
  const int tileBottom =
      std::min(MAP_HEIGHT - 1, static_cast<int>(bottom / TILE_RENDER_H) + 1);

  endLayer = std::min(endLayer, static_cast<int>(layers_.size()));

  for (int l = startLayer; l < endLayer; ++l) {
    const MapLayer& layer = layers_[l];
    if (!layer.visible || !layer.data) continue;

    for (int ty = tileTop; ty <= tileBottom; ++ty) {
      for (int tx = tileLeft; tx <= tileRight; ++tx) {
        const uint32_t rawGid = layer.data[ty * MAP_WIDTH + tx];
        if (rawGid == 0) continue;
        // destX/Y dùng TILE_RENDER (32) cho world position
        drawTile(target, rawGid, static_cast<float>(tx * TILE_RENDER_W),
                 static_cast<float>(ty * TILE_RENDER_H));
      }
    }
  }
}
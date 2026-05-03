#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

#include "Constants.hpp"

struct TilesetInfo {
  std::string filename;
  int firstGid = 0;
  int tileCount = 0;
  int columns = 0;
  sf::Texture texture;
};

struct MapLayer {
  std::string name;
  const uint32_t* data = nullptr;
  int size = 0;
  bool visible = true;
};

class TileMap {
 public:
  void addTileset(const std::string& filename, int firstGid);
  void addLayer(const std::string& name, const uint32_t* data, int size);
  bool loadTilesets();

  // Render layer [startLayer, endLayer) với frustum culling
  void renderLayers(sf::RenderTarget& target, const sf::View& view,
                    int startLayer, int endLayer) const;

  int layerCount() const { return static_cast<int>(layers_.size()); }

 private:
  std::vector<TilesetInfo> tilesets_;
  std::vector<MapLayer> layers_;

  int findTilesetIndex(int gid) const;
  void drawTile(sf::RenderTarget& target, uint32_t rawGid, float destX,
                float destY) const;
};
#pragma once

#include "map_cell.hpp"
#include "game_map.hpp"

#include <memory>
#include <unordered_map>

using namespace std;

namespace hlt {
    struct CommonFunction {
        static int calculateDistanceWithAnotherDropoff(hlt::Position cellPosition, std::unordered_map<hlt::EntityId, std::shared_ptr<hlt::Dropoff>> dropoffs, std::unique_ptr<hlt::GameMap> &game_map);
        static std::shared_ptr<hlt::Ship> calculateDistanceWithShip(hlt::Position cellPosition, std::unordered_map<hlt::EntityId, std::shared_ptr<hlt::Ship>> ships, std::unique_ptr<hlt::GameMap> &game_map);
        static double nearbyHalite(int x, int y,std::unique_ptr<hlt::GameMap> &game_map);
    };
}
#include "commonFunction.hpp"

//Permet de calculer la distance avec un autre dropoff
int hlt::CommonFunction::calculateDistanceWithAnotherDropoff(hlt::Position cellPosition, std::unordered_map<hlt::EntityId, std::shared_ptr<hlt::Dropoff>> dropoffs, std::unique_ptr<hlt::GameMap> &game_map) {
    int minDistance = 9999;
    
    for (auto it = dropoffs.begin(); it != dropoffs.end(); ++it)
    {
        int tmpDistance = game_map->calculate_distance(cellPosition, it->second->position);
        if (tmpDistance < minDistance) {
            minDistance = tmpDistance;
        }
    }
    return minDistance;
}

//Permet de calculer la distance avec un autre vaisseau
std::shared_ptr<hlt::Ship> hlt::CommonFunction::calculateDistanceWithShip(hlt::Position cellPosition, std::unordered_map<hlt::EntityId, std::shared_ptr<hlt::Ship>> ships, std::unique_ptr<hlt::GameMap> &game_map) {
    int minDistance = 9999;
    std::shared_ptr<hlt::Ship> nearestShip = nullptr;
    for (auto it = ships.begin(); it != ships.end(); ++it)
    {
        int tmpDistance = game_map->calculate_distance(cellPosition, it->second->position);
        if (tmpDistance < minDistance) {
            minDistance = tmpDistance;
            nearestShip = it->second;
        }
    }
    if (minDistance < constants::MIN_SHIP_DIST) {
        return nearestShip;
    } else {
        return nullptr;
    }
}

//Permet de calculer l'halite autour de la case x, y avec un rayon de "NEARBY_HALITE_DIST"
double hlt::CommonFunction::nearbyHalite(int x, int y, std::unique_ptr<hlt::GameMap> &game_map) {
    double nearbyHalite = 0;
    for (int i = x-(constants::NEARBY_HALITE_DIST); i < x+(constants::NEARBY_HALITE_DIST); i++)
    {
        for (int j = y-(constants::NEARBY_HALITE_DIST); j < y+(constants::NEARBY_HALITE_DIST); j++)
        {
            nearbyHalite += game_map->cells[i%game_map->width][j%game_map->height].halite;
        }
    }
    return nearbyHalite;
}
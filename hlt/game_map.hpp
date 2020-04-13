#pragma once

#include "types.hpp"
#include "map_cell.hpp"

#include <vector>
#include <memory>
#include <map>
#include <queue>
#include <limits>
#include <cmath>
#include <stack>

using namespace std;

namespace hlt {

	

	typedef std::vector<std::vector<int>> VVI;
	typedef std::vector<std::vector<int>> VVI;
	typedef std::vector<std::vector<Position>> VVP;

	struct BFSR {
		VVI dist;
		VVP parent;
		VVI turns;
	};

	// represents a single pixel
	struct Node {
	public:
		Position pos;     // index in the flattened grid
		double cost;  // cost of traversing this pixel

		Node(Position i, double c) : pos(i), cost(c) {}
	};

	inline bool operator<(const Node &n1, const Node &n2) {
		return n1.cost > n2.cost;
	}

	inline bool operator==(const Node &n1, const Node &n2) {
		return n1.pos == n2.pos;
	}

    struct GameMap {
        int width;
        int height;
        std::vector<std::vector<MapCell>> cells;

        MapCell* at(const Position& position) {
            Position normalized = normalize(position);
            return &cells[normalized.y][normalized.x];
        }

        MapCell* at(const Entity& entity) {
            return at(entity.position);
        }

        MapCell* at(const Entity* entity) {
            return at(entity->position);
        }

        MapCell* at(const std::shared_ptr<Entity>& entity) {
            return at(entity->position);
        }

        int calculate_distance(const Position& source, const Position& target) {
            const auto& normalized_source = normalize(source);
            const auto& normalized_target = normalize(target);

            const int dx = std::abs(normalized_source.x - normalized_target.x);
            const int dy = std::abs(normalized_source.y - normalized_target.y);

            const int toroidal_dx = std::min(dx, width - dx);
            const int toroidal_dy = std::min(dy, height - dy);

            return toroidal_dx + toroidal_dy;
        }

        Position normalize(const Position& position) {
            const int x = ((position.x % width) + width) % width;
            const int y = ((position.y % height) + height) % height;
            return { x, y };
        }

        std::vector<Direction> get_unsafe_moves(const Position& source, const Position& destination) {
            const auto& normalized_source = normalize(source);
            const auto& normalized_destination = normalize(destination);

            const int dx = std::abs(normalized_source.x - normalized_destination.x);
            const int dy = std::abs(normalized_source.y - normalized_destination.y);
            const int wrapped_dx = width - dx;
            const int wrapped_dy = height - dy;

            std::vector<Direction> possible_moves;

            if (normalized_source.x < normalized_destination.x) {
                possible_moves.push_back(dx > wrapped_dx ? Direction::WEST : Direction::EAST);
            } else if (normalized_source.x > normalized_destination.x) {
                possible_moves.push_back(dx < wrapped_dx ? Direction::WEST : Direction::EAST);
            }

            if (normalized_source.y < normalized_destination.y) {
                possible_moves.push_back(dy > wrapped_dy ? Direction::NORTH : Direction::SOUTH);
            } else if (normalized_source.y > normalized_destination.y) {
                possible_moves.push_back(dy < wrapped_dy ? Direction::NORTH : Direction::SOUTH);
            }

            return possible_moves;
        }

        Direction naive_navigate(std::shared_ptr<Ship> ship, const Position& destination) {
            // get_unsafe_moves normalizes for us
            for (auto direction : get_unsafe_moves(ship->position, destination)) {
                Position target_pos = ship->position.directional_offset(direction);
                if (!at(target_pos)->is_occupied()) {
                    at(target_pos)->mark_unsafe(ship);
                    return direction;
                }
            }

            return Direction::STILL;
        }



		double GameMap::costfn(Ship *s, Position shipyard, Position dest, bool is_1v1);


		


		// weights:        flattened h x w grid of costs
		// h, w:           height and width of grid
		// start, goal:    index of start/goal in flattened grid
		// diag_ok:        if true, allows diagonal moves (8-conn.)
		// paths (output): for each node, stores previous node in path
		
		
		stack<Position> Astar(unique_ptr<GameMap>& game_map, const int h, const int w, const Position start, const Position goal, std::shared_ptr<Ship> ship);
		BFSR BFS(Position source, bool collide = false, int starting_hal = 0);

        void _update();
        static std::unique_ptr<GameMap> _generate();
    };
}


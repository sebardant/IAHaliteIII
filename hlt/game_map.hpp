#pragma once

#include "types.hpp"
#include "map_cell.hpp"

#include <vector>
#include <memory>
#include <map>


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

	typedef std::pair<Position, double> MyPairType;
	struct CompareSecond
	{
		bool operator()(const MyPairType& left, const MyPairType& right) const
		{
			return left.second < right.second;
		}

	};

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



		double GameMap::costfn(Ship *s, int to_cost, Position shipyard, Position dest, bool is_1v1) {

			if (dest == shipyard) return 10000000;

			int halite = at(dest)->halite;
			int turns_to = calculate_distance(s->position, dest);
			int turns_back = calculate_distance(dest, shipyard);

			int turns = fmax(1.0, turns_to + turns_back);
			if (!is_1v1) {
				turns = turns_to + turns_back;
			}

			int curr_hal = s->halite;
			double out = -1000;
			int mined = 0;
			for (int i = 0; i < 5; i++) {
				mined += halite * 0.25;
				halite *= 0.75;
				if (mined + curr_hal > 1000) {
					mined = 1000 - curr_hal;
				}
				int c = std::max(0, mined - to_cost);
				double cout = (c) / ((double)1 + turns + i);
				out = std::max(cout, out);
			}

			return out;
		}

		/*double GameMap::costfn(Ship *s, int to_cost,Position shipyard, Position dest, bool is_1v1) {

			if (dest == shipyard) return 10000000;

			int halite = at(dest)->halite;
			int turns_to = calculate_distance(s->position, dest);
			int turns_back = calculate_distance(dest, shipyard);

			int turns = fmax(1.0, turns_to + turns_back);
			if (!is_1v1) {
				turns = turns_to + turns_back;
			}

			return halite / turns;
		}*/


		pair<Position, double> GameMap::getMin(map<Position, double> mymap)
		{
			pair<Position, double> min
				= *min_element(mymap.begin(), mymap.end(), CompareSecond());
			return min;
		}
		
		

		BFSR BFS(Position source, bool collide = false, int starting_hal = 0);

        void _update();
        static std::unique_ptr<GameMap> _generate();
    };
}


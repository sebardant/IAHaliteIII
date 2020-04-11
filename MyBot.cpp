#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"

#include <random>
#include <ctime>
#include <limits.h> 
#include <stdio.h> 
#include <map>

using namespace std;
using namespace hlt;

int main(int argc, char* argv[]) {
    unsigned int rng_seed;
    if (argc > 1) {
        rng_seed = static_cast<unsigned int>(stoul(argv[1]));
    } else {
        rng_seed = static_cast<unsigned int>(time(nullptr));
    }
    mt19937 rng(rng_seed);

    Game game;
    // At this point "game" variable is populated with initial map data.
    // This is a good place to do computationally expensive start-up pre-processing.
    // As soon as you call "ready" function below, the 2 second per turn timer will start.
    game.ready("MyCppBot");
	map<EntityId, ShipState> stateMp;
	map<EntityId, Position> objectives;
	map<Position, bool> usedAsObjectiv;

	//Aucune des cases n'est un objectif
	for (int i = 0; i < game.game_map->height; i++)
	{
		for (int y = 0; y < game.game_map->width; y++) {
			usedAsObjectiv.insert({ Position(i,y), false });
		}
	}

    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");

    for (;;) {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;
		map<Position, BFSR> ship_to_dist; //des indications de distance et cout par rapport au ship
        vector<Command> command_queue;
		map<Position, double> scoreCases;
		
		ship_to_dist.clear();//On clear a chaque tour 

		for (auto s : me->ships) {//On va remplir pour tout les ship les différentes infos de cout de toute les cases 
			ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
		}

        for (const auto& ship_iterator : me->ships) {
			scoreCases.clear();
            shared_ptr<Ship> ship = ship_iterator.second;
			EntityId id = ship->id;
			bool justChange = false;
			bool still = false;
			bool arrived = ship->position == objectives[id];
			
			if (!stateMp.count(id)) {
				stateMp[id] = GATHERING;
				justChange = true;
			}

			/*if (ship->halite >= constants::MAX_HALITE * 0.90) {
				stateMp[id] = RETURNING;
				justChange = true;
				objectives[id] = me->shipyard->position;
			}*/
			if (arrived && game_map->at(ship->position)->halite == 0) {
				stateMp[id] = RETURNING;
				justChange = true;
				objectives[id] = me->shipyard->position;
			}

			if (ship->halite == 0) {
				stateMp[id] = GATHERING;
				justChange = true;
			}

			switch (stateMp[id])
			{
				case GATHERING:
				{
					if (arrived) {
						still = true;
						if (game_map->at(ship->position)->halite == 0) {
							justChange = true;
						}
					}

					if (justChange) {
						for (int i = 0; i < game_map->cells.size(); i++)
						{
							for (int y = 0; y < game_map->cells[i].size(); y++) {
								scoreCases[game_map->cells[i][y].position] = game_map->costfn(ship.get(), ship_to_dist[ship->position].dist[game_map->cells[i][y].position.x][game_map->cells[i][y].position.y], me->shipyard->position, game_map->cells[i][y].position, true);
							}
						}
						bool good = false;
						pair<Position, double> min;
						while (!good)
						{
							min = game_map->getMin(scoreCases);
							if (usedAsObjectiv[min.first] == false) {
								good = true;
							}
							else {
								scoreCases.erase(min.first);
							}
						}
						objectives[id] = min.first;
						justChange = false;
						still = false;
						arrived = false;
					}
				}
				break;
			}

			if (ship->halite < game_map->at(ship->position)->halite * 0.10) {
				still = true;
			}
			else {
				still = false;
			}

			if (still) {
				command_queue.push_back(ship->stay_still());
			}
			else {
				command_queue.push_back(ship->move(game_map->naive_navigate(ship, objectives[id])));
			}
        }
		
        /*if (
            game.turn_number <= 200 &&
            me->halite >= constants::SHIP_COST &&
            !game_map->at(me->shipyard)->is_occupied())
        {
            command_queue.push_back(me->shipyard->spawn());
        }*/
		if (
			game.turn_number <= 1)
		{
			command_queue.push_back(me->shipyard->spawn());
		}

        if (!game.end_turn(command_queue)) {
            break;
        }
    }

    return 0;
}



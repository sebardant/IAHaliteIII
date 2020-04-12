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
	log::log("BLAH");
	int initialHalite = 0;
    // At this point "game" variable is populated with initial map data.
    // This is a good place to do computationally expensive start-up pre-processing.
    // As soon as you call "ready" function below, the 2 second per turn timer will start.
	for (int i = 0; i < game.game_map->height; i++)
		{
			for (int j = 0; j < game.game_map->width; j++)
			{
				MapCell cell = game.game_map->cells[i][j];
				initialHalite += cell.halite;
			}
			
		}
	log::log("HALITE DE DEPART : ");
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

	

        for (const auto& ship_iterator : me->ships) {
			scoreCases.clear();
            shared_ptr<Ship> ship = ship_iterator.second;
			ship_to_dist[ship->position] = game_map->BFS(ship->position);
			EntityId id = ship->id;
			bool justChange = false;
			
			if (!stateMp.count(id)) {
				stateMp[id] = GATHERING;
				justChange = true;
			}

			if(ship->position == objectives[id]) {
				stateMp[id] = STILL;
			}

			if (stateMp[id] == STILL && game_map->at(ship->position)->halite == 0) {
				stateMp[id] = GATHERING;
				justChange = true;
			}

			if (ship->halite >= constants::MAX_HALITE * 0.90) {
				stateMp[id] = RETURNING;
				usedAsObjectiv[objectives[id]] = false;
				objectives[id] = me->shipyard->position;
			}

			if (ship->halite == 0) {
				stateMp[id] = GATHERING;
				justChange = true;
			}
			map<Position, BFSR> &greedy_bfs = ship_to_dist;

			switch (stateMp[id])
			{
				case GATHERING:
				{
					if (justChange) {
						VVI &dist = greedy_bfs[ship->position].dist;
						for (int i = 0; i < game_map->width; i++)
						{
							for (int y = 0; y < game_map->height; y++) {
								auto dest = Position(i, y);
								int net_cost_to = dist[dest.x][dest.y];

								scoreCases[dest] = game_map->costfn(ship.get(), net_cost_to, me->shipyard->position, dest, true);
							}
						}
						bool good = false;
						Position goodPos = Position(0,0);
						while (!good)
						{
							auto x = std::min_element(scoreCases.begin(), scoreCases.end(),
								[](const pair<Position, double>& p1, const pair<Position, double>& p2) {
								return p1.second > p2.second; });
							if (usedAsObjectiv[x->first] == false) {
								goodPos = x->first;
								good = true;
							}
							else {
								scoreCases.erase(x->first);
							}
						}
						usedAsObjectiv[goodPos] = true;
						objectives[id] = goodPos;
						
						justChange = false;
					}
					command_queue.push_back(ship->move(game_map->naive_navigate(ship, objectives[id])));
				}
				break;
				case STILL:
					command_queue.push_back(ship->stay_still());
					break;
				case RETURNING:
					command_queue.push_back(ship->move(game_map->naive_navigate(ship, objectives[id])));
					break;
			}
        }
		
		//tweakable parameter
		int x = 0.1;
		int y = 2;

		//calcule le nombre de vaisseau et d'halite sur la map
		int thisTurnShips = 1;
		int thisTurnHalite = 0;

		for (int i = 0; i < game_map->height; i++)
		{
			for (int j = 0; j < game_map->width; j++)
			{
				MapCell cell = game_map->cells[i][j];
				if (cell.is_occupied()) {
					thisTurnShips += 1;
				}
				thisTurnHalite += cell.halite;
			}
			
		}
		

        if (
            game.turn_number <= 200 &&
            me->halite >= constants::SHIP_COST &&
            !game_map->at(me->shipyard)->is_occupied() &&
			(y * (thisTurnHalite - x * initialHalite) / thisTurnShips > constants::SHIP_COST)
			)
        {
            command_queue.push_back(me->shipyard->spawn());
        }
	

        if (!game.end_turn(command_queue)) {
            break;
        }
    }

    return 0;
}


/*
bool good = false;
						pair<Position, double> min;

						objectives[id] = min.first;
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


*/
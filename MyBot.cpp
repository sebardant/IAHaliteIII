
#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"
#include "hlt/commonFunction.hpp"

#include <random>
#include <ctime>
#include <limits.h> 
#include <stdio.h> 
#include <map>
#include <stack>

using namespace std;
using namespace hlt;

int main(int argc, char* argv[]) {
	unsigned int rng_seed;
	if (argc > 1) {
		rng_seed = static_cast<unsigned int>(stoul(argv[1]));
	}
	else {
		rng_seed = static_cast<unsigned int>(time(nullptr));
	}
	mt19937 rng(rng_seed);

	Game game;
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
	game.ready("MyCppBot");
	map<EntityId, ShipState> stateMp;
	map<EntityId, Position> objectives;
	map<EntityId, stack<Position>> paths;
	map<Position, bool> usedAsObjectiv;
	map<EntityId, Position> DropOfToBuild;
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

		vector<Command> command_queue;
		map<Position, BFSR> ship_to_dist; //des indications de distance et cout par rapport au ship
		

		ship_to_dist.clear();//On clear a chaque tour 

		for (unordered_map<EntityId, std::shared_ptr<Ship>>::iterator it = me->ships.begin(); it != me->ships.end(); ++it) {
			for (int x = -2; x < 3; x++) {
				for (int y = -2; y < 3; y++) {
					if (x == -2 || y == -2) {
						MapCell* cell = game_map->at(game_map->normalize(Position(it->second->position.x + x, it->second->position.y + y)));
						if (cell->occupied_by_not(me->id)){
							for (int i = -1; i < 2; i++) {
								for (int j = -1; j < 2; j++) {
									MapCell* cell2 = game_map->at(game_map->normalize(Position(cell->position.x + i, cell->position.y + j)));
									cell2->mark_unsafe(cell->ship);
								}
							}
						}
					}
				}
			}
		}

		for (const auto& ship_iterator : me->ships) {
			shared_ptr<Ship> ship = ship_iterator.second;

			EntityId id = ship->id;
			bool justChange = false;

			if (!stateMp.count(id)) {
				stateMp[id] = GATHERING;
				justChange = true;
			}

			if (DropOfToBuild.count(id)) {
				stateMp[id] = DROPOFF;
				usedAsObjectiv[objectives[id]] = false;
				justChange = true;
			}

			if (stateMp[id] == DROPOFF && ship->position == objectives[id]) {
				ship->make_dropoff();
			}

			if (ship->position == objectives[id] && objectives[id] != me->shipyard->position) {
				stateMp[id] = STILL;
			}

			if (stateMp[id] == STILL && game_map->at(ship->position)->halite == 0) {
				stateMp[id] = GATHERING;
				usedAsObjectiv[ship->position] = false;
				justChange = true;
			}

			if (ship->halite >= constants::MAX_HALITE * 0.90) {
				stateMp[id] = RETURNING;
				usedAsObjectiv[ship->position] = false;
				justChange = true;
			}
			
			if (ship->halite == 0 && ship->position == me->shipyard->position) {
				stateMp[id] = GATHERING;
				justChange = true;
			}

			switch (stateMp[id])
			{
				case GATHERING:
				{
					if (justChange) {
						map<Position, double> scoreCases;
						double max = 0.;
						Position bestPos;
						for (int i = 0; i < game_map->height; i++) {
							for (int j = 0; j < game_map->width; j++) {
								scoreCases[Position(i, j)] = game_map->costfn(ship.get(), me->shipyard->position, Position(i, j), true);
								if (scoreCases[Position(i, j)] > max) {
									if (usedAsObjectiv[Position(i, j)] == false) {
										max = scoreCases[Position(i, j)];
										bestPos = Position(i, j);
									}
								}
							}
						}
						objectives[id] = bestPos;
						usedAsObjectiv[bestPos] = true;
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
						justChange = false;
					}
					else if(paths[id].empty()){
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
					}
					else if(game_map->at(paths[id].top())->is_occupied()){
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
					}


					//--------------------------------------------------------------------------------------------------------------
					Direction newDir = Direction::STILL;
					if (!paths[id].empty()) {
						Position nextPos = paths[id].top();

						auto& normalized_source = game_map->normalize(ship->position);
						auto& normalized_destination = game_map->normalize(nextPos);

						int dx = std::abs(normalized_source.x - normalized_destination.x);
						int dy = std::abs(normalized_source.y - normalized_destination.y);
						int wrapped_dx = game_map->width - dx;
						int wrapped_dy = game_map->height - dy;

						std::vector<Direction> possible_moves;

						if (normalized_source.x < normalized_destination.x) {
							newDir = dx > wrapped_dx ? Direction::WEST : Direction::EAST;
						}
						else if (normalized_source.x > normalized_destination.x) {
							newDir = dx < wrapped_dx ? Direction::WEST : Direction::EAST;
						}

						if (normalized_source.y < normalized_destination.y) {
							newDir = dy > wrapped_dy ? Direction::NORTH : Direction::SOUTH;
						}
						else if (normalized_source.y > normalized_destination.y) {
							newDir = dy < wrapped_dy ? Direction::NORTH : Direction::SOUTH;
						}

						//log::log(" GATHERING --- SHIP ID= " + to_string(id) + " ACTUAL -> X: " + to_string(ship->position.x) + " Y:" + to_string(ship->position.y) + " ---- NEXT -> X: " + to_string(nextPos.x) + " Y:" + to_string(nextPos.y) +" ---- DIRECTION = " + to_string(dir));
						
						if (ship->halite >= game_map->at(ship->position)->halite * 0.10) {
							command_queue.push_back(ship->move(newDir));
							game_map->at(nextPos)->mark_unsafe(ship);
							paths[id].pop();
						}
					}else {
						justChange = true;
					}
				}
				break;
				case STILL:
					command_queue.push_back(ship->stay_still());
					break;

				case DROPOFF:
				{
					if (justChange) {
						objectives[id] = DropOfToBuild[id];
						DropOfToBuild.erase(id);
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
						justChange = false;
					}
					else if (paths[id].empty()) {
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
					}
					else if (game_map->at(paths[id].top())->is_occupied()) {
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
					}

					Direction newDir = Direction::STILL;
					if (!paths[id].empty()) {
						if (game_map->at(paths[id].top())->is_occupied())
						{
							stack<Position>().swap(paths[id]);
							paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
						}
						if (!paths[id].empty()) {

							Position nextPos = paths[id].top();

							auto& normalized_source = game_map->normalize(ship->position);
							auto& normalized_destination = game_map->normalize(nextPos);

							int dx = std::abs(normalized_source.x - normalized_destination.x);
							int dy = std::abs(normalized_source.y - normalized_destination.y);
							int wrapped_dx = game_map->width - dx;
							int wrapped_dy = game_map->height - dy;

							std::vector<Direction> possible_moves;

							if (normalized_source.x < normalized_destination.x) {
								newDir = dx > wrapped_dx ? Direction::WEST : Direction::EAST;
							}
							else if (normalized_source.x > normalized_destination.x) {
								newDir = dx < wrapped_dx ? Direction::WEST : Direction::EAST;
							}

							if (normalized_source.y < normalized_destination.y) {
								newDir = dy > wrapped_dy ? Direction::NORTH : Direction::SOUTH;
							}
							else if (normalized_source.y > normalized_destination.y) {
								newDir = dy < wrapped_dy ? Direction::NORTH : Direction::SOUTH;
							}

							log::log(" RETURNING --- SHIP ID= " + to_string(id) + " ACTUAL -> X: " + to_string(ship->position.x) + " Y:" + to_string(ship->position.y) + " ---- NEXT -> X: " + to_string(nextPos.x) + " Y:" + to_string(nextPos.y));// +" ---- DIRECTION = " + to_string(newDir));
							if (ship->halite >= game_map->at(ship->position)->halite * 0.10) {
								command_queue.push_back(ship->move(newDir));
								game_map->at(nextPos)->mark_unsafe(ship);
								paths[id].pop();
							}
						}
					}
				}
				break;
				case RETURNING: 
				{
					if (justChange) {
						objectives[id] = me->shipyard->position;
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
						justChange = false;
					}
					else if (paths[id].empty()) {
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
					}
					else if (game_map->at(paths[id].top())->is_occupied()) {
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
					}
					
					Direction newDir = Direction::STILL;
					if (!paths[id].empty()) {
						if (game_map->at(paths[id].top())->is_occupied())
						{
							stack<Position>().swap(paths[id]);
							paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
						}
						if (!paths[id].empty()) {
							
							Position nextPos = paths[id].top();

							auto& normalized_source = game_map->normalize(ship->position);
							auto& normalized_destination = game_map->normalize(nextPos);

							int dx = std::abs(normalized_source.x - normalized_destination.x);
							int dy = std::abs(normalized_source.y - normalized_destination.y);
							int wrapped_dx = game_map->width - dx;
							int wrapped_dy = game_map->height - dy;

							std::vector<Direction> possible_moves;

							if (normalized_source.x < normalized_destination.x) {
								newDir = dx > wrapped_dx ? Direction::WEST : Direction::EAST;
							}
							else if (normalized_source.x > normalized_destination.x) {
								newDir = dx < wrapped_dx ? Direction::WEST : Direction::EAST;
							}

							if (normalized_source.y < normalized_destination.y) {
								newDir = dy > wrapped_dy ? Direction::NORTH : Direction::SOUTH;
							}
							else if (normalized_source.y > normalized_destination.y) {
								newDir = dy < wrapped_dy ? Direction::NORTH : Direction::SOUTH;
							}

							log::log(" RETURNING --- SHIP ID= " +to_string(id) + " ACTUAL -> X: " + to_string(ship->position.x) + " Y:" + to_string(ship->position.y) + " ---- NEXT -> X: " + to_string(nextPos.x) + " Y:" + to_string(nextPos.y));// +" ---- DIRECTION = " + to_string(newDir));
							if (ship->halite >= game_map->at(ship->position)->halite * 0.10) {
								command_queue.push_back(ship->move(newDir));
								game_map->at(nextPos)->mark_unsafe(ship);
								paths[id].pop();
							}
						}
					}
					//command_queue.push_back(ship->move(newDir));
				}
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
				//Opération pour un dropoff, on profite du parcours de la map pour le faire ici et ne pas avoir a reparcourir la map
				if (game.turn_number <= constants::MAX_TURNS * constants::DROPOFF_TURNS) {
					if (me->ships.size() >= constants::SHIPS_PER_DROPOFF*(me->dropoffs.size()+1)) {
						int distanceWithAnotherDropoff = hlt::CommonFunction::calculateDistanceWithAnotherDropoff(game_map->cells[i][j].position, me->dropoffs, game_map);
						if (distanceWithAnotherDropoff > constants::MIN_DROPOFF_DIST) {
							std::shared_ptr<hlt::Ship> nearestShip = hlt::CommonFunction::calculateDistanceWithShip(game_map->cells[i][j].position, me->ships, game_map);
							if (nearestShip != nullptr) {
								double nearbyHalite = hlt::CommonFunction::nearbyHalite(i, j, game_map);
								if (nearbyHalite > constants::NEARBY_HALITE_NEEDED) {
									int costOfConstruction = constants::DROPOFF_COST - game_map->cells[i][j].halite - nearestShip->halite;
									if (me->halite > costOfConstruction) {
										DropOfToBuild[nearestShip->id] = game_map->cells[i][j].position;
									}
								}
							}
						}
					}
				}
			}
		}

		if (game.turn_number <= 200 &&
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
/*#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"

#include <random>
#include <ctime>
#include <limits.h> 
#include <stdio.h> 
#include <map>
#include <stack>

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
    game.ready("MyCppBot");
	map<EntityId, ShipState> stateMp;
	map<EntityId, Position> objectives;
	map<EntityId, stack<Position>> paths;
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
		
		vector<Command> command_queue;
		map<Position, BFSR> ship_to_dist; //des indications de distance et cout par rapport au ship
		map<Position, double> scoreCases;
		
		ship_to_dist.clear();//On clear a chaque tour 

	
		
        for (const auto& ship_iterator : me->ships) {
			scoreCases.clear();
            shared_ptr<Ship> ship = ship_iterator.second;
			
			EntityId id = ship->id;
			bool justChange = false;

			if (!stateMp.count(id)) {
				stateMp[id] = GATHERING;
				justChange = true;
			}

			if (ship->position == objectives[id] && objectives[id] != me->shipyard->position) {
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
				justChange = true;
				paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
			}

			if (ship->halite == 0) {
				stateMp[id] = GATHERING;
				justChange = true;
			}
		
			switch (stateMp[id])
			{
				case GATHERING:
				{
					if (justChange) {
						ship_to_dist[ship->position] = game_map->BFS(ship->position);
						map<Position, BFSR> &greedy_bfs = ship_to_dist;
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
							auto x = std::max_element(scoreCases.begin(), scoreCases.end(),
								[](const pair<Position, double>& p1, const pair<Position, double>& p2) {
								return p1.second < p2.second; });
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
						
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
						log::log("OBJECTIV SET = X: "+ to_string(objectives[id].x)+" -- Y: "+ to_string(objectives[id].y));
						justChange = false;
					}
					else if (paths[id].empty()) {
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
					}

					Direction newDir = Direction::STILL;

					if (!paths[id].empty()) {
						Position nextPos = paths[id].top();
						if (game_map->at(nextPos)->is_occupied())
						{
							paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
							nextPos = paths[id].top();
						}

						paths[id].pop();
						game_map->at(nextPos)->mark_unsafe(ship);


						if (nextPos.x < ship->position.x) {
							newDir = Direction::WEST;
						}
						if (nextPos.x > ship->position.x) {
							newDir = Direction::EAST;
						}
						if (nextPos.y < ship->position.y) {
							newDir = Direction::NORTH;
						}
						if (nextPos.y > ship->position.y) {
							newDir = Direction::SOUTH;
						}
					}
					command_queue.push_back(ship->move(newDir));
					//command_queue.push_back(ship->move(game_map->naive_navigate(ship, objectives[id])));


				}
				break;
				case STILL:
					command_queue.push_back(ship->stay_still());
					break;
				case RETURNING:
					if (paths[id].empty() && !justChange) {
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
					}
					justChange = false;
					Direction newDir = Direction::STILL;
					if (!paths[id].empty()) {
						Position nextPos = paths[id].top();
						if (game_map->at(nextPos)->is_occupied())
						{
							paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);
							nextPos = paths[id].top();
						}

						paths[id].pop();
						game_map->at(nextPos)->mark_unsafe(ship);


						if (nextPos.x < ship->position.x) {
							newDir = Direction::WEST;
						}
						if (nextPos.x > ship->position.x) {
							newDir = Direction::EAST;
						}
						if (nextPos.y < ship->position.y) {
							newDir = Direction::NORTH;
						}
						if (nextPos.y > ship->position.y) {
							newDir = Direction::SOUTH;
						}
					}
					command_queue.push_back(ship->move(newDir));
					//command_queue.push_back(ship->move(game_map->naive_navigate(ship, objectives[id])));
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
				
				//Opération pour un dropoff, on profite du parcours de la map pour le faire ici et ne pas avoir a reparcourir la map
				if (game.turn_number <= constants::MAX_TURNS * constants::DROPOFF_TURNS) {
					if (me->ships.size() >= constants::SHIPS_PER_DROPOFF*(me->dropoffs.size()+1)) {
						int distanceWithAnotherDropoff = hlt::Dropoff::calculateDistanceWithAnotherDropoff(i, j, me->dropoffs);
						if (distanceWithAnotherDropoff > constants::MIN_DROPOFF_DIST) {
							std::shared_ptr<hlt::Ship> nearestShip = hlt::Dropoff::calculateDistanceWithShip(i, j, me->ships);
							if (nearestShip != nullptr) {
								double nearbyHalite = hlt::CommonFunction::nearbyHalite(i, j, game_map->cells);
								if (nearbyHalite > constants::NEARBY_HALITE_NEEDED) {
									int costOfConstruction = constants::DROPOFF_COST - game_map->cells[i][j].halite - nearestShip->halite;
									if (me->halite > costOfConstruction) {
										nearestShip->make_dropoff();
									}
								}
							}
						}
					}
				}
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
*/
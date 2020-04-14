
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

	map<Position, bool> usedAsObjectiv; // Un tableau nous permettant de savoir si une case est l'objectif d'un ship

	// At this point "game" variable is populated with initial map data.
	// This is a good place to do computationally expensive start-up pre-processing.
	// As soon as you call "ready" function below, the 2 second per turn timer will start.
	for (int i = 0; i < game.game_map->height; i++)
	{
		for (int j = 0; j < game.game_map->width; j++)
		{
			MapCell cell = game.game_map->cells[i][j];
			initialHalite += cell.halite;
			usedAsObjectiv.insert({ Position(i,j), false });//Au départ, aucune case n'est un objectif
		}

	}
	game.ready("Seb-Victor-BOT");
	map<EntityId, ShipState> stateMp;//Tableau contenant les états de tous les ships (Gathering, Still, Returning et DropOff)
	map<EntityId, Position> objectives;//Tableau contenant les objectifs de tous les ships
	map<EntityId, stack<Position>> paths;//Tableau contenant les chemins de tous les ships
	map<EntityId, Position> dropOfToBuild;//Tableau contenant tous les dropoff étant à construire et les ship censé les construire
	log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");

	for (;;) {
		game.update_frame();
		shared_ptr<Player> me = game.me;
		unique_ptr<GameMap>& game_map = game.game_map;
		vector<Command> command_queue;
		
		/*
		Cette boucle permet de marquer les cases "dangereuses" proches des ships ennemis étant à un certains rayon d'un ship allié.

				X X X X X
				X 0 0 0 X
				X 0 S 0 X
				X 0 0 0 X
				X X X X X

		X étant les cases à check pour la présence d'ennemis, S le ship allié
				---------------------------------------------------------------------
				  V V V 
				  V E V
				  V V V

		Si un ennemis est détecté, toute la zone V autour de E est marqué comme unsafe

		PS: Cette fonctionnalité n'est pas très fonctionnel. En effet cela peut cause un disfonctionnement dans le pathfinding et qui bloque les ships.
		*/
		if (game_map->height > 32) {
			for (const auto& it : me->ships) {
				for (int x = -2; x < 3; x++) {
					for (int y = -2; y < 3; y++) {
						if (x == -2 || y == -2 || x == 2 || y == 2) {
							MapCell* cell = game_map->at(game_map->normalize(Position(it.second->position.x + x, it.second->position.y + y)));
							if (cell->occupied_by_not(me->id)) {
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
		}

		// On va maintenant itérer sur chacun des ships alliés afin de déterminer leur action du tour
		for (const auto& ship_iterator : me->ships) {
			shared_ptr<Ship> ship = ship_iterator.second;
			EntityId id = ship->id;

			bool justChange = false; //Permet de savoir si l'état du ship vient tout juste de changer.

			if (!stateMp.count(id)) { // Si le ship est nouveau, alors on lui applique le statut GATHERING
				stateMp[id] = GATHERING;
				justChange = true; 
			}

			if (dropOfToBuild.count(id)) {// Si le ship est nouveau, alors on lui applique le statut DropOff
				stateMp[id] = DROPOFF;
				usedAsObjectiv[objectives[id]] = false; // on oublie pas de passer son objectifs précédent à false dans usedAsObjectives. La position devient alors redisponible comme objectif
				justChange = true;
			}

			if (stateMp[id] == DROPOFF && ship->position == objectives[id]) {// Si le ship avait le statut DROPOFF et qu'il est présent sur son objectif
				ship->make_dropoff();// alors on transforme le ship en dropoff
			}

			if (ship->position == objectives[id] && objectives[id] != me->shipyard->position) {// Si un ship est arrivé sur son objectif et que ce n'est pas le shipyard, alors on le passe au statut MINE
				stateMp[id] = MINE;
			}

			if (stateMp[id] == MINE && game_map->at(ship->position)->halite == 0) {// Si le ship était en statut MINE et que la case sur laquel il est n'a plus d'halite, on repasse en statut GATHERING
				stateMp[id] = GATHERING;
				usedAsObjectiv[ship->position] = false;
				justChange = true;
			}

			if (ship->halite >= constants::MAX_HALITE * 0.90) {// Si le ship à 90% ou plus de la quantité maximum qu'il peut stocker, on le passe au statut RETURNING 
				stateMp[id] = RETURNING;
				usedAsObjectiv[ship->position] = false;
				justChange = true;
			}
			
			if (ship->halite == 0 && ship->position == me->shipyard->position) {// Si le ship est sur le shipyard et qu'il stock 0 d'halite, on passe en GATHERING
				stateMp[id] = GATHERING;
				justChange = true;
			}

			switch (stateMp[id])// On va maintenant déterminer ce que les ships vont faire selon leur statut
			{
				case GATHERING:
				{
					/*
					Si le ship vient tout juste de passer à GATHERING, on va faire 3 choses
						1. Donner un score d'attractivité à chaque case de la carte (avec ScoreCell() )
						2. Choisir la case la plus attractive (celle avec le score le plus élevé)
						3. Calculer le chemin pour accéder à cette case avec un algorithme a* (Astar())
					*/
					if (justChange) { 
						map<Position, double> scoreCases;
						double max = 0.;
						Position bestPos;
						for (int i = 0; i < game_map->height; i++) {
							for (int j = 0; j < game_map->width; j++) {
								scoreCases[Position(i, j)] = game_map->ScoreCell(ship.get(), me->shipyard->position, Position(i, j));
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
					else if(paths[id].empty()){ // Dans le cas ou on ne vient pas tout juste de passer au statut GATHERING mais que notre chemin pour aller à l'objectif est vide
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);//alors on recherche un nouveau chemin
					}
					else if(game_map->at(paths[id].top())->is_occupied()){ //Si la prochaine case de notre chemin est occupé,
						stack<Position>().swap(paths[id]);
						paths[id] = game_map->Astar(game_map, game_map->height, game_map->width, ship->position, objectives[id], ship);//alors on recherche un nouveau chemin
					}

					/*
					On va maintenant choisir la prochaine direction à envoyer dans la queue de commande.
					Par defaut la direction est STILL
					*/
					Direction newDir = Direction::STILL;

					if (!paths[id].empty()) { // On revérifie encore que notre path n'est pas vide
						Position nextPos = paths[id].top();// On récupère le top de notre pile de position
						
						newDir = game_map->DirectionToGo(ship->position, nextPos);//On détermine la nouvelle direction 

						if (ship->halite >= game_map->at(ship->position)->halite * 0.10) {//On va check si le ship possède assez d'ahlite pour se déplacer
							command_queue.push_back(ship->move(newDir));//On push la commande 
							game_map->at(nextPos)->mark_unsafe(ship);//On marque la destination comme unsafe pour les prochains ships
							paths[id].pop(); //On sort la destination de la pile du chemin.
						}
					}
				}
				break;
				case MINE:// Si le statut est MINE alors on reste sur la case
					command_queue.push_back(ship->stay_still());
				break;

				case DROPOFF: 
				{
					/*Si le ship vient tout juste de passer à DROPOFF, on va faire 2 choses
						1. Donner au ship comme objectif la position du dropoff correspondant
						3. Calculer le chemin pour accéder à cette case avec un algorithme a* (Astar())

						Après le if(justChange) c'est à peu près la même chose que dans gathering
					*/	
					if (justChange) {
						objectives[id] = dropOfToBuild[id];
						dropOfToBuild.erase(id);
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

							newDir = game_map->DirectionToGo(ship->position, nextPos);

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
					/*Si le ship vient tout juste de passer à RETURNING, on va faire 2 choses
						1. Donner au ship comme objectif la position du shipyard ou du dropOff le plus proche
						3. Calculer le chemin pour accéder à cette case avec un algorithme a* (Astar())

						Après le if(justChange) c'est à peu près la même chose que dans gathering
					*/
					if (justChange) {
						Position closestDropOff = me->shipyard->position;
						std::vector<std::vector<int>> ship_to_dist = game_map->BreadthFirstSearch(ship->position);
						for (const auto& it : me->dropoffs) {
							if (ship_to_dist[it.second->position.x][it.second->position.y] < ship_to_dist[closestDropOff.x][closestDropOff.y]) {
								closestDropOff = it.second->position;
							}
						}
						objectives[id] = closestDropOff;
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

							newDir = game_map->DirectionToGo(ship->position, nextPos);

							if (ship->halite >= game_map->at(ship->position)->halite * 0.10) {
								command_queue.push_back(ship->move(newDir));
								game_map->at(nextPos)->mark_unsafe(ship);
								paths[id].pop();
							}
						}
					}
				}
				break;
			}
		}

		/*
		La prise de décision pour le spawn des ships et le placements des dropoff ne sont pas au point. En effet, nous avons voulu nous inspirer du gagnant de HALITE 3 pour la stratégi.
		Sa stratégie prend en compte des variables a tweaker (x et y) par exemple. Or, même après pleins d'essais il nous était impossible de trouver des valeurs correct. Il nous aurait fallu
		intégrer à notre code un algorithme génétique afin de tester pleins de combinaison possible et avoir la meilleure. Cette implémentation n'est donc pas très utile dans notre code. Nous nous en 
		sommes rendu trop tard.
		*/

		//Pramètre modifiable pour changer le taux de spawn
		int x = 0.1;
		int y = 2;

		//stock le nombre de vaisseau et d'halite sur la map
		int thisTurnShips = 1;
		int thisTurnHalite = 0;
		//calcule le nombre de vaisseau et d'halite sur la map
		for (int i = 0; i < game_map->height; i++)
		{
			for (int j = 0; j < game_map->width; j++)
			{
				MapCell cell = game_map->cells[i][j];
				if (cell.is_occupied()) { //Si la case est occupé c'est qu'elle contien ou un vaisseau
					thisTurnShips += 1;
				}
				thisTurnHalite += cell.halite; //On ajoute le nombre de halite de la case au total calculé.
				
				//Opération pour un dropoff, on profite du parcours de la map pour le faire ici et ne pas avoir a reparcourir la map
				if (game.turn_number <= constants::MAX_TURNS * constants::DROPOFF_TURNS) { // On vérifie qu'il n'est pas trop tard dans la game
					if (me->ships.size() >= constants::SHIPS_PER_DROPOFF*(me->dropoffs.size()+1)) { //On vérifie que l'on dispose d'un nombre de vaisseau intéressant
						
						// On vérifie qu'il n'y a pas un autre dropoff proche
						int distanceWithAnotherDropoff = hlt::CommonFunction::calculateDistanceWithAnotherDropoff(game_map->cells[i][j].position, me->dropoffs, game_map);
						if (distanceWithAnotherDropoff > constants::MIN_DROPOFF_DIST) { // On vérifie qu'il n'y a pas un autre dropoff proche

							// On cherche le vaisseau le plus proche
							std::shared_ptr<hlt::Ship> nearestShip = hlt::CommonFunction::calculateDistanceWithShip(game_map->cells[i][j].position, me->ships, game_map);
							if (nearestShip != nullptr) {

								//On calcule l'halite proche du point potentiel pour calculer si oui ou non cette case est intéressante
								double nearbyHalite = hlt::CommonFunction::nearbyHalite(i, j, game_map);
								if (nearbyHalite > constants::NEARBY_HALITE_NEEDED) {

									//On vérifie qu'on a l'halite nécessaire pour construire un dropoff, en soustrayant le cout du dropoff avec l'halite de la case et celle du bateau
									int costOfConstruction = constants::DROPOFF_COST - game_map->cells[i][j].halite - nearestShip->halite;
									if (me->halite > costOfConstruction) {
										
										//Tout est ok, on envois le bateau le plus proche sur la case valide pour qu'il construise un dropoff
										dropOfToBuild[nearestShip->id] = game_map->cells[i][j].position;
									}
								}
							}
						}
					}
				}
			}
		}

		//Logique de spawn
		if (game.turn_number < constants::MAX_TURNS * constants::SPAWN_TURNS && //On vérifie qu'il n'es pas trop tard dans la game
			me->halite >= constants::SHIP_COST && //On vérifie qu'on a l'halite nécessaire
			!game_map->at(me->shipyard)->is_occupied() && // On vérifie que le port n'est pas occupé
			(y * (thisTurnHalite - x * initialHalite) / thisTurnShips > constants::SHIP_COST) //On regarde en fonction de l'halite restante sur la map et du nombre de vaisseau total dans la map
			)
		{
			command_queue.push_back(me->shipyard->spawn()); //Tout est valide, on spawn un vaisseau
		}

		if (!game.end_turn(command_queue)) {
			break;
		}
	}

	return 0;
}

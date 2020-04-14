#include "game_map.hpp"
#include "input.hpp"
#include "log.hpp"
#include <stack>
using namespace std;
using namespace hlt;

//Fonction permettant de choisir la direction à prendre pour aller d'une destination à une autre.
Direction  GameMap::DirectionToGo(Position start, Position goal) {
	Direction newDir;
	auto& normalized_source = normalize(start);
	auto& normalized_destination = normalize(goal);

	int dx = std::abs(normalized_source.x - normalized_destination.x);
	int dy = std::abs(normalized_source.y - normalized_destination.y);
	int wrapped_dx = width - dx;
	int wrapped_dy = height - dy;

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

	return newDir;
}

/*Fonction permettant de terminer le score d'attractivité d'une case. On fait un simple ratio halite / tours pour y accéder. Ce n'est pas la façon la plus précise.
 Plus le score est élevé, mieux c'est.
*/
double GameMap::ScoreCell(Ship *s, Position shipyard, Position goal) {

	if (goal == shipyard) return 0;

	int halite = at(goal)->halite;
	int turns_to = calculate_distance(s->position, goal);
	int turns_back = calculate_distance(goal, shipyard);

	int turns = turns_to + turns_back;
	turns = turns_to + turns_back;

	return halite / turns;
}
/*
Cette fonction nous permet de calculer le meilleur chemin (le moins cher en ressource et le plus court possible) avec un algorithme a*
game_map: map de la partie
		h: hauteur de la map
		w: largeur de la map
		start: Position de départ
		goal: position à atteindre
		ship: ship voulant se déplacer

		retourne une pile avec le chemin demandé

		start: https://github.com/hjweide/a-star/blob/master/astar.cpp
*/
std::stack<Position> hlt::GameMap::Astar(unique_ptr<GameMap>& game_map, const int h, const int w, const Position start, const Position goal, std::shared_ptr<Ship> ship) {

	const double INF = std::numeric_limits<double>::infinity();
	int lim = 5;
	int compteur = 0;
	Node start_node(start, 0.);
	Node goal_node(goal, 0.);

	std::shared_ptr<map<Position, Position>> paths = std::make_shared<map<Position, Position>>();
	std::shared_ptr<map<Position, double>> costs = std::make_shared<map<Position, double>>();

	for (int i = 0; i < h; ++i) {
		for (int y = 0; y < w; ++y) {
			costs->operator[](Position(i, y)) = INF;
		}
	}
	costs->operator[](start) = 0.;
	std::priority_queue<Node> nodes_to_visit;
	nodes_to_visit.push(start_node);

	Position* nbrs = new Position[4];

	bool solution_found = false;
	while (!nodes_to_visit.empty()) {
		Node cur = nodes_to_visit.top();

		if (cur == goal_node) {
			solution_found = true;
			break;
		}

		nodes_to_visit.pop();

		int row = cur.pos.y;
		int col = cur.pos.x;

		nbrs[0] = normalize(Position(cur.pos.x, cur.pos.y - 1));
		nbrs[1] = normalize(Position(cur.pos.x - 1, cur.pos.y));
		nbrs[2] = normalize(Position(cur.pos.x + 1, cur.pos.y));
		nbrs[3] = normalize(Position(cur.pos.x , cur.pos.y + 1));

		double heuristic_cost;
		for (int i = 0; i < 4; ++i) {
			if (nbrs[i].x >= 0 ) { 
				if (compteur < 4 && game_map->at(nbrs[i])->is_occupied()) {
					compteur++;
					continue;
				}
				double new_cost = costs->operator[](cur.pos) + (game_map->at(nbrs[i])->halite * 0.10 >=1 ? game_map->at(nbrs[i])->halite * 0.10 : 1);
				if (new_cost < costs->operator[](nbrs[i])) {
					
					//Pour le calcul de l'heureistique, je ne peux pas utiliser le BreadthFirstSearch sur une map de plus de 32 car sinon le nombre de case est trop important et le tour risque de faire plus de 2sec.
					if (height > 32) {
						heuristic_cost = calculate_distance(nbrs[i], goal);
					}
					else {
						std::vector<std::vector<int>> ship_to_dist = game_map->BreadthFirstSearch(nbrs[i]);
						heuristic_cost = ship_to_dist[goal.x][goal.y];
					}

					double priority = new_cost + heuristic_cost;
					nodes_to_visit.push(Node(nbrs[i], priority));

					costs->operator[](nbrs[i]) = new_cost;
					paths->operator[](nbrs[i]) = cur.pos;
				}
			}
		}
	}
	costs->clear();
	delete[] nbrs;
	stack<Position> path;
	
	// Ici on rempli la pile qui contiendra tout le chemin voulu
	if (solution_found) {
		Position path_pos = goal;
		while (path_pos != start) {
			path.push(path_pos);
			path_pos = paths->operator[](path_pos);
		}
	}
	return path;
}

static bool visited[64][64];
/*
Cette fonction nous permet de calculer le cout d'acces de toute les case de la carte depuis une position.
game_map: map de la partie
		start: hauteur de la map

		retourne un double vector correspondant à la map avec le cout d'accès de chaque cas

		Source: https://github.com/aidenbenner/halite3/blob/master/hlt/game_map.cpp (BFS)
*/
std::vector<std::vector<int>> hlt::GameMap::BreadthFirstSearch(Position start) {	
	for (int i = 0; i < 64; i++) {
		for (int k = 0; k < 64; k++) {
			visited[i][k] = false;
		}
	}
	
	int def = 1e8;
	
	vector<vector<int>> dist(width, vector<int>(height, def));	
	dist[start.x][start.y] = 0;

	vector<Position> frontier;
	vector<Position> next;
	next.reserve(200);
	frontier.reserve(200);
	next.push_back(start);
	while (!next.empty()) {
		std::swap(next, frontier);
		next.clear();
		while (!frontier.empty()) {
			auto p = frontier.back();
			frontier.pop_back();

			if (visited[p.x][p.y]) {
				continue;
			}

			visited[p.x][p.y] = true;

			for (auto d : ALL_CARDINALS) {
				auto f = normalize(p.directional_offset(d));
				if (!visited[f.x][f.y]) {
					next.push_back(f);
					continue;
				}
				int c = at(f)->cost() + dist[f.x][f.y];
				if (c <= dist[p.x][p.y]) {
					dist[p.x][p.y] = c;
				}
			}
		}
	}
	return dist;
}

void hlt::GameMap::_update() {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            cells[y][x].ship.reset();
        }
    }

    int update_count;
    hlt::get_sstream() >> update_count;

    for (int i = 0; i < update_count; ++i) {
        int x;
        int y;
        int halite;
        hlt::get_sstream() >> x >> y >> halite;
        cells[y][x].halite = halite;
    }
}

std::unique_ptr<hlt::GameMap> hlt::GameMap::_generate() {
    std::unique_ptr<hlt::GameMap> map = std::make_unique<GameMap>();

    hlt::get_sstream() >> map->width >> map->height;

    map->cells.resize((size_t)map->height);
    for (int y = 0; y < map->height; ++y) {
        auto in = hlt::get_sstream();

        map->cells[y].reserve((size_t)map->width);
        for (int x = 0; x < map->width; ++x) {
            hlt::Halite halite;
            in >> halite;

            map->cells[y].push_back(MapCell(x, y, halite));
        }
    }

    return map;
}
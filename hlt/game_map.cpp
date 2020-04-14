#include "game_map.hpp"
#include "input.hpp"
#include "log.hpp"
#include <stack>
using namespace std;
using namespace hlt;


double GameMap::costfn(Ship *s, Position shipyard, Position dest, bool is_1v1) {

	if (dest == shipyard) return 0;

	int halite = at(dest)->halite;
	int turns_to = calculate_distance(s->position, dest);
	int turns_back = calculate_distance(dest, shipyard);

	int turns = fmax(1.0, turns_to + turns_back);
	if (!is_1v1) {
		turns = turns_to + turns_back;
	}
	turns = turns_to + turns_back;

	return halite / turns;
}
		
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
		// .top() doesn't actually remove the node
		Node cur = nodes_to_visit.top();

		if (cur == goal_node) {
			solution_found = true;
			break;
		}

		nodes_to_visit.pop();

		int row = cur.pos.y;
		int col = cur.pos.x;
		// check bounds and find up to eight neighbors: top to bottom, left to right
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
				// the sum of the cost so far and the cost of this move
				double new_cost = costs->operator[](cur.pos) + (game_map->at(nbrs[i])->halite * 0.10 >=1 ? game_map->at(nbrs[i])->halite * 0.10 : 1);
				//double new_cost = costs->operator[](cur.pos) + game_map->at(nbrs[i])->halite * 0.10;
				if (new_cost < costs->operator[](nbrs[i])) {
					if (height > 32) {
						heuristic_cost = calculate_distance(nbrs[i], goal);
					}
					else {
						std::vector<std::vector<int>> ship_to_dist = game_map->BFS(nbrs[i]);
						heuristic_cost = ship_to_dist[goal.x][goal.y];
					}

					//paths with lower expected cost are explored first
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
std::vector<std::vector<int>> hlt::GameMap::BFS(Position source, bool collide, int starting_hal) {
	// dfs out of source to the entire map
	
	for (int i = 0; i < 64; i++) {
		for (int k = 0; k < 64; k++) {
			visited[i][k] = false;
		}
	}
	
	int def = 1e8;
	
	vector<vector<int>> dist(width, vector<int>(height, def));	
	dist[source.x][source.y] = 0;

	vector<Position> frontier;
	vector<Position> next;
	next.reserve(200);
	frontier.reserve(200);
	next.push_back(source);
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
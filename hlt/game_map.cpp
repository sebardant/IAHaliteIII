#include "game_map.hpp"
#include "input.hpp"

using namespace std;
using namespace hlt;

static bool visited[64][64];

hlt::BFSR hlt::GameMap::BFS(Position source, bool collide, int starting_hal) {
	// dfs out of source to the entire map
	for (int i = 0; i < 64; i++) {
		for (int k = 0; k < 64; k++) {
			visited[i][k] = false;
		}
	}
	
	int def = 1e8;
	
	vector<vector<int>> dist(width, vector<int>(height, def));
	vector<vector<Position>> parent(width, vector<Position>(height, { -1, -1 }));
	vector<vector<int>> turns(width, vector<int>(height, 1e9));
	
	dist[source.x][source.y] = 0;
	turns[source.x][source.y] = 1;

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
					parent[p.x][p.y] = f;
				}
			}
		}
	}
	return BFSR{ dist, parent, turns };
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






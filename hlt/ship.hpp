#pragma once

#include "entity.hpp"
#include "constants.hpp"
#include "command.hpp"

#include <memory>

namespace hlt {
    struct Ship : Entity {
        Halite halite;

        Ship(PlayerId player_id, EntityId ship_id, int x, int y, Halite halite) :
            Entity(player_id, ship_id, x, y),
            halite(halite)
        {}

        bool is_full() const {
            return halite >= constants::MAX_HALITE;
        }

        Command make_dropoff() const {
            return hlt::command::transform_ship_into_dropoff_site(id);
        }

        Command move(Direction direction) const {
            return hlt::command::move(id, direction);
        }

        Command stay_still() const {
            return hlt::command::move(id, Direction::STILL);
        }

        static std::shared_ptr<Ship> _generate(PlayerId player_id);
    };

	//diff�rent �tat
	enum ShipState {
		GATHERING, //Etat de r�colte, plus pr�cisement de d�placement pour atteindre une nouvelle case � miner
		MINE, //Etat de minage
		RETURNING, //Retorune � la case shipyard ou dropoff la plus proche
		DROPOFF, //se d�place pour se transformer en dropOff
	};
}

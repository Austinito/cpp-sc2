#include <sc2api/sc2_api.h>
#include <sc2api/sc2_data.h>
#include <sc2api/sc2_unit_filters.h>

#include <iostream>

// Number of workers to assign to each townhall
const int kWorkersPerTownhall = 16;

using namespace sc2;

class Bot : public Agent {
public:
    virtual void OnGameStart() final {
        std::cout << "Hello, World!" << std::endl;
    }

    virtual void OnStep() final {
        if (Observation()->GetGameLoop() % 100 == 0) {
            printf("%d: %s:%d %s:%d\n", Observation()->GetGameLoop(), "Minerals", Observation()->GetMinerals(),
                   "Vespene", Observation()->GetVespene());
        }

        HandleTownhall();

        TryBuildBarracks();
    }

    virtual void OnUnitIdle(const Unit* unit) final {
        switch (unit->unit_type.ToType()) {
            case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
                // Check the number of SCV's and build more if needed
                int numSCVs = Observation()->GetFoodWorkers();
                int maxSCVs = kWorkersPerTownhall * Observation()->GetUnits(Unit::Alliance::Self, IsTownHall()).size();
                int numRefineries = Observation()->GetUnits(Unit::Alliance::Self, IsVisibleGeyser()).size();
                maxSCVs += numRefineries * 3;
                if (numSCVs < maxSCVs) {
                    std::cout << "Building SCV: " << numSCVs << " of " << maxSCVs << std::endl;
                    Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
                }
                break;
            }
            case UNIT_TYPEID::TERRAN_REFINERY:
                AssignWorker(unit);
                break;
            case UNIT_TYPEID::TERRAN_REFINERYRICH:
                AssignWorker(unit);
                break;
            case UNIT_TYPEID::TERRAN_SCV: {
                const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
                if (!mineral_target) {
                    break;
                } else {
                    Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
                    break;
                }
            }
            case UNIT_TYPEID::TERRAN_BARRACKS: {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
                break;
            }
            case UNIT_TYPEID::TERRAN_MARINE: {
                if (CountUnitType(UNIT_TYPEID::TERRAN_MARINE) >= 15) {
                    const GameInfo& game_info = Observation()->GetGameInfo();
                    Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
                } else {
                    Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, GetRandomNearbyLocation(unit->pos));
                }
                break;
            }
            default:
                break;
        }
    }

    virtual void OnBuildingConstructionComplete(const Unit* unit) {
        if (unit->unit_type == UNIT_TYPEID::TERRAN_REFINERY) {
            for (int i = 0; i < 3; i++) {
                AssignWorker(unit);
            }
        }
    }

    void HandleTownhall() {
        if (TryBuildSupplyDepot()) {
            std::cout << "Building supply depot" << std::endl;
        }

        if (TryBuildVespeneGas()) {
            std::cout << "Building refinery" << std::endl;
        }

        BalanceWorkers();
    }

    void BalanceWorkers() {
        const ObservationInterface* observation = Observation();
        int onMineralPatch = observation->GetUnits(Unit::Alliance::Self, IsTownHall()).front()->assigned_harvesters;
        if (onMineralPatch > kWorkersPerTownhall) {
            std::cout << "Moving workers to gas" << std::endl;
            std::vector<const Unit*> refineriesWithoutMaxWorkers;

            for (const auto& unit : observation->GetUnits(Unit::Alliance::Self, IsBuilding())) {
                if (unit->unit_type == UNIT_TYPEID::TERRAN_REFINERY && unit->assigned_harvesters < 3) {
                    refineriesWithoutMaxWorkers.push_back(unit);
                }
            }

            if (!refineriesWithoutMaxWorkers.empty()) {
                std::cout << "Assigning worker to geyser" << std::endl;
                AssignWorker(refineriesWithoutMaxWorkers.front());
            } else {
                std::cout << "No refineries without max workers" << std::endl;
            }
        }
    }

    bool TryBuildBarracks() {
        const ObservationInterface* observation = Observation();

        if (CountUnitType(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
            return false;
        }

        if (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) >= 3) {
            return false;
        }

        return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
    }

    size_t CountUnitType(UNIT_TYPEID unit_type) {
        return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
    }

    bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type, Tag location_tag) {
        const ObservationInterface* observation = Observation();
        Units workers = observation->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
        const Unit* target = observation->GetUnit(location_tag);

        if (workers.empty()) {
            return false;
        }

        // Check to see if there is already a worker heading out to build it
        for (const auto& worker : workers) {
            for (const auto& order : worker->orders) {
                if (order.ability_id == ability_type_for_structure) {
                    return false;
                }
            }
        }

        // If no worker is already building one, get a random worker to build one
        const Unit* unit = GetRandomEntry(workers);

        // Check to see if unit can build there
        if (Query()->Placement(ability_type_for_structure, target->pos)) {
            Actions()->UnitCommand(unit, ability_type_for_structure, target);
            return true;
        }
        return false;
    }

    bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV,
                           int amountToBuild = 1) {
        const ObservationInterface* observation = Observation();

        // If a unit already is building a supply structure of this type, do nothing.
        // Also get an scv to build the structure.
        const Unit* unit_to_build = nullptr;
        Units units = observation->GetUnits(Unit::Alliance::Self);
        int count = 0;
        for (const auto& unit : units) {
            for (const auto& order : unit->orders) {
                if (order.ability_id == ability_type_for_structure) {
                    count++;
                    if (count == amountToBuild) {
                        return false;
                    }
                }
            }

            if (unit->unit_type == unit_type) {
                unit_to_build = unit;
            }
        }
        if (unit_to_build) {
            Point2D loc = GetRandomNearbyLocation(unit_to_build->pos);
            Actions()->UnitCommand(unit_to_build, ability_type_for_structure, loc);
            return true;
        } else {
            return false;
        }
    }

    Point2D GetRandomNearbyLocation(const Point2D& start) {
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        return Point2D(start.x + rx * 15.0f, start.y + ry * 15.0f);
    }

    bool TryBuildSupplyDepot() {
        const ObservationInterface* observation = Observation();
        // If we are not supply capped, don't build a supply depot.
        if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
            return false;

        // Try and build a depot. Find a random SCV and give it the order.
        int numDepots = observation->GetFoodUsed() / 12;
        return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT, UNIT_TYPEID::TERRAN_SCV, numDepots);
    }

    bool TryBuildVespeneGas() {
        const ObservationInterface* observation = Observation();

        // If we have more than 14 SCV's then build a vespene
        // we don't need more than 2 refineries
        if (observation->GetFoodWorkers() < 14)
            return false;

        if (CountUnitType(UNIT_TYPEID::TERRAN_REFINERY) >= 2)
            return false;

        Point2D loc = observation->GetStartLocation();
        const Unit* geyser = FindClosestGeyser(loc);

        if (geyser) {
            return TryBuildStructure(ABILITY_ID::BUILD_REFINERY, UNIT_TYPEID::TERRAN_SCV, geyser->tag);
        } else {
            return false;
        }
    }

    bool AssignWorker(const Unit* unit) {
        const Unit* scv = FindNearestUnassignedScv(unit, unit->pos);
        if (scv) {
            Actions()->UnitCommand(scv, ABILITY_ID::SMART, unit);
            return true;
        } else {
            return false;
        }
    }

    const Unit* FindClosestGeyser(const Point2D& start) {
        Units units = Observation()->GetUnits(Unit::Alliance::Neutral, IsVisibleGeyser());
        float distance = std::numeric_limits<float>::max();
        const Unit* target = nullptr;
        for (auto u : units) {
            if (u->unit_type != UNIT_TYPEID::NEUTRAL_VESPENEGEYSER) {
                continue;
            }
            float d = DistanceSquared2D(u->pos, start);
            if (d < distance) {
                distance = d;
                target = u;
            }
        }
        return target;
    }

    const Unit* FindNearestMineralPatch(const Point2D& start) {
        Units units = Observation()->GetUnits(Unit::Alliance::Neutral, IsVisibleMineralPatch());
        float distance = std::numeric_limits<float>::max();
        const Unit* target = nullptr;
        for (const auto& u : units) {
            float d = DistanceSquared2D(u->pos, start);
            if (d < distance) {
                distance = d;
                target = u;
            }
        }
        return target;
    }

    const Unit* FindNearestUnassignedScv(const Unit* unit, const Point2D& start) {
        const ObservationInterface* observation = Observation();
        Units workers = observation->GetUnits(Unit::Alliance::Self, IsWorker());
        const Unit* closest = nullptr;
        float distance = std::numeric_limits<float>::max();
        for (const auto& worker : workers) {
            if (!worker->orders.empty()) {
                // This should move a worker that isn't mining gas to gas
                const Unit* target = observation->GetUnit(worker->orders.front().target_unit_tag);
                if (target == nullptr) {
                    continue;
                }
                if (target->unit_type != UNIT_TYPEID::TERRAN_REFINERY) {
                    float d = DistanceSquared2D(worker->pos, start);
                    if (d < distance) {
                        distance = d;
                        closest = worker;
                    }
                }
            }
        }
        return closest;
    }
};

int main(int argc, char* argv[]) {
    Coordinator coordinator;
    coordinator.LoadSettings(argc, argv);

    Bot bot;
    coordinator.SetParticipants({CreateParticipant(Race::Terran, &bot), CreateComputer(Race::Zerg)});

    coordinator.LaunchStarcraft();
    coordinator.StartGame(sc2::kMapBelShirVestigeLE);

    while (coordinator.Update()) {
    }
    return 0;
}

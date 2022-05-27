#include <sc2api/sc2_api.h>
#include <sc2api/sc2_unit_filters.h>

#include <iostream>

using namespace sc2;

class Bot : public Agent {
public:
    virtual void OnGameStart() final {
        std::cout << "Hello, World!" << std::endl;
    }

    virtual void OnStep() final {

        //======DEBUG======
        if (Observation()->GetGameLoop() % 100 == 0)
        {
            std::cout << "Gameloop: " << Observation()->GetGameLoop() << std::endl;
            std::cout << "Minerals: " << Observation()->GetMinerals() << std::endl;
            std::cout << "Vesp Gas: " << Observation()->GetVespene() << std::endl;
            std::cout << "Military: " << Observation()->GetFoodArmy()  << std::endl;
        }
        //==================

        TryBuildSupplyDepot();
        TryBuildBarracks();
        TryBuildRefinery();

        TryGetGas();
        Combat();
    }

    virtual void OnUnitIdle(const Unit* unit) final {
        switch (unit->unit_type.ToType()) {
            case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
                if(CheckSCVCount(unit))
                    Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
                break;
            }
            case UNIT_TYPEID::TERRAN_SCV: {
                const Unit* mineral_target = FindNearestMineralPatch(unit->pos);
                if (!mineral_target) {
                    break;
                }
                Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
                break;
            }
            case UNIT_TYPEID::TERRAN_BARRACKS: {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
                break;
            }
            case UNIT_TYPEID::TERRAN_MARINE: {
                if(TryAttackMove())
                {
                    const GameInfo& game_info = Observation()->GetGameInfo();
                    Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
                    break;
                }
                break;
            }
            default: {
                break;
            }
        }
    }
private:
    size_t CountUnitType(UNIT_TYPEID unit_type) {
        return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
    }

    const Unit* FindNearest(const Point2D& start, UNIT_TYPEID unit_type) {
        Units units = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type));
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

    bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV) {
        const ObservationInterface* observation = Observation();

        // If more than 1 unit already is building a supply structure of this type, do nothing.
        // Also get an scv to build the structure.
        const Unit* unit_to_build = nullptr;
        Units units = observation->GetUnits(Unit::Alliance::Self);
        int count = 0;
        for (const auto& unit : units) {
            for (const auto& order : unit->orders) {
                if (order.ability_id == ability_type_for_structure && count > 1) {
                    return false;
                }
            }

            if (unit->unit_type == unit_type) {
                unit_to_build = unit;
                count++;
            }
        }
        
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        if (ability_type_for_structure == ABILITY_ID::BUILD_REFINERY)
        {
           Actions()->UnitCommand(unit_to_build,
                ability_type_for_structure,
                FindNearestVespeneGas(unit_to_build->pos));
        }
        else
        {
           Actions()->UnitCommand(unit_to_build,
                ability_type_for_structure,
                Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));
        }

        return true;
    }

    bool TryBuildSupplyDepot() {
        const ObservationInterface* observation = Observation();

        // If we are not supply capped, don't build a supply depot. Or we are at max supply already
        if (observation->GetFoodUsed() <= observation->GetFoodCap() - 4 || observation->GetFoodCap() == 200)
            return false;

        // Try and build a depot. Find a random SCV and give it the order.
        return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
    }

    bool TryBuildRefinery() {
        const ObservationInterface* observation = Observation();

        // If we have a supply depot and a barracks
        if (CountUnitType(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1 && CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
            return false;
        }
        if (CountUnitType(UNIT_TYPEID::TERRAN_REFINERY) == 2)
        {
            return false;
        }

        // Try and build a refinery. Find a random SCV and give it the order.
        return TryBuildStructure(ABILITY_ID::BUILD_REFINERY);
    }

    bool TryBuildBarracks() {
        const ObservationInterface* observation = Observation();

        if (CountUnitType(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
            return false;
        }

        if (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) > 2) {
            return false;
        }

        return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
    }

    const Unit* FindNearestMineralPatch(const Point2D& start) {
        Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
        float distance = std::numeric_limits<float>::max();
        const Unit* target = nullptr;
        for (const auto& u : units) {
            if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
                float d = DistanceSquared2D(u->pos, start);
                if (d < distance) {
                    distance = d;
                    target = u;
                }
            }
        }
        return target;
    }

    const Unit* FindNearestVespeneGas(const Point2D& start) {
        Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
        float distance = std::numeric_limits<float>::max();
        const Unit* target = nullptr;
        for (const auto& u : units) {
            if (u->unit_type == UNIT_TYPEID::NEUTRAL_VESPENEGEYSER) {
                float d = DistanceSquared2D(u->pos, start);
                if (d < distance) {
                    distance = d;
                    target = u;
                }
            }
        }
        return target;
    }

    //Attack Movement 
    bool TryAttackMove()
    {
        const ObservationInterface* observation = Observation();

        if (CountUnitType(UNIT_TYPEID::TERRAN_MARINE) < 10) {
            return false;
        }

        return true;
    }

    ////If the refiner isn't at max then find nearest scv to mine it.
    void TryGetGas()
    {
        Units units = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));
        const Unit* closestSCV;
        bool validSCV = true;
        if (units.size() > 0)
        {
            for (const auto& u : units) {
                if (u->ideal_harvesters > u->assigned_harvesters)
                {
                    //AssignMiner(u->pos);
                    closestSCV = FindNearest(u->pos, UNIT_TYPEID::TERRAN_SCV);
                    for (const auto& buff : closestSCV->buffs) {
                        if (buff == 273) {
                            validSCV = false;
                        }
                    }
                    if (validSCV)
                    {
                        Actions()->UnitCommand(closestSCV, ABILITY_ID::SMART, u);
                        std::cout << "SCV has been manually assigned to refiner." << std::endl;
                    }
                }
            }
        }
    }

    //Going to check to already at optimal miners
    bool CheckSCVCount(const Unit* unit)
    {
        if (unit->ideal_harvesters > unit->assigned_harvesters)
        {
            return true;
        }
        return false;
    }

    //To micromanage Comabat systems, check range vs if an enemny is in range. If so advance backwards and fire till. I.e. Marines vs zerg.
    void Combat()
    {
        DistanceCheck();
    }

    bool DistanceCheck()
    {
        return false;
    }

};

int main(int argc, char* argv[]) {
    Coordinator coordinator;
    coordinator.LoadSettings(argc, argv);
    //coordinator.SetRealtime(true);
    Bot bot;
    coordinator.SetParticipants({
        CreateParticipant(Race::Terran, &bot),
        CreateComputer(Race::Zerg)
        });

    coordinator.LaunchStarcraft();
    coordinator.StartGame(sc2::kMapBelShirVestigeLE);

    while (coordinator.Update()) {
    }

    return 0;
}

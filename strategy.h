
#define HEADER_OLNY_INLINE inline
#define HEADER_OLNY_STATIC static
#include "Strategy.hpp"
#include "Strategy.cpp"
#include "mystrategy.hpp"
#include "mystrategy.cpp"






#include "base_strategy.h"
#include <vector>
#include <sstream>
#include <set>
#include <map>
#include <memory>
#include <bitset>


class Strategy : public BaseStrategy
{
    std::unique_ptr<MyStrategy> myStrategy;
    std::map<int, int> myToApiElIdMap;
    std::map<int, int> apiToMyElIdMap;
    std::set<int> registeredPassengers;
    std::map<int, std::bitset<LEVELS_COUNT> > visitedLevels;
public:
    void on_tick(std::vector<Elevator>& myElevators, std::vector<Passenger>& myPassengers, std::vector<Elevator>& enemyElevators, std::vector<Passenger>& enemyPassengers)
    {
        Side side = getSide(myElevators[0].type);

        if (!myStrategy)
        {
            myStrategy.reset(new MyStrategy(side));
            for (int i = 0; i < 4; ++i)
            {
                for (int myI = 0; myI < 8; ++myI)
                {
                    MyElevator &myElevator = myStrategy->sim.elevators[myI];

                    if (myElevator.ind == i)
                    {
                        if (side == myElevator.side)
                        {
                            myToApiElIdMap[myElevator.id] = myElevators[i].id;
                            apiToMyElIdMap[myElevators[i].id] = myElevator.id;
                        }
                        else
                        {
                            myToApiElIdMap[myElevator.id] = enemyElevators[i].id;
                            apiToMyElIdMap[enemyElevators[i].id] = myElevator.id;
                        }
                    }
                }
            }
        }

        std::map<int, Passenger*> passById;
        std::map<int, Elevator*> myElById;

        for (Passenger &p : myPassengers)
        {
            syncPass(p);
            passById[p.id] = &p;
        }

        for (Passenger &p : enemyPassengers)
        {
            syncPass(p);
            passById[p.id] = &p;
        }

        for (Elevator &e : myElevators)
        {
            syncElevator(e, true);
            myElById[e.id] = &e;
        }

        for (Elevator &e : enemyElevators)
            syncElevator(e, false);

        myStrategy->makeMove();

        for (const std::pair<const int, MyPassenger> & passenger : myStrategy->sim.passengers)
        {
            int id = passenger.first;
            const MyPassenger &otherPass = passenger.second;

            Passenger *thisPass = passById[id];
            if (!thisPass)
            {
                std::cout << myStrategy->sim.tick << " Passenger not found " << myStrategy->sim.tick << " " << id << " " << otherPass.x << " " << otherPass.y << " " << getPStateName(otherPass.state) << " " << otherPass.time_to_away <<
                             std::endl;
            }
            else
            {
                for (int i : otherPass.set_elevator)
                {
                    Elevator *el = myElById[myToApiElIdMap[i]];
                    if (!el)
                    {
                        std::cout << "Elevator not found " << i << std::endl;
                    }
                    else
                    {
                        thisPass->set_elevator(*el);
                    }
                }

            }
        }

        for (const MyElevator &elevator : myStrategy->sim.elevators)
        {
            if (elevator.go_to_floor == -1)
                continue;

            if (elevator.side == side)
            {
                Elevator *thisElevator = myElById[myToApiElIdMap[elevator.id]];
                if (!thisElevator)
                {
                    std::cout << "Elevator not found2 " << elevator.id << std::endl;
                }
                else
                {
                    thisElevator->go_to_floor(elevator.go_to_floor + 1);
                }
            }
        }

        myStrategy->sim.step();
    }

    void syncPass(Passenger &otherPass)
    {
        if (!visitedLevels.count(otherPass.id))
        {
            visitedLevels[otherPass.id].set(0);
        }

        PState pstate = convertPState(otherPass.state);
        if (pstate == PState::EXITING || pstate == PState::MOVING_TO_FLOOR)
            return;

        Simulator &sim = myStrategy->sim;
        int id = otherPass.id;
        bool exists = sim.passengers.count(id);
        MyPassenger &thisPass = sim.passengers[id];
        if (!id)
            std::cout << "ERR3 " << id << std::endl;

        thisPass.x = otherPass.x;
        thisPass.y = otherPass.y - 1.0;
        thisPass.from_floor = otherPass.from_floor - 1;
        thisPass.dest_floor = otherPass.dest_floor - 1;
        thisPass.dest_floor_confirmed = true;
        thisPass.time_to_away = otherPass.time_to_away;
        if (thisPass.time_to_away == 0)
            std::cout << sim.tick << " TTA " << otherPass.id << std::endl;
        if (!apiToMyElIdMap.count(otherPass.elevator))
            thisPass.elevator = -1;
        else
            thisPass.elevator = apiToMyElIdMap[otherPass.elevator];
        thisPass.state = convertPState(otherPass.state);
        thisPass.mass = otherPass.weight;

        if (pstate == PState::WAITING_FOR_ELEVATOR) {
            visitedLevels[otherPass.id].set(thisPass.getFloor());
        }
        else if (pstate == PState::USING_ELEVATOR) {
            visitedLevels[otherPass.id].set(thisPass.dest_floor);
        }
        else if (pstate == PState::MOVING_TO_FLOOR) {
            visitedLevels[otherPass.id].set(thisPass.dest_floor);
        }

        for (int i = 0; i < LEVELS_COUNT; ++i)
        {
            if (visitedLevels[otherPass.id].test(i))
                thisPass.visitedLevels.set(i);
        }

        if (!registeredPassengers.count(otherPass.id) || !exists)
        {
            if (registeredPassengers.count(otherPass.id))
                std::cout << sim.tick << " ERR ALREADY REGISTERED " << otherPass.id << std::endl;
            else
                registeredPassengers.insert(otherPass.id);

            std::cout << sim.tick << " REG " << otherPass.id << std::endl;

            thisPass.id = otherPass.id;
            thisPass.spawn_x = thisPass.x > 0 ? 20.0 : -20.0;
            thisPass.side = getSide(otherPass.type);
            thisPass.placesRemained = 4;

            std::set<int> placesSet;
            placesSet.insert(thisPass.dest_floor);
            while(placesSet.size() < 5)
            {
                placesSet.insert(sim.ramdomFloor());
            }

            placesSet.erase(thisPass.dest_floor);
            std::vector<int> placesArr = std::vector<int>(placesSet.begin(), placesSet.end());
            std::random_shuffle(placesArr.begin(), placesArr.end());
            for (size_t i = 0; i < placesArr.size(); ++i)
                thisPass.places[i] = placesArr[i];
        }
    }

    void syncElevator(Elevator &elevator, bool my)
    {
        Simulator &sim = myStrategy->sim;
        MyElevator &thisElevator = sim.elevators[apiToMyElIdMap[elevator.id]];

         EState state = convertEState(elevator.state);
         if (state == EState::WAITING)
              state = EState::CLOSING;

        if (my && thisElevator.state != state)
            std::cout << sim.tick << " WRONG STATE " << getEStateName(thisElevator.state) << " " << getEStateName(state)
             << " ttf " << thisElevator.time_to_floor << " nf " << thisElevator.next_floor << " id " << elevator.id << std::endl;


        thisElevator.y = elevator.y - 1.0;
        thisElevator.speed = elevator.speed;
        if (convertEState(elevator.state) == EState::FILLING && thisElevator.state != EState::FILLING)
            thisElevator.time_on_the_floor_with_opened_doors = 0;

        if (elevator.next_floor == -1)
            thisElevator.next_floor = -1;
        else
            thisElevator.next_floor = elevator.next_floor - 1;



        if (thisElevator.state != state)
            thisElevator.setState(state, sim);


        thisElevator.passengers.clear();
        for (Passenger &p : elevator.passengers)
            thisElevator.passengers.insert(p.id);
    }

    EState convertEState(int state)
    {
        return (EState) state;
    }

    PState convertPState(int state)
    {
        return (PState) (state - 1);
    }

    Side getSide(QString type)
    {
        if (type == "FIRST_PLAYER")
            return Side::LEFT;
        else
            return Side::RIGHT;
    }

};



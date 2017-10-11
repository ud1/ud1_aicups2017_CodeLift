#ifndef MYSTRATEGY_H
#define MYSTRATEGY_H

#ifndef HEADER_OLNY_INLINE
#include "Strategy.hpp"
#endif

#include <bitset>
#include <iostream>
#include <map>

enum class Direction {
    UP, DOWN
};

int getNearestToLevelDestination(Simulator &sim, MyElevator& elevator, int level);
int getNearestToLevelDestinationNoRand(Simulator &sim, MyElevator& elevator, int level);

class MyStrategy;
struct ElevatorStrategyUpDown
{
	Side side;
	Direction dir = Direction::UP;
	int dirChanges = 0;
	int firstMoveMinDest = 0;
	bool doPredictions = true;
	
	void makeMove(Simulator &sim, MyElevator &elevator, MyStrategy *strategy)
	{
		Direction old = dir;
		doMakeMove(sim, elevator, strategy);
		if (old != dir)
			++dirChanges;
	}
	
	void doMakeMove(Simulator &sim, MyElevator &elevator, MyStrategy *strategy)
	{
		if (dir == Direction::UP && elevator.getFloor() == (LEVELS_COUNT - 1))
			dir = Direction::DOWN;
		else if (dir == Direction::DOWN && elevator.getFloor() == 0)
			dir = Direction::UP;
		
		if (doPredictions && elevator.state == EState::CLOSING && elevator.closing_or_opening_ticks == 98)
		{
			recalcDestinationBeforeDoorsClose(sim, elevator, strategy);
			return;
		}
		
		if (elevator.state != EState::FILLING)
			return;
		
		Value value = sim.getCargoValue(elevator);
		
		int floor = elevator.getFloor();
		
		if (elevator.passengers.size() == MAX_PASSENGERS && elevator.time_on_the_floor_with_opened_doors >= 40)
		{
			int go_to_floor = getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
			goToFloor(elevator, go_to_floor);
		}
		
		int passCount = invitePassengers(sim, elevator);
		if (sim.tick < 1980 && floor == 0)
			++passCount;
        
		bool anyElevatorsCloserThanMe = false;
		for (MyElevator &otherEl : sim.elevators)
		{
			if (otherEl.side == side && otherEl.state == EState::FILLING && otherEl.getFloor() == floor && otherEl.ind < elevator.ind)
			{
				anyElevatorsCloserThanMe = true;
				break;
			}
		}
		
        if (!passCount)
		{
			bool anyElevatorsCloser = false;
			/*for (MyElevator &e : sim.elevators)
			{
				if (e.id != elevator.id && e.ind < elevator.ind && e.side == elevator.side && e.state == EState::FILLING && e.getFloor() == floor
					&& e.passengers.size() < 10 && elevator.passengers.size() >= 10 && e.time_on_the_floor_with_opened_doors > 40)
				{
					anyElevatorsCloser = true;
				}
			}*/
			
			if (!anyElevatorsCloser)
			{
				//int maxWait = 300 - std::max(0, (int) (elevator.passengers.size()) - 12) * 25;
				int maxWait;
				if (elevator.time_on_the_floor_with_opened_doors < 40)
					maxWait = 630;
				else
					maxWait = 630 - std::max(0, (int) (elevator.passengers.size()) - 9) * 50;
				
				// 600 7*50 RES: 1.06206 10375.2 11019.1 rem 817.7 1285.5 W 72 L 28 GOOD!
				// 600 8 50 RES: 1.07105 10244.1 10971.9 rem 833.9 1335.4 W 75 L 24 GOOD!
				// 600 9 50 RES: 1.07693 10161.8 10943.5 rem 801 1342.8 W 82 L 18 GOOD!
				// 620 9 50 RES: 1.07793 10141.9 10932.3 rem 842.4 1348.2 W 83 L 17 GOOD!
				// 630 9 50 RES: 1.07779 10126.6 10914.3 rem 847.8 1374.3 W 84 L 16 GOOD!
				// 600 9 55 RES: 1.07187 10227.1 10962.1 rem 818.9 1351.1 W 79 L 20 GOOD!
				// 620 9 55 RES: 1.07481 10184.1 10946 rem 767.8 1360.6 W 81 L 19 GOOD!
				// 630 9 55 RES: 1.07199 10189.6 10923.1 rem 798.5 1360.8 W 79 L 20 GOOD!
				// 630 9 60 RES: 1.07513 10215.9 10983.4 rem 830 1335 W 78 L 21 GOOD!
				// 10 50 RES: 1.05791 10207.7 10798.8 rem 832.3 1418.2 W 75 L 25 GOOD!
				
				int ticksToWait = std::min(std::max(0, 7200 - sim.tick - std::min(1200, value.ticks)), maxWait);
				
				// 1300 RES: 1.08322 10153.2 10998.2 rem 900 1232.8 W 83 L 16 GOOD!
				// 1200 RES: 1.08367 10136.8 10984.9 rem 890.4 1263.6 W 84 L 15 GOOD!
				// 1100 RES: 1.08038 10130.6 10944.9 rem 879.5 1313.5 W 84 L 16 GOOD!
				// 1000 RES: 1.07779 10126.6 10914.3 rem 847.8 1374.3 W 84 L 16 GOOD!
				// 900 RES: 1.07609 10130.1 10900.9 rem 839.1 1437.5 W 83 L 17 GOOD!
				
				// RES: 7 50  1.06206 10375.2 11019.1 rem 817.7 1285.5 W 72 L 28 GOOD!
				
				int t = -1;
				for (auto && p = sim.outPassengers.begin(); p != sim.outPassengers.upper_bound(sim.tick + ticksToWait); ++p)
				{
					if (p->second.getFloor() == floor)
					{
						++passCount;
						if (t == -1)
							t = p->first;
					}
				}
				
				if (t > sim.tick + (250 + passCount * 60))
				{
					passCount = 0;
				}
			}
		}
		
		int go_to_floor = getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
		
		if (!passCount && elevator.time_on_the_floor_with_opened_doors >= 40)
		{
			if (go_to_floor != floor)
			{
				goToFloor(elevator, go_to_floor);
			}
			else
			{
				int floorsPass[LEVELS_COUNT] = {};
				for (auto && p = sim.outPassengers.begin(); p != sim.outPassengers.upper_bound(sim.tick + 800); ++p)
				{
					floorsPass[p->second.getFloor()] += p->second.getValue2(elevator.side);
				}
				
				if (dir == Direction::UP)
					goToFloor(elevator, floor + 1);
				else
					goToFloor(elevator, floor - 1);
			}
		}
		else if (go_to_floor != floor && sim.tick > 6500)
		{
			double timeToFloor = std::abs(go_to_floor - elevator.getFloor());
			if (go_to_floor > elevator.getFloor())
				timeToFloor *= 60;
			else
				timeToFloor *= 51;
			timeToFloor += 241;
			if (sim.tick + timeToFloor >= 7200)
				goToFloor(elevator, go_to_floor);
		}
	}
	
	void recalcDestinationBeforeDoorsClose(Simulator &sim, MyElevator &elevator, MyStrategy *strategy);
	
	void goToFloor(MyElevator &elevator, int go_to_floor)
	{
        if (go_to_floor > elevator.getFloor())
            dir = Direction::UP;
        else
            dir = Direction::DOWN;
        elevator.go_to_floor = go_to_floor;
	}
	
	int invitePassengers(Simulator &sim, MyElevator &elevator)
	{
		int passCount = 0;
		int floor = elevator.getFloor();
		
		bool anyEnemyElevatorsCloser = false;
		bool anyElevatorsCloser = false;
		for (MyElevator &e : sim.elevators)
		{
			if (e.id != elevator.id && e.ind <= elevator.ind && e.state == EState::FILLING && e.getFloor() == floor && e.side != elevator.side)
			{
				anyEnemyElevatorsCloser = true;
			}
			if (e.id != elevator.id && e.ind <= elevator.ind && e.state == EState::FILLING && e.getFloor() == floor)
			{
				anyElevatorsCloser = true;
			}
		}
		
		std::multimap<int, MyPassenger *> passengers;
		
		for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
        {
            MyPassenger &passenger = passengerEntry.second;
            int dest_floor = passenger.dest_floor;
			
			if (std::abs(dest_floor - passenger.from_floor) <= 1)
				continue;
			
			bool our = passenger.side == elevator.side;
            //if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
            {
				if (floor == 0 && dirChanges == 0 && dest_floor <= firstMoveMinDest && sim.tick < 1980)
					continue;
				
                if (passenger.getFloor() == floor)
				{
					if (passenger.state == PState::WAITING_FOR_ELEVATOR || passenger.state == PState::RETURNING)
					{
						/*if (passenger.dest_floor == 0)
						{
							passenger.set_elevator.insert(elevator.id);
							++passCount;
						}
						else
						{*/
							passengers.insert(std::make_pair(passenger.getValue(side), &passenger));
						//}
					}
					else if (passenger.state == PState::MOVING_TO_ELEVATOR && passenger.elevator == elevator.id)
					{
						++passCount;
						//levels.set(dest_floor);
					}
				}
            }
        }
		
		//double maxDist = 0;
		//if (!anyElevatorsCloser)
		{
			int limit = MAX_PASSENGERS - (int) elevator.passengers.size() - passCount;
			if (anyEnemyElevatorsCloser || elevator.ind == 3 && anyElevatorsCloser)
				limit += 3;
			
			for (auto it = passengers.rbegin(); it != passengers.rend(); ++it)
			{
				if (limit <= 0)
					break;
				
				if (limit <= 2 && it->first < 30 || limit <= 4 && it->first < 20)
					continue;
				
				it->second->set_elevator.insert(elevator.id);
				++passCount;
				
				--limit;
				
				//maxDist = std::max(maxDist, std::abs(it->second->x - (double) elevator.x));
			}
		}
		
        return passCount;
	}
};

class MyStrategy
{
public:
	Side side;
	Simulator sim;
	ElevatorStrategyUpDown strategy1;
	ElevatorStrategyUpDown strategy2;
	ElevatorStrategyUpDown strategy3;
	ElevatorStrategyUpDown strategy4;

    MyStrategy(Side side);
    ~MyStrategy();
	
	void makeMove(Simulator &inputSim);
	void makeMove();
};

#endif // MYSTRATEGY_H

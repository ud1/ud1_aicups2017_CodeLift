
#ifndef HEADER_OLNY_INLINE
#include "mystrategy.hpp"
#endif
#include <cmath>
#include <algorithm>

#ifndef HEADER_OLNY_INLINE
#define HEADER_OLNY_INLINE
#define HEADER_OLNY_STATIC
#endif

HEADER_OLNY_INLINE int getNearestToLevelDestination(Simulator &sim, MyElevator& elevator, int level)
{
    int delta = 1000;
    int targetFloor = level;
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = sim.passengers[id];
        int d = std::abs(level - pass.dest_floor);
        if (d < delta)
        {
            delta = d;
            targetFloor = pass.dest_floor;
        }
    }
    
    if (targetFloor == level)
	{
		return sim.random.get_random() % 8 + 1;
	}

    return targetFloor;
}

HEADER_OLNY_INLINE int getNearestToLevelDestinationNoRand(Simulator &sim, MyElevator& elevator, int level)
{
    int targetFloor = level;

	std::map<int, int> points;
	std::map<int, int> destNum;
	
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = sim.passengers[id];
		points[pass.dest_floor] -= pass.getValue(elevator.side);
		destNum[pass.dest_floor]++;
    }
    
    for (std::pair<const int, int> &p : destNum)
	{
		int k = p.second;
		if (k > 1)
		{
			if (k == 2)
				points[p.first] -= 10.0;
			else if (k == 3)
				points[p.first] -= 20.0;
			else if (k == 4)
				points[p.first] -= 40.0;
			else
				points[p.first] -= (40 + (k - 4) * 30);
		}
	}

    auto x = std::min_element(points.begin(), points.end(),
    [level](const std::pair<int, int>& p1, const std::pair<int, int>& p2) {
        return (p1.second + 70*std::abs(level - p1.first)) < (p2.second + 70*std::abs(level - p2.first)); });

	if (x != points.end())
		return x->first;
	
    return targetFloor;
}

HEADER_OLNY_INLINE MyStrategy::MyStrategy(Side side) : side(side)
{
	strategy1.side = side;
	strategy2.side = side;
	strategy3.side = side;
	strategy4.side = side;
	
	strategy1.firstMoveMinDest = 6;
	strategy2.firstMoveMinDest = 5;
	strategy3.firstMoveMinDest = 3;
	strategy4.firstMoveMinDest = 2;
}

HEADER_OLNY_INLINE MyStrategy::~MyStrategy()
{

}

HEADER_OLNY_INLINE void MyStrategy::makeMove()
{
	for (MyElevator & elevator : sim.elevators)
    {
        if (elevator.side == side && elevator.ind == 0)
        {
			strategy1.makeMove(sim, elevator, this);
        }
        else if (elevator.side == side && elevator.ind == 1)
        {
			strategy2.makeMove(sim, elevator, this);
        }
        else if (elevator.side == side && elevator.ind == 2)
        {
			strategy3.makeMove(sim, elevator, this);
        }
        else if (elevator.side == side && elevator.ind == 3)
        {
			strategy4.makeMove(sim, elevator, this);
		}
    }
}

HEADER_OLNY_INLINE void MyStrategy::makeMove(Simulator &inputSim)
{
    sim.synchronizeWith(inputSim);
    makeMove();
    inputSim.copyCommandsFrom(sim, side);
    sim.step();
}

HEADER_OLNY_INLINE void ElevatorStrategyUpDown::recalcDestinationBeforeDoorsClose(Simulator &sim, MyElevator &elevator, MyStrategy *strategy)
{
	int res = -100000;
	int targetFloor = -1;
	
	Side enemySide = inverseSide(strategy->side);
	int maxTicks = 7200 - sim.tick - 2;
	float coef = 1.0;
	if (maxTicks < 1500)
		coef = ((float)maxTicks - 300.0f) / 1200.0f;
	
	for (int i = 0; i < LEVELS_COUNT; ++i)
	{
		if (i != elevator.getFloor())
		{
			MyStrategy copy = *strategy;
			copy.strategy1.doPredictions = false;
			copy.strategy2.doPredictions = false;
			copy.strategy3.doPredictions = false;
			copy.strategy4.doPredictions = false;
			
			MyElevator &el = copy.sim.elevators[elevator.id];
			el.go_to_floor = i;
			
			int tick = 0;
			for (; tick < std::min(400 + elevator.ind * 40, maxTicks); ++tick)
			{
				//makeMoveSimple(enemySide, copy.sim);
				copy.makeMove();
				copy.sim.step();
			}
			
			int points;
			
			
			int valLeft, valRight;
			valLeft = copy.sim.scores[0] + copy.sim.passengersTotal[0]*1 + copy.sim.totalCargoValue2(Side::LEFT).points*0.5 * coef;
			valRight = copy.sim.scores[1] + copy.sim.passengersTotal[1]*1 + copy.sim.totalCargoValue2(Side::RIGHT).points*0.5 * coef;
			
			if (copy.side == Side::LEFT)
				points = valLeft - valRight;
			else
				points = valRight - valLeft;
			
			
			int passGone = 0;
			for (int id : elevator.passengers)
			{
				if (!el.passengers.count(id))
				{
					++passGone;
				}
			}
			
			if (passGone < 10 && i == 0)
				points -= 100;
			
			
			if (i == elevator.next_floor)
				++points;
			
			for (MyElevator &e : sim.elevators)
			{
				if (e.id != elevator.id && e.state == EState::MOVING && e.next_floor == i)
				{
					if (e.side == elevator.side && e.ind < elevator.ind)
					{
						points -= 200;
					}
				}
			}
			
			if (points > res)
			{
				res = points;
				targetFloor = i;
			}
		}
	}
	
	if (targetFloor != -1)
	{
		int prevTarget = elevator.next_floor;
		elevator.go_to_floor = targetFloor;
		//std::cout << " Target " << targetFloor << " OLD " << prevTarget << std::endl;
	}
}


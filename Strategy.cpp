#ifndef HEADER_OLNY_INLINE
#include "Strategy.hpp"
#endif
#include <cstdlib>
#include <set>
#include <algorithm>
#include <iostream>
#include <bitset>

#ifndef HEADER_OLNY_INLINE
#define HEADER_OLNY_INLINE
#define HEADER_OLNY_STATIC
#endif

const int ELEVATORS_COUNT = 4;

HEADER_OLNY_STATIC const char * PSTATE_NAMES[] = {
	"WAITING_FOR_ELEVATOR",
    "MOVING_TO_ELEVATOR",
    "RETURNING",
    "MOVING_TO_FLOOR",
    "USING_ELEVATOR",
    "EXITING",
	"OUT"
};
HEADER_OLNY_INLINE const char * getPStateName(PState s) {
	return PSTATE_NAMES[(int) s];
}

HEADER_OLNY_STATIC const char * ESTATE_NAMES[] = {
    "WATING",
    "MOVING",
    "OPENING",
    "FILLING",
    "CLOSING"
};
HEADER_OLNY_INLINE const char * getEStateName(EState s) {
	return ESTATE_NAMES[(int) s];
}

HEADER_OLNY_INLINE Simulator::Simulator()
{
	elevators.reserve(8);
	passengers.reserve(200);
	for (int i = 0; i < ELEVATORS_COUNT * 2; ++i)
	{
		MyElevator elevator;
		elevator.id = i;
		if (i < ELEVATORS_COUNT) {
			elevator.x = -60 - i * 80;
			elevator.y = 0;
			elevator.side = Side::LEFT;
			elevator.ind = i;
		}
		else
		{
			int k = i - ELEVATORS_COUNT;
			elevator.x = 60 + k * 80;
			elevator.y = 0;
			elevator.side = Side::RIGHT;
			elevator.ind = k;
		}
		
		elevator.speed = 0;
		elevator.time_on_the_floor_with_opened_doors = 0;
		elevator.closing_or_opening_ticks = 0;
		elevator.next_floor = -1;
		elevator.state = EState::FILLING;
		
		elevators.push_back(elevator);
	}
	addPassengers();
}

HEADER_OLNY_INLINE int Simulator::ramdomFloor()
{
	return random.get_random() % (LEVELS_COUNT - 1) + 1;
}

HEADER_OLNY_INLINE void Simulator::synchronizeWith(const Simulator &sim)
{
	for (const std::pair<const int, MyPassenger> & passenger : sim.passengers)
	{
		int id = passenger.first;
		const MyPassenger &otherPass = passenger.second;
		bool exists = passengers.count(id) > 0;
		MyPassenger &thisPass = passengers[id];
		
		thisPass.x = otherPass.x;
        thisPass.y = otherPass.y;
        thisPass.from_floor = otherPass.from_floor;
        thisPass.dest_floor = otherPass.dest_floor;
		thisPass.dest_floor_confirmed = true;
        thisPass.time_to_away = otherPass.time_to_away;
        thisPass.elevator = otherPass.elevator;
        thisPass.state = otherPass.state;
        thisPass.mass = otherPass.mass;
			
		if (!exists)
		{
			thisPass.spawn_x = thisPass.x > 0 ? 20.0 : -20.0;
			thisPass.side = otherPass.side;
			thisPass.placesRemained = 5 - thisPass.visitedLevels.count();
			if (thisPass.placesRemained > 4)
			{
				std::cout << "ERROR number of places" << sim.tick << " " << thisPass.placesRemained << " id " << thisPass.id << std::endl;
				thisPass.placesRemained = 4;
			}
			
			if (thisPass.placesRemained > 0)
				random.generateRandFloors(thisPass.places, thisPass.placesRemained, thisPass.visitedLevels);
		}
	}
	
	for (int i = 0; i < sim.elevators.size(); ++i)
	{
		const MyElevator &elevator = sim.elevators[i];
		MyElevator &thisElevator = elevators[i];
		thisElevator.x = elevator.x;
		thisElevator.y = elevator.y;
		thisElevator.speed = elevator.speed;
		if (elevator.state == EState::FILLING && thisElevator.state != EState::FILLING)
			thisElevator.time_on_the_floor_with_opened_doors = 0;
		thisElevator.next_floor = elevator.next_floor;
		thisElevator.state = elevator.state;
		thisElevator.passengers = elevator.passengers;
	}
}

HEADER_OLNY_INLINE void Simulator::copyCommandsFrom(const Simulator &sim, Side side)
{
	for (const std::pair<const int, MyPassenger> & passenger : sim.passengers)
	{
		int id = passenger.first;
		const MyPassenger &otherPass = passenger.second;
		bool exists = sim.passengers.count(id) > 0;
		if (!exists)
			std::cerr << "Passenger not found" << std::endl;
		
		MyPassenger &thisPass = passengers[id];
		thisPass.set_elevator.insert(otherPass.set_elevator.begin(), otherPass.set_elevator.end());
	}
	
	for (int i = 0; i < sim.elevators.size(); ++i)
	{
		const MyElevator &elevator = sim.elevators[i];
		if (elevator.side == side)
		{
			MyElevator &thisElevator = elevators[i];
			thisElevator.go_to_floor = elevator.go_to_floor;
		}
	}
}

HEADER_OLNY_INLINE void Simulator::step()
{
	tick++;
	
	applyElevatorGoToFloorCommands();
	appySetElevatorToPassengerCommands();
	simulateElevators();
	simulatePassengers();
	
	if (tick % 20 == 19)
	{
		if (tick <= 1979)
		{
			addPassengers();
		}
	}
}

HEADER_OLNY_INLINE double clamp(double val, double l, double r) {
	if (val < l)
		return l;
	
	if (val > r)
		return r;
	
	return val;
}

HEADER_OLNY_INLINE void Simulator::appySetElevatorToPassengerCommands()
{
	for (Passengers::iterator it = passengers.begin(); it != passengers.end();)
	{
		MyPassenger &passenger = it->second;
		
		if (passenger.elevator == -1 && passenger.set_elevator.size() > 0 && (passenger.state == PState::WAITING_FOR_ELEVATOR || passenger.state == PState::RETURNING))
		{
			double dist = 1e10;
			for (int elId : passenger.set_elevator)
			{
				MyElevator &elevator = elevators[elId];
				if (elevator.state == EState::FILLING && elevator.getFloor() == passenger.getFloor() && elevator.passengers.size() < MAX_PASSENGERS)
				{
					if (elevator.side != passenger.side && elevator.time_on_the_floor_with_opened_doors <= 40)
						continue;
					
					double d = std::abs(elevator.x - passenger.x);
					if (d < dist)
					{
						dist = d;
						passenger.elevator = elId;
					}
				}
			}
		}
		passenger.set_elevator.clear();
		
		++it;
	}
}

HEADER_OLNY_INLINE void Simulator::simulatePassengers()
{
	for (Passengers::iterator it = passengers.begin(); it != passengers.end();)
	{
		MyPassenger &passenger = it->second;
		
		MyElevator *elevator = nullptr;
		if (passenger.elevator != -1)
			elevator = &elevators[passenger.elevator];
		
		if (passenger.state != PState::USING_ELEVATOR && passenger.state != PState::MOVING_TO_FLOOR)
			passenger.time_to_away--;
		
		if (passenger.time_to_away < 0)
		{
			if (passenger.dest_floor > 0)
			{
				passenger.ticks = tick;
				passenger.state = PState::MOVING_TO_FLOOR;
				if (passenger.side == Side::LEFT)
					passenger.x = -400 - (passenger.dest_floor) * 10;
				else
					passenger.x = +400 + (passenger.dest_floor) * 10;
				passenger.time_to_away = 500;
				passenger.elevator = -1;
				
				int ticksToArrive = std::abs(passenger.dest_floor - passenger.from_floor) * 100;
				if (passenger.dest_floor > passenger.from_floor)
					ticksToArrive *= 2;
				
				passenger.y = passenger.dest_floor;
				passenger.visitedLevels.set(passenger.dest_floor);
				outPassengers.insert(std::make_pair(tick + ticksToArrive + 500, passenger));
			}
			it = passengers.erase(it);
			continue;
		}
		
		if (passenger.state == PState::WAITING_FOR_ELEVATOR)
		{
			if (elevator)
			{
				passenger.state = PState::MOVING_TO_ELEVATOR;
			}
		}
		else if (passenger.state == PState::MOVING_TO_ELEVATOR)
		{
			if (elevator->getFloor() != passenger.getFloor() || elevator->state != EState::FILLING)
			{
				passenger.state = PState::RETURNING;
				passenger.elevator = -1;
			}
			else
			{
				if (passenger.x == elevator->x)
				{
					if (elevator->passengers.size() < MAX_PASSENGERS)
					{
						passenger.state = PState::USING_ELEVATOR;
						elevator->passengers.insert(passenger.id);
					}
					else
					{
						passenger.state = PState::RETURNING;
						passenger.elevator = -1;
					}
				}
				else
				{
					passenger.x = clamp(elevator->x, passenger.x - 2.0, passenger.x + 2.0);
				}
			}
		}
		else if (passenger.state == PState::RETURNING)
		{
			if (passenger.x == passenger.spawn_x)
			{
				passenger.state = PState::WAITING_FOR_ELEVATOR;
			}
			else
			{
				passenger.x = clamp(passenger.spawn_x, passenger.x - 2.0, passenger.x + 2.0);
			}
		}
		else if (passenger.state == PState::USING_ELEVATOR)
		{
			passenger.y = elevator->y;
			if (elevator->state == EState::FILLING && passenger.dest_floor == passenger.y && elevator->time_on_the_floor_with_opened_doors == 1)
			{
				passenger.elevator = -1;
				passenger.state = PState::EXITING;
				passenger.ticks = tick;
				elevator->passengers.erase(passenger.id);
				int score = 10 * std::abs(passenger.dest_floor - passenger.from_floor);
				if (passenger.side != elevator->side)
					score *= 2;
				scores[(int) (elevator->side)] += score;
				scoresByElevators[elevator->id] += score;
				passengersTotal[(int) (elevator->side)]++;
				
				if (passenger.dest_floor > 0)
				{
					passenger.y = passenger.dest_floor;
					passenger.side = passenger.side;
					passenger.visitedLevels.set(passenger.dest_floor);
					if (passenger.visitedLevels.count() > 6)
					{
						std::cout << "ERR" << std::endl;
					}
					outPassengers.insert(std::make_pair(tick + 500 + 40 - 1, passenger));
				}
				it = passengers.erase(it);
				continue;
			}
		}

		++it;
	}
	
	while (outPassengers.size() > 0 && outPassengers.begin()->first < tick)
	{
		int id = outPassengers.begin()->second.id;
		passengers.insert(std::make_pair(id, outPassengers.begin()->second));
		outPassengers.erase(outPassengers.begin());
		
		MyPassenger &passenger = passengers[id];
		passenger.state = PState::WAITING_FOR_ELEVATOR;
		passenger.x = passenger.spawn_x;
		if (passenger.placesRemained --> 0)
		{
			passenger.dest_floor = passenger.places[passenger.placesRemained];
		}
		else
		{
			passenger.dest_floor = 0;
		}
		passenger.dest_floor_confirmed = false;
		passenger.from_floor = passenger.getFloor();
	}
}

HEADER_OLNY_INLINE void Simulator::applyElevatorGoToFloorCommands()
{
	for (MyElevator &elevator : elevators)
	{
		if (elevator.state != EState::MOVING)
		{
			if (elevator.go_to_floor != -1)
			{
				elevator.next_floor = elevator.go_to_floor;
			}
			/*if (elevator.next_floor == elevator.y)
			{
				elevator.next_floor = -1;
			}*/
		}
		
		elevator.go_to_floor = -1;
	}
}

HEADER_OLNY_INLINE void Simulator::simulateElevators()
{
	for (MyElevator &elevator : elevators)
	{
		if (elevator.state == EState::FILLING)
		{
			elevator.time_on_the_floor_with_opened_doors++;
			if (elevator.time_on_the_floor_with_opened_doors >= MIN_FILLING_TICS && elevator.next_floor != -1)
			{
				elevator.setState(EState::CLOSING, *this);
			}
		}
		else if (elevator.state == EState::CLOSING)
		{
			elevator.closing_or_opening_ticks++;
			if (elevator.closing_or_opening_ticks > CLOSING_TICS)
			{
				elevator.setState(EState::MOVING, *this);
			}
		}
		else if (elevator.state == EState::MOVING)
		{
			elevator.y = clamp(elevator.next_floor, elevator.y - 1.0f / 50.0f, elevator.y + elevator.speed);
			elevator.time_to_floor -= 1.0;
			
			if (elevator.time_to_floor <= 1e-5)
			{
				elevator.y = elevator.next_floor;
				elevator.setState(EState::OPENING, *this);
			}
		}
		else if (elevator.state == EState::OPENING)
		{
			elevator.closing_or_opening_ticks++;
			if (elevator.closing_or_opening_ticks >= OPENING_TICS)
			{
				elevator.setState(EState::FILLING, *this);
			}
		}
	}
}

HEADER_OLNY_INLINE void MyElevator::setState(EState state, Simulator &sim)
{
	if (state == EState::CLOSING && this->state == EState::FILLING)
	{
		closing_or_opening_ticks = 0;
	}
	else if (state == EState::OPENING && this->state == EState::MOVING)
	{
		closing_or_opening_ticks = 0;
	}
	else if (state == EState::FILLING && this->state == EState::OPENING)
	{
		time_on_the_floor_with_opened_doors = 0;
		next_floor = -1;
	}
	else if (state == EState::MOVING && this->state == EState::CLOSING)
	{
		if (passengers.size() <= 10)
			speed = 1.0f / 50.0f;
		else
			speed = 1.0f / 50.0f / 1.1f;
		
		float totalMass = 1.0f;

		for (int id : passengers)
		{
			if (!passengers.count(id))
				std::cerr << sim.tick << " ERR PASS " << id << std::endl;
			
			MyPassenger &pass = sim.passengers[id];
			totalMass *= pass.mass;
		}
		speed /= totalMass;
		
		if (next_floor > getFloor())
			time_to_floor = (next_floor - getFloor()) / speed;
		else
			time_to_floor = (getFloor() - next_floor) * 50.0;
	}
	
	this->state = state;
}

HEADER_OLNY_INLINE double MyElevator::calcTimeToFloor(double f, Simulator &sim) const
{
	double speed;
	double time_to_floor;
	if (passengers.size() <= 10)
		speed = 1.0f / 50.0f;
	else
		speed = 1.0f / 50.0f / 1.1f;
	
	float totalMass = 1.0f;

	for (int id : passengers)
	{
		if (!passengers.count(id))
			std::cerr << sim.tick << " ERR PASS " << id << std::endl;
		
		MyPassenger &pass = sim.passengers[id];
		totalMass *= pass.mass;
	}
	speed /= totalMass;
	
	if (f > getFloor())
		time_to_floor = (f - getFloor()) / speed;
	else
		time_to_floor = (getFloor() - f) * 50.0;
	return time_to_floor;
}

HEADER_OLNY_INLINE void Simulator::addPassengers()
{
	size_t places = random.get_random() % 5 + 1;
	int placesArr[LEVELS_COUNT];
	std::bitset<LEVELS_COUNT> visitedLevels;
	visitedLevels.set(0);
	random.generateRandFloors(placesArr, places, visitedLevels);
		
	float mass = 1.01f + ((float) (random.get_random() % 20000)) / 1000000.0f;
	
	for (int i = 0; i < 2; ++i)
	{
		MyPassenger passenger;
		passenger.visitedLevels = visitedLevels;
		passenger.id = ++cur_pass_seq;
		if (i == 0) {
			passenger.x = -20;
			passenger.spawn_x = -20;
			passenger.side = Side::LEFT;
		} else {
			passenger.x = 20;
			passenger.spawn_x = 20;
			passenger.side = Side::RIGHT;
		}
		passenger.y = 0;
		passenger.from_floor = 0;
		passenger.dest_floor = placesArr[places - 1];
		for (size_t i = 0; i < places; ++i)
			passenger.places[i] = placesArr[i];
		passenger.time_to_away = 500;
		passenger.elevator = -1;
		passenger.placesRemained = places - 1;
		passenger.mass = mass;
		passenger.state = PState::WAITING_FOR_ELEVATOR;
		
		passengers.insert(std::make_pair(passenger.id, passenger));
	}
}

HEADER_OLNY_INLINE void Simulator::randomizePassenger(MyPassenger &passenger)
{
	std::bitset<LEVELS_COUNT> visitedLevels = passenger.visitedLevels;
	bool visitedDestination = visitedLevels.test(passenger.dest_floor);
	if (!visitedDestination)
		visitedLevels.set(passenger.dest_floor);
	
	/*passenger.placesRemained = 5 - (int) visitedLevels.count();
	if (passenger.placesRemained > 3)
	{
		std::cout << "ERROR2 number of places " << tick << " " << passenger.placesRemained << " id " << passenger.id << std::endl;
		passenger.placesRemained = 3;
	}*/
	
	if (passenger.placesRemained > 0)
		random.generateRandFloors(passenger.places, passenger.placesRemained, visitedLevels);
	
	/*if (!visitedDestination)
	{
		passenger.placesRemained++;
		//std::cout << passenger.placesRemained << std::endl;
		passenger.places[passenger.placesRemained] = passenger.dest_floor;
	}*/
}

HEADER_OLNY_INLINE void Simulator::randomizePassengers()
{
	for (Passengers::iterator it = passengers.begin(); it != passengers.end(); ++it)
	{
		MyPassenger &passenger = it->second;
		randomizePassenger(passenger);
	}
	
	for (auto && it = outPassengers.begin(); it != outPassengers.end();  ++it)
	{
		MyPassenger &passenger = it->second;
		randomizePassenger(passenger);
	}
}

HEADER_OLNY_INLINE Value Simulator::getCargoValue(MyElevator &elevator)
{
    Value result;

    std::bitset<LEVELS_COUNT> levels;
    levels.set(0);

    int minFloor = elevator.getFloor();
    int maxFloor = minFloor;
    int floor = elevator.getFloor();
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = passengers[id];
        int pts = std::abs(pass.dest_floor - pass.from_floor) * 10;
		if (elevator.side != pass.side)
			pts *= 2;
		result.points += pts;
        minFloor = std::min(minFloor, pass.dest_floor);
        maxFloor = std::max(maxFloor, pass.dest_floor);
        if (pass.dest_floor != floor)
        {
            levels.set(pass.dest_floor);
        }
    }

    result.ticks = (maxFloor - minFloor + std::min(maxFloor - floor, floor - minFloor)) * 10 * 60 + levels.count() * 250;
	result.uniqueLevels = levels.count();
	
    return result;
}

HEADER_OLNY_INLINE Value Simulator::getCargoValueBug(MyElevator &elevator)
{
    Value result;

    std::bitset<LEVELS_COUNT> levels;
    levels.set(0);

    int minFloor = elevator.getFloor();
    int maxFloor = minFloor;
    int floor = elevator.getFloor();
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = passengers[id];
        int pts = std::abs(pass.dest_floor - pass.from_floor) * 10;
		if (elevator.side != pass.side)
			pts *= 2;
		result.points += pts;
        minFloor = std::min(minFloor, pass.dest_floor);
        maxFloor = std::max(maxFloor, pass.dest_floor);
        if (pass.dest_floor != floor)
        {
            levels.set(pass.dest_floor);
        }
    }

    result.ticks = (maxFloor - minFloor + std::min(maxFloor - floor, floor - minFloor)) * 10 * 60 + levels.size() * 250;
	result.uniqueLevels = levels.count();
	
    return result;
}

HEADER_OLNY_INLINE Value Simulator::totalCargoValue(Side side)
{
	Value res;
	for (MyElevator &elevator : elevators)
	{
		if (elevator.side == side)
		{
			Value val = getCargoValue(elevator);
			res.points += val.points;
			res.ticks = std::max(res.ticks, val.ticks);
		}
	}
	
	return res;
}

HEADER_OLNY_INLINE Value Simulator::getCargoValue2(MyElevator &elevator)
{
    Value result;

    std::bitset<LEVELS_COUNT> levels;
    levels.set(0);

    int minFloor = elevator.getFloor();
    int maxFloor = minFloor;
    int floor = elevator.getFloor();
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = passengers[id];
        int pts = pass.getValue2(elevator.side);
		result.points += pts;
        minFloor = std::min(minFloor, pass.dest_floor);
        maxFloor = std::max(maxFloor, pass.dest_floor);
        if (pass.dest_floor != floor)
        {
            levels.set(pass.dest_floor);
        }
    }

    result.ticks = (maxFloor - minFloor + std::min(maxFloor - floor, floor - minFloor)) * 10 * 60 + levels.count() * 250;
	result.uniqueLevels = levels.count();
	
    return result;
}

HEADER_OLNY_INLINE Value Simulator::totalCargoValue2(Side side)
{
	Value res;
	for (MyElevator &elevator : elevators)
	{
		if (elevator.side == side)
		{
			Value val = getCargoValue2(elevator);
			res.points += val.points;
			res.ticks = std::max(res.ticks, val.ticks);
		}
	}
	
	return res;
}

HEADER_OLNY_INLINE double MyPassenger::getValue2(Side mySide) const
{
	double res;
		
	if (dest_floor_confirmed)
	{
		res = std::abs(dest_floor - from_floor) * 10;
	}
	else
	{
		int cnt = visitedLevels.count();
		if (cnt < 1 || cnt > 6)
			std::cout << "ERR invalid visitedLevels "  << cnt << std::endl;
		
		double avgDest = 0.0;
		int k = 0;
		for (int i = 0; i < LEVELS_COUNT; ++i)
		{
			if (!visitedLevels.test(i))
			{
				avgDest += std::abs(i - from_floor);
				++k;
				
				if (i == from_floor)
				{
					std::cout << "ERR2 invalid visitedLevels "  << i << std::endl;
				}
			}
		}
		
		if (k > 0)
			avgDest /= k;
		
		res = (((cnt - 1) * 0.2 * std::abs(from_floor - 0)) + ((6 - cnt) * 0.2 * avgDest)) * 10.0;
	}
	
	if (side != mySide)
		res *= 2.0;
	return res;
}


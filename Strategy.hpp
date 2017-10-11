#ifndef STRATEGY_HPP
#define STRATEGY_HPP

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <bitset>

constexpr int LEVELS_COUNT = 9;
constexpr int OPENING_TICS = 100;
constexpr int CLOSING_TICS = 100;
constexpr int MIN_FILLING_TICS = 40;
constexpr int MAX_PASSENGERS = 20;

enum class PState
{
    WAITING_FOR_ELEVATOR,
    MOVING_TO_ELEVATOR,
    RETURNING,
    MOVING_TO_FLOOR,
    USING_ELEVATOR,
    EXITING,
	OUT,
	COUNT
};

enum class Side {
    LEFT, RIGHT
};

inline Side inverseSide(Side side)
{
	if (side == Side::LEFT)
		return Side::RIGHT;
	
	return Side::LEFT;
}

enum class EState
{
    WAITING,
    MOVING,
    OPENING,
    FILLING,
    CLOSING
};

const char * getPStateName(PState s);
const char * getEStateName(EState s);

struct MyPassenger
{
	int id;             // идентификатор
    double x, y;           // координаты
    double spawn_x;
    int from_floor, dest_floor; // этаж появления и цели
    int time_to_away;
    Side side;
    int elevator;    // идентификатор назначенного лифта
    PState state;
	int ticks = 0;
	int placesRemained;
	int places[5];
	std::bitset<LEVELS_COUNT> visitedLevels;
	float mass;
	bool dest_floor_confirmed;
	
	int getFloor() const
	{
		return y;
	}
	
	int getValue(Side mySide) const
	{
		int res = std::abs(dest_floor - from_floor) * 10;
		if (side != mySide)
			res *= 2;
		return res;
	}
	
	double getValue2(Side mySide) const;
	
	std::set<int> set_elevator;
};

struct Simulator;

struct MyElevator
{
	int id;
	int ind;
    double x, y;
    double speed;
    int time_on_the_floor_with_opened_doors;
	int closing_or_opening_ticks;
    int next_floor;
	double time_to_floor;
    Side side;
    EState state;
	std::set<int> passengers;
	
	int go_to_floor = -1;
	
	int getFloor() const
	{
		return y;
	}
	
	void setState(EState state, Simulator &sim);
	double calcTimeToFloor(double f, Simulator &sim) const;
};

struct Value {
	double points = 0.0;
	int ticks = 0;
	int uniqueLevels = 0;
};

struct Random
{
	uint32_t m_w = 12345;    /* must not be zero, nor 0x464fffff */
	uint32_t m_z = 456345;    /* must not be zero, nor 0x9068ffff */

	uint32_t get_random()
	{
		m_z = 36969 * (m_z & 65535) + (m_z >> 16);
		m_w = 18000 * (m_w & 65535) + (m_w >> 16);
		return (m_z << 16) + m_w;  /* 32-bit result */
	}
	
	int generateRandFloors(int floors_out[], int size, const std::bitset<LEVELS_COUNT> &visitedLevels)
	{
		int levels[LEVELS_COUNT - 1];
		int j = 0;
		for (int i = 0; i < LEVELS_COUNT; ++i)
		{
			if (!visitedLevels.test(i))
				levels[j++] = i;
		}
		
		if (j > 1)
			std::random_shuffle(levels, levels + j, [this](int n){return get_random() % n;});
		
		if (j > size)
			j = size;
		
		std::copy(levels, levels + j, floors_out);
		return j;
	}
};

struct Simulator
{
	Simulator();
	
	void synchronizeWith(const Simulator &sim);
	void copyCommandsFrom(const Simulator &sim, Side side);
	
	typedef std::unordered_map<int, MyPassenger> Passengers;
	Passengers passengers;
	
	std::vector<MyElevator> elevators;
	std::multimap<int, MyPassenger> outPassengers;
	int scores[2] = {};
	int scoresByElevators[8] = {};
	int passengersTotal[2] = {};
	
	int tick = -1;
	int cur_pass_seq = 0;
	void step();
	void applyElevatorGoToFloorCommands();
	void appySetElevatorToPassengerCommands();
	void simulatePassengers();
	void simulateElevators();
	void addPassengers();
	void randomizePassenger(MyPassenger &passenger);
	void randomizePassengers();
	Random random;
	
	Value getCargoValue(MyElevator &elevator);
	Value getCargoValueBug(MyElevator &elevator);
	Value totalCargoValue(Side side);
	
	Value getCargoValue2(MyElevator &elevator);
	Value totalCargoValue2(Side side);
	int ramdomFloor();
};

#endif


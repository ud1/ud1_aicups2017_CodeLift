#include <iostream>

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <boost/timer/timer.hpp>
#include <vector>
#include <set>
#include <sstream>
#include <algorithm>
#include "MyUtils.hpp"

#define NANOVG_GL2_IMPLEMENTATION	// Use GL2 implementation.
#include "nanovg.h"
#include "nanovg_gl.h"
#include "Strategy.hpp"
#include "mystrategy.hpp"

struct Renderer {
	void init();
	void startRendering();
	void finishRendering();
	void renderPoint(const P &p);
};

constexpr int WIDTH = 1300;
constexpr int HEIGHT = 1000;

SDL_Window *window;
SDL_GLContext glContext;
NVGcontext* vg;

double SCALE = 0.9;
double zoom = 1.0;
P zoomCenter = P(0.5, 0.5);

std::vector<P> points;

void Renderer::init()
{
	int width = (int) (WIDTH * SCALE);
	int height = (int) (HEIGHT * SCALE);

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
			return;

	window = SDL_CreateWindow("My Game Window",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		width, height,
		SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);

	glContext = SDL_GL_CreateContext(window);
	if (glContext == NULL)
	{
		printf("There was an error creating the OpenGL context!\n");
		return;
	}

	const unsigned char *version = glGetString(GL_VERSION);
	if (version == NULL)
	{
		printf("There was an error creating the OpenGL context!\n");
		return;
	}

	SDL_GL_MakeCurrent(window, glContext);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetSwapInterval(1);

	//MUST make a context AND make it current BEFORE glewInit()!
	glewExperimental = GL_TRUE;
	GLenum glew_status = glewInit();
	if (glew_status != 0)
	{
		fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_status));
		return;
	}

	glViewport( 0, 0, width, height);
}

void updateZoom()
{
    glMatrixMode(GL_PROJECTION);

    P center = zoomCenter * P(WIDTH, HEIGHT);
    P c1 = center - P(WIDTH, HEIGHT) * 0.5 / zoom;
    P c2 = center + P(WIDTH, HEIGHT) * 0.5 / zoom;

    if (c2.x > WIDTH)
    {
        double d = c2.x - WIDTH;
        c1.x -= d;
        c2.x -= d;
    }

    if (c2.y > HEIGHT)
    {
        double d = c2.y - HEIGHT;
        c1.y -= d;
        c2.y -= d;
    }

    if (c1.x < 0)
    {
        double d = -c1.x;
        c1.x += d;
        c2.x += d;
    }

    if (c1.y < 0)
    {
        double d = -c1.y;
        c1.y += d;
        c2.y += d;
    }

    zoomCenter = (c1 + c2) / 2.0 / P(WIDTH, HEIGHT);

    //LOG("ZOOM " << zoomCenter);
    glLoadIdentity();
    glOrtho(c1.x, c2.x, c2.y, c1.y, 0.0, 1.0);
}

void Renderer::startRendering()
{
	points.clear();
}

void renderCircle(P p, double rad)
{
	glBegin ( GL_TRIANGLE_FAN );

	glVertex2d ( p.x, p.y );

	double N = 200;

	for ( double i = 0; i <= N; ++i ) {
		double alpha = i * PI * 2.0 / N;
		P p2 = P (rad  * std::cos ( alpha ), rad * std::sin ( alpha ) ) + p;
		glVertex2d ( p2.x, p2.y );
	}

	glEnd();
}

float floorCoord(float y)
{
	return 950.0f - y*100.0f;
}

Simulator *g_realsimulator = nullptr;
Simulator *g_mysimulator = nullptr;
bool renderMySim = false;

void renderPassenger(MyPassenger &passenger, bool isOut)
{
	Simulator *g_simulator = renderMySim ? g_mysimulator : g_realsimulator;
	
	float passW2 = 10;
	float passH = 40;
	
	double y = passenger.y;
	
	if (isOut)
	{
		if (passenger.state == PState::MOVING_TO_FLOOR)
		{
			int ticksPassed = g_simulator->tick - passenger.ticks;
			int ticksToArrive = std::abs(passenger.dest_floor - passenger.from_floor) * 100;
			if (passenger.dest_floor > passenger.from_floor)
				ticksToArrive *= 2;
			
			if (ticksPassed > ticksToArrive)
				return;
			
			y = passenger.from_floor + (float)(passenger.dest_floor - passenger.from_floor) * (float) ticksPassed / (float) ticksToArrive;
		}
		else if (passenger.state == PState::EXITING)
		{
			int ticksPassed = g_simulator->tick - passenger.ticks;
			if (ticksPassed > 40)
				return;
		}
	}
	
	if (passenger.state == PState::WAITING_FOR_ELEVATOR)
		glColor3f(0.7, 0.7, 0.7);
	else if (passenger.state == PState::MOVING_TO_ELEVATOR)
		glColor3f(0.2, 0.8, 0.2);
	else if (passenger.state == PState::RETURNING)
		glColor3f(0.8, 0.2, 0.2);
	else if (passenger.state == PState::MOVING_TO_FLOOR)
		glColor3f(0.9, 0.0, 0.0);
	else if (passenger.state == PState::USING_ELEVATOR)
		glColor3f(0.0, 0.9, 0.0);
	else if (passenger.state == PState::EXITING)
		glColor3f(0.0, 0.5, 0.9);
	else
		glColor3f(0.0, 0.0, 0.0);
	
	float y1 = floorCoord(y);
	float y2 = y1 - passH;
	
	float passX = WIDTH / 2.0f + passenger.x;
	glBegin(GL_LINE_STRIP);
	glVertex2f(passX - passW2, y1);
	glVertex2f(passX + passW2, y1);
	glVertex2f(passX + passW2, y2);
	glVertex2f(passX - passW2, y2);
	glVertex2f(passX - passW2, y1);
	glEnd();
	
	int idx = passenger.id / 20;
	int idy = passenger.id % 20;
	
	float dd = 0.1;
	float x1 = passX + idx / 5.0f * passW2 - passW2 + dd;
	float x2 = passX + (idx + 1) / 5.0f * passW2 - passW2 - dd;
	
	y1 = floorCoord(y) - idy / 20.0f * passH + dd;
	y2 = floorCoord(y) - (idy + 1) / 20.0f * passH - dd;
	
	glBegin(GL_TRIANGLES);
	if ((float) passenger.dest_floor > y)
	{
		glVertex2f(x1, y1);
		glVertex2f(x2, y1);
		glVertex2f((x1 + x2)*0.5f, y2);
	}
	else
	{
		glVertex2f((x1 + x2)*0.5f, y1);
		glVertex2f(x2, y2);
		glVertex2f(x1, y2);
	}
	glEnd();
}

void renderSimulator()
{
	Simulator *g_simulator = renderMySim ? g_mysimulator : g_realsimulator;
	if (!g_simulator)
		return;
	
	glColor3f(0.9, 0.9, 0.9);
	
	glBegin(GL_LINES);
	glVertex2f(WIDTH / 2.0f, 0.0f);
	glVertex2f(WIDTH / 2.0f, HEIGHT);
	glEnd();
	
	glColor3f(0.7, 0.7, 0.7);
	
	for (MyElevator &el : g_simulator->elevators)
	{
		float x = WIDTH / 2.0f + el.x;
		glBegin(GL_LINES);
		glVertex2f(x, 0.0f);
		glVertex2f(x, HEIGHT);
		glEnd();
	}
	
	for (int y = 0; y < LEVELS_COUNT; ++y)
	{
		glBegin(GL_LINES);
		glVertex2f(0, floorCoord(y));
		glVertex2f(WIDTH, floorCoord(y));
		glEnd();
	}
	
	const float elevatorW05 = 25.0;
	for (MyElevator &el : g_simulator->elevators)
	{
		if (el.state == EState::FILLING)
			glColor3f(0.9, 0.9, 0.0);
		else if (el.state == EState::MOVING || el.state == EState::OPENING || el.state == EState::CLOSING)
			glColor3f(0.4, 0.4, 0.2);
		else
			glColor3f(0.9, 0.9, 0.0);
		
		float x = WIDTH / 2.0f + el.x;
		float y1 = floorCoord(el.y + 0.9f);
		float y2 = floorCoord(el.y);

		glBegin(GL_QUADS);
		glVertex2f(x - elevatorW05, y1);
		glVertex2f(x + elevatorW05, y1);
		glVertex2f(x + elevatorW05, y2);
		glVertex2f(x - elevatorW05, y2);
		glEnd();
		
		if (el.state == EState::OPENING || el.state == EState::CLOSING) {
			glColor3f(0.9, 0.9, 0.0);
			
			float w = (el.state == EState::OPENING) ? (el.closing_or_opening_ticks / 100.0) : (1.0 - el.closing_or_opening_ticks / 100.0);
			w *= elevatorW05;
			glBegin(GL_QUADS);
			glVertex2f(x - w, y1);
			glVertex2f(x + w, y1);
			glVertex2f(x + w, y2);
			glVertex2f(x - w, y2);
			glEnd();
		}
	}
	
	int passByState[(int) PState::COUNT] = {};
	for (std::pair<const int, MyPassenger> & passengerEntry : g_simulator->passengers)
	{
		MyPassenger &passenger = passengerEntry.second;
		renderPassenger(passenger, false);
		passByState[(int) passenger.state]++;
	}
	
	glColor3d(1.0, 0.0, 0.0);
		//renderCircle(P(100, 100), 50);
		
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    nvgBeginFrame(vg, w, h, 1);

    nvgFontFace(vg, "sans");
    nvgFontSize(vg, 15.0f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 255));
	std::stringstream oss;
	oss << "Tick " << g_simulator->tick;
    nvgText(vg, 10, 20, oss.str().c_str(), nullptr);
	
	passByState[(int) PState::OUT] = g_simulator->outPassengers.size();
	for (int i = 0; i < (int) PState::COUNT; ++i)
	{
		oss.str("");
		oss << getPStateName((PState) i) << ": " << passByState[i];
		nvgText(vg, 10, 20 + (i + 1) * 15, oss.str().c_str(), nullptr);
	}
	
	nvgFontSize(vg, 25.0f);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
	oss.str("");
	Value valLeft = g_simulator->totalCargoValue2(Side::LEFT);
	oss << g_simulator->scores[0] << "/" << valLeft.points << "/" << valLeft.ticks;
	nvgText(vg, 20, h - 30, oss.str().c_str(), nullptr);
	
	nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
	oss.str("");
	Value valRight = g_simulator->totalCargoValue2(Side::RIGHT);
	oss << g_simulator->scores[1] << "/" << valRight.points << "/" << valRight.ticks;
	nvgText(vg, w - 20, h - 30, oss.str().c_str(), nullptr);
	
	for (MyElevator &el : g_simulator->elevators)
	{
		float y = floorCoord(el.y);
		float x = WIDTH / 2.0f + el.x;
		
		nvgFontSize(vg, 20.0f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
		oss.str("");
		oss << "->" << el.next_floor << " " << el.passengers.size();
		
		P mousePos = (P(x / WIDTH, y / HEIGHT) - zoomCenter) * zoom + P(0.5, 0.5);
		P p = mousePos * P(WIDTH * SCALE, HEIGHT*SCALE);
		nvgText(vg, p.x, p.y, oss.str().c_str(), nullptr);
	}
	
	int leftOutPass[LEVELS_COUNT] = {};
	int rightOutPass[LEVELS_COUNT] = {};
	int leftOutTick[LEVELS_COUNT] = {};
	int rightOutTick[LEVELS_COUNT] = {};
	
	for (std::pair<const int, MyPassenger> &p : g_simulator->outPassengers)
	{
		MyPassenger &passenger = p.second;
		renderPassenger(passenger, true);
		passByState[(int) passenger.state]++;
		
		int tick = p.first;
		int floor = p.second.getFloor();
		if (p.second.side == Side::LEFT)
		{
			leftOutPass[floor]++;
			if (!leftOutTick[floor])
				leftOutTick[floor] = tick;
		}
		else
		{
			rightOutPass[floor]++;
			if (!rightOutTick[floor])
				rightOutTick[floor] = tick;
		}
	}
	
	for (int i = 0; i < LEVELS_COUNT; ++i)
	{
		float y = floorCoord(i);
		
		nvgFontSize(vg, 15.0f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		
		P mousePos = (P(-350.0 / WIDTH + 0.5, y / HEIGHT) - zoomCenter) * zoom + P(0.5, 0.5);
		P p = mousePos * P(WIDTH * SCALE, HEIGHT*SCALE);
		
		oss.str("");
		oss << leftOutPass[i];
		if (leftOutPass[i])
			oss << " - " << (leftOutTick[i] - g_simulator->tick);
		nvgText(vg, p.x, p.y, oss.str().c_str(), nullptr);
		
		mousePos = (P(+350.0 / WIDTH + 0.5, y / HEIGHT) - zoomCenter) * zoom + P(0.5, 0.5);
		p = mousePos * P(WIDTH * SCALE, HEIGHT*SCALE);
		
		oss.str("");
		oss << rightOutPass[i];
		if (rightOutPass[i])
			oss << " - " << (rightOutTick[i] - g_simulator->tick);
		nvgText(vg, p.x, p.y, oss.str().c_str(), nullptr);
	}

    /*P mousePos = (P(100, 100) / WIDTH - zoomCenter) * zoom + P(0.5, 0.5);
    P p = mousePos * P(WIDTH * SCALE, HEIGHT*SCALE);
    nvgText(vg, p.x, p.y, oss.str().c_str(), nullptr);*/

    nvgEndFrame(vg);
}

int renderTickMod = 1;

void Renderer::finishRendering()
{
	static std::set<int> keys, pressedKeys;
	static bool pause = true;
	static double delay = 20.0;
	static P mousePos = P(0.0, 0.0);
    static bool lButton = false;
    static bool updateTitle = true;
    static P titlePos = P(0, 0);

	do
	{
		pressedKeys.clear();
		SDL_Event e;
		while ( SDL_PollEvent(&e) )
		{
			if (e.type == SDL_KEYDOWN)
			{
				keys.insert(e.key.keysym.sym);
				pressedKeys.insert(e.key.keysym.sym);
				std::cout << "PRESS " << e.key.keysym.sym << std::endl;
			}
			if (e.type == SDL_KEYUP)
			{
				keys.erase(e.key.keysym.sym);
			}

            if (e.type == SDL_MOUSEMOTION)
            {
                mousePos = P(e.motion.x, e.motion.y) / P(WIDTH * SCALE, HEIGHT*SCALE);
                if (lButton)
                {
                    zoomCenter -= P(e.motion.xrel, e.motion.yrel) / P(WIDTH * SCALE, HEIGHT*SCALE) / zoom;
                }
            }

            if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                if (e.button.button == SDL_BUTTON_LEFT)
                    lButton = true;
            }

            if (e.type == SDL_MOUSEBUTTONUP)
            {
                if (e.button.button == SDL_BUTTON_LEFT)
                    lButton = false;
            }

            if (e.type == SDL_MOUSEWHEEL)
            {
                P zoomPoint = zoomCenter + (mousePos - P(0.5, 0.5))/zoom;

                int w = e.wheel.y;
                if (w > 0)
                {
                    for (int i = 0; i < w; ++i)
                        zoom *= 1.2;
                } else
                {
                    for (int i = 0; i < -w; ++i)
                        zoom /= 1.2;
                }

                if (zoom < 1.0)
                    zoom = 1.0;

                zoomCenter = zoomPoint - (mousePos - P(0.5, 0.5))/zoom;
            }
		}

		if (pressedKeys.count(SDLK_SPACE))
		{
			pause = !pause;
			keys.erase(SDLK_SPACE);
			SDL_Delay(200);
		}

        if (pressedKeys.count(SDLK_KP_0))
        {
            LOG("CUR SIM " << currentSimulation);
        }

        if (pressedKeys.count(SDLK_KP_1))
        {
            updateTitle = !updateTitle;
        }
		
		if (pressedKeys.count(SDLK_KP_MULTIPLY))
		{
			renderMySim = !renderMySim;
			if (renderMySim)
				std::cout << "Render my sim" << std::endl;
			else
				std::cout << "Render real sim" << std::endl;
		}
		
		if (pressedKeys.count(SDLK_KP_PLUS))
		{
            renderTickMod++;
		}
		else if (pressedKeys.count(SDLK_KP_MINUS))
		{
			if (renderTickMod > 1)
				renderTickMod--;
		}

		if (keys.count(SDLK_UP))
		{
			delay /= 1.5;
		}

		if (keys.count(SDLK_DOWN))
		{
			delay *= 1.5;
		}
		
		if (pressedKeys.count(SDLK_ESCAPE))
		{
			exit(0);
		}

		glClearColor(1.0, 1.0, 1.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glClear(GL_DEPTH_BUFFER_BIT);
        updateZoom();
				
		#ifdef SIMULATE
		//renderFitMap();
		//::renderExperimentCarCircleCollision();
		//::renderExperimentCarTraj();
		//::renderExperimentCarWallCollision();
		//::renderExperimentCarCarCollision();
		//::renderExperimentOilSlickEvasion();
		//::renderExperimentBonuses();
		//::renderCarRot();
		#endif
		
		renderSimulator();
		
		


		SDL_GL_SwapWindow(window);

		if (pressedKeys.count(SDLK_RIGHT))
		{
			keys.erase(SDLK_RIGHT);


			return;
		}

		if (delay > 1)
			SDL_Delay((int) delay);
	} while(pause);
}

void makeMove(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				if (passenger.side == side)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
		}
	}
}

void makeMove2(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				//if (passenger.side == side)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
            
            if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 300 && elevator.passengers.size() < 10)
				elevator.go_to_floor = -1;
		}
	}
}

void makeMove3(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				int elAbsX = std::abs(elevator.x) ;
				if (elAbsX < 70 || elAbsX < 150 && passenger.dest_floor < 5 || elAbsX >= 150 && passenger.dest_floor >= 5)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
            
            if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
				elevator.go_to_floor = -1;
		}
	}
}

void makeMove4(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				int elAbsX = std::abs(elevator.x) ;
				int ind = 0;
				if (elAbsX < 70)
					ind = 0;
				else if (elAbsX < 150)
					ind = 1;
				else if (elAbsX < 230)
					ind = 2;
				else
					ind = 3;
				
				int dest_floor = passenger.dest_floor;
				if (ind == 0 || ind == 1 && dest_floor < 5 || ind == 2 && dest_floor >= 5 || ind == 3 && dest_floor >= 4)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
            
            if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
				elevator.go_to_floor = -1;
		}
	}
}

void makeMove5(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			int elAbsX = std::abs(elevator.x) ;
            int ind = 0;
            if (elAbsX < 70)
                ind = 0;
            else if (elAbsX < 150)
                ind = 1;
            else if (elAbsX < 230)
                ind = 2;
            else
                ind = 3;
				
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				
				int dest_floor = passenger.dest_floor;
				if (ind == 0 || ind == 1 && dest_floor < 5 || ind == 2 && dest_floor >= 5 || ind == 3 && dest_floor >= 4)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
            
            size_t sz = elevator.passengers.size() ;
			
			if (elevator.time_on_the_floor_with_opened_doors < 200 && sz < 10)
				elevator.go_to_floor = -1;
			
            //if (elevator.getFloor() == 0 && sim.tick < 2000 && (ind == 0 && sz < 20 || ind > 0 && sz < 20) || elevator.time_on_the_floor_with_opened_doors < 200 && sz < 4)
			//	elevator.go_to_floor = -1;
		}
	}
}

void makeMove6(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				int elAbsX = std::abs(elevator.x) ;
				int ind = 0;
				if (elAbsX < 70)
					ind = 0;
				else if (elAbsX < 150)
					ind = 1;
				else if (elAbsX < 230)
					ind = 2;
				else
					ind = 3;
				
				int dest_floor = passenger.dest_floor;
				if (ind == 0 && dest_floor >= 7 || ind == 1 && dest_floor >= 5 || ind == 2 && dest_floor >= 4 || ind == 3)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
            
            if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
				elevator.go_to_floor = -1;
		}
	}
}

void makeMove7(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				int elAbsX = std::abs(elevator.x) ;
				int ind = 0;
				if (elAbsX < 70)
					ind = 0;
				else if (elAbsX < 150)
					ind = 1;
				else if (elAbsX < 230)
					ind = 2;
				else
					ind = 3;
				
				int dest_floor = passenger.dest_floor;
				if (ind == 0 && dest_floor >= 7 || ind == 1 && dest_floor >= 6 || ind == 2 && dest_floor >= 5 || ind == 3 && dest_floor >= 5)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
            
            if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
				elevator.go_to_floor = -1;
		}
	}
}

int getNearestToLevelDestination(Simulator &sim, MyElevator& elevator, int level);

struct StratE1
{
	Side side;
	Direction dir = Direction::UP;
	StratE1(Side side) : side(side)
	{
	}
	
	bool reachedTop = false;
	void makeMove(Simulator &sim)
	{
		for (MyElevator & elevator : sim.elevators)
		{
			if (elevator.side == side && elevator.ind == 0)
			{
				if (!reachedTop && elevator.getFloor() > 0 && elevator.state == EState::OPENING && getNearestToLevelDestination(sim, elevator, 100) <= elevator.getFloor())
				{
					reachedTop = true;
					dir = Direction::DOWN;
				}
				
				for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
				{
					MyPassenger &passenger = passengerEntry.second;
					int dest_floor = passenger.dest_floor;
					if (elevator.state == EState::FILLING && passenger.getFloor() == elevator.getFloor() && passenger.state == PState::WAITING_FOR_ELEVATOR)
					if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
					{
						if (elevator.state == EState::FILLING && passenger.getFloor() == elevator.getFloor() && passenger.state == PState::WAITING_FOR_ELEVATOR)
						{
							if (dir == Direction::UP && sim.tick < 2000 && dest_floor > 7)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
							
							if (dir == Direction::DOWN && dest_floor <= 4)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
						}
					}
				}
				
                if (elevator.passengers.size() == MAX_PASSENGERS || elevator.getFloor() > 0 && elevator.getFloor() < (LEVELS_COUNT - 1) ||
					elevator.getFloor() == (LEVELS_COUNT - 1) && sim.tick > 4000 && elevator.passengers.size() > 15
				)
                {
                    elevator.go_to_floor = getNearestToLevelDestination(sim, elevator, elevator.getFloor());
                }
			}
			else if (elevator.side == side)
			{
				for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
				{
					MyPassenger &passenger = passengerEntry.second;
					int elAbsX = std::abs(elevator.x) ;
					int ind = 0;
					if (elAbsX < 70)
						ind = 0;
					else if (elAbsX < 150)
						ind = 1;
					else if (elAbsX < 230)
						ind = 2;
					else
						ind = 3;
					
					int dest_floor = passenger.dest_floor;
					if (ind == 0 && dest_floor >= 7 || ind == 1 && dest_floor >= 5 || ind == 2 && dest_floor >= 4 || ind == 3)
					{
						if ((int) passenger.state < 4)
						{
							if (elevator.state != EState::MOVING)
							{
								elevator.go_to_floor = passenger.from_floor;
							}
							if (elevator.getFloor() == passenger.from_floor)
							{
								passenger.set_elevator.insert(elevator.id);
							}
						}
					}
				}
				
				if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
					int delta = 100;
					int targetFloor = 0;
					for (int id : elevator.passengers)
					{
						MyPassenger &pass = sim.passengers[id];
						int d = std::abs(elevator.getFloor() - pass.dest_floor);
						if (d < delta)
						{
							delta = d;
							targetFloor = pass.dest_floor;
						}
					}
					elevator.go_to_floor = targetFloor;
				}
				
				if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
					elevator.go_to_floor = -1;
			}
		}
	}
};

struct StratE2
{
	Side side;
	Direction dir = Direction::UP;
	StratE2(Side side) : side(side)
	{
	}
	
	bool reachedTop = false;
	bool reachedTop2 = false;
	void makeMove(Simulator &sim)
	{
		for (MyElevator & elevator : sim.elevators)
		{
			if (elevator.side == side && elevator.ind == 0)
			{
				if (!reachedTop && elevator.getFloor() > 0 && elevator.state == EState::OPENING && getNearestToLevelDestination(sim, elevator, 100) <= elevator.getFloor())
				{
					reachedTop = true;
					dir = Direction::DOWN;
				}
				
				for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
				{
					MyPassenger &passenger = passengerEntry.second;
					int dest_floor = passenger.dest_floor;
					if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
					{
						if (elevator.state == EState::FILLING && passenger.getFloor() == elevator.getFloor() && passenger.state == PState::WAITING_FOR_ELEVATOR)
						{
							if (dir == Direction::UP && sim.tick < 2000 && dest_floor > 7)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
							
							if (dir == Direction::DOWN && dest_floor <= 4)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
						}
					}
				}
				
                if (elevator.passengers.size() == MAX_PASSENGERS || elevator.getFloor() > 0 && elevator.getFloor() < (LEVELS_COUNT - 1) ||
					elevator.getFloor() == (LEVELS_COUNT - 1) && sim.tick > 4000 && elevator.passengers.size() > 15 ||
					sim.tick > 2000 && elevator.getFloor() == 0
				)
                {
                    elevator.go_to_floor = getNearestToLevelDestination(sim, elevator, elevator.getFloor());
                }
			}
			else if (elevator.side == side && elevator.ind == 1)
			{
				if (!reachedTop2 && elevator.getFloor() > 0 && elevator.state == EState::OPENING && getNearestToLevelDestination(sim, elevator, 100) <= elevator.getFloor())
				{
					reachedTop2 = true;
					dir = Direction::DOWN;
				}
				
				for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
				{
					MyPassenger &passenger = passengerEntry.second;
					int dest_floor = passenger.dest_floor;
					if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
					{
						if (elevator.state == EState::FILLING && passenger.getFloor() == elevator.getFloor() && passenger.state == PState::WAITING_FOR_ELEVATOR)
						{
							if (dir == Direction::UP && sim.tick < 2000 && dest_floor > 6 && dest_floor < 8)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
							
							if (dir == Direction::DOWN && dest_floor <= 3)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
						}
					}
				}
				
                if (elevator.passengers.size() == MAX_PASSENGERS || elevator.getFloor() > 0 && elevator.getFloor() < (LEVELS_COUNT - 2) ||
					elevator.getFloor() == (LEVELS_COUNT - 2) && sim.tick > 3800 && elevator.passengers.size() > 15 ||
					sim.tick > 2000 && elevator.getFloor() == 0
				)
                {
                    elevator.go_to_floor = getNearestToLevelDestination(sim, elevator, elevator.getFloor());
                }
			}
			else if (elevator.side == side)
			{
				for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
				{
					MyPassenger &passenger = passengerEntry.second;
					int elAbsX = std::abs(elevator.x) ;
					int ind = 0;
					if (elAbsX < 70)
						ind = 0;
					else if (elAbsX < 150)
						ind = 1;
					else if (elAbsX < 230)
						ind = 2;
					else
						ind = 3;
					
					int dest_floor = passenger.dest_floor;
					if (ind == 0 && dest_floor >= 7 || ind == 1 && dest_floor >= 5 || ind == 2 && dest_floor >= 4 || ind == 3)
					{
						if ((int) passenger.state < 4)
						{
							if (elevator.state != EState::MOVING)
							{
								elevator.go_to_floor = passenger.from_floor;
							}
							if (elevator.getFloor() == passenger.from_floor)
							{
								passenger.set_elevator.insert(elevator.id);
							}
						}
					}
				}
				
				if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
					int delta = 100;
					int targetFloor = 0;
					for (int id : elevator.passengers)
					{
						MyPassenger &pass = sim.passengers[id];
						int d = std::abs(elevator.getFloor() - pass.dest_floor);
						if (d < delta)
						{
							delta = d;
							targetFloor = pass.dest_floor;
						}
					}
					elevator.go_to_floor = targetFloor;
				}
				
				if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
					elevator.go_to_floor = -1;
			}
		}
	}
};

struct StratE3
{
	Side side;
	Direction dir = Direction::UP;
	StratE3(Side side) : side(side)
	{
	}
	
	bool reachedTop = false;
	bool reachedTop2 = false;
	int maxFloor1 = LEVELS_COUNT - 1;
	int maxFloor2 = LEVELS_COUNT - 2;
	void makeMove(Simulator &sim)
	{
		for (MyElevator & elevator : sim.elevators)
		{
			if (elevator.side == side && elevator.ind == 0)
			{
				if (!reachedTop && elevator.getFloor() > 0 && elevator.state == EState::OPENING && getNearestToLevelDestination(sim, elevator, 100) <= elevator.getFloor())
				{
					reachedTop = true;
					dir = Direction::DOWN;
					maxFloor1 = elevator.getFloor();
				}
				
				for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
				{
					MyPassenger &passenger = passengerEntry.second;
					int dest_floor = passenger.dest_floor;
					if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
					{
						if (elevator.state == EState::FILLING && passenger.getFloor() == elevator.getFloor() && passenger.state == PState::WAITING_FOR_ELEVATOR)
						{
							if (dir == Direction::UP && sim.tick < 2000 && dest_floor > 6)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
							
							if (dir == Direction::DOWN && dest_floor <= 4)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
						}
					}
				}
				
                if (elevator.passengers.size() == MAX_PASSENGERS || elevator.getFloor() > 0 && elevator.getFloor() < maxFloor1 ||
					elevator.getFloor() == maxFloor1 && sim.tick > 4600 && elevator.passengers.size() >= 10 ||
					sim.tick > 2000 && elevator.getFloor() == 0
				)
                {
                    elevator.go_to_floor = getNearestToLevelDestination(sim, elevator, elevator.getFloor());
                }
			}
			else if (elevator.side == side && elevator.ind == 1)
			{
				if (!reachedTop2 && elevator.getFloor() > 0 && elevator.state == EState::OPENING && getNearestToLevelDestination(sim, elevator, 100) <= elevator.getFloor())
				{
					reachedTop2 = true;
					dir = Direction::DOWN;
					maxFloor2 = elevator.getFloor();
				}
				
				for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
				{
					MyPassenger &passenger = passengerEntry.second;
					int dest_floor = passenger.dest_floor;
					if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
					{
						if (elevator.state == EState::FILLING && passenger.getFloor() == elevator.getFloor() && passenger.state == PState::WAITING_FOR_ELEVATOR)
						{
							if (dir == Direction::UP && sim.tick < 2000 && dest_floor > 5 && dest_floor < 8)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
							
							if (dir == Direction::DOWN && dest_floor <= 3)
							{
								if (elevator.passengers.size() < MAX_PASSENGERS)
								{
									passenger.set_elevator.insert(elevator.id);
								}
							}
						}
					}
				}
				
                if (elevator.passengers.size() == MAX_PASSENGERS || elevator.getFloor() > 0 && elevator.getFloor() < maxFloor2 ||
					elevator.getFloor() == maxFloor2 && sim.tick > 4600 && elevator.passengers.size() >= 10 ||
					sim.tick > 2000 && elevator.getFloor() == 0
				)
                {
                    elevator.go_to_floor = getNearestToLevelDestination(sim, elevator, elevator.getFloor());
                }
			}
			else if (elevator.side == side)
			{
				for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
				{
					MyPassenger &passenger = passengerEntry.second;
					int elAbsX = std::abs(elevator.x) ;
					int ind = 0;
					if (elAbsX < 70)
						ind = 0;
					else if (elAbsX < 150)
						ind = 1;
					else if (elAbsX < 230)
						ind = 2;
					else
						ind = 3;
					
					int dest_floor = passenger.dest_floor;
					if (ind == 0 && dest_floor >= 7 || ind == 1 && dest_floor >= 5 || ind == 2 && dest_floor >= 4 || ind == 3)
					{
						if ((int) passenger.state < 4)
						{
							if (elevator.state != EState::MOVING)
							{
								elevator.go_to_floor = passenger.from_floor;
							}
							if (elevator.getFloor() == passenger.from_floor)
							{
								passenger.set_elevator.insert(elevator.id);
							}
						}
					}
				}
				
				if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
					int delta = 100;
					int targetFloor = 0;
					for (int id : elevator.passengers)
					{
						MyPassenger &pass = sim.passengers[id];
						int d = std::abs(elevator.getFloor() - pass.dest_floor);
						if (d < delta)
						{
							delta = d;
							targetFloor = pass.dest_floor;
						}
					}
					elevator.go_to_floor = targetFloor;
				}
				
				if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
					elevator.go_to_floor = -1;
			}
		}
	}
};

namespace strat2661 {
	enum class Direction {
    UP, DOWN
};

int getNearestToLevelDestination(Simulator &sim, MyElevator& elevator, int level);
int getNearestToLevelDestinationNoRand(Simulator &sim, MyElevator& elevator, int level);

struct Value {
	int points = 0;
	int ticks = 0;
};

struct ElevatorStrategyUpDown
{
	Side side;
	Direction dir = Direction::UP;
	int dirChanges = 0;
	int firstMoveMinDest = 0;
	
	void makeMove(Simulator &sim, MyElevator &elevator)
	{
		Direction old = dir;
		doMakeMove(sim, elevator);
		if (old != dir)
			++dirChanges;
	}
	
	void doMakeMove(Simulator &sim, MyElevator &elevator)
	{
		if (dir == Direction::UP && elevator.getFloor() == (LEVELS_COUNT - 1))
			dir = Direction::DOWN;
		else if (dir == Direction::DOWN && elevator.getFloor() == 0)
			dir = Direction::UP;
		
		if (elevator.state != EState::FILLING)
			return;
		
		Value value = getCargoValue(sim, elevator);
		
		int floor = elevator.getFloor();
		
		if (elevator.passengers.size() == MAX_PASSENGERS)
		{
			int go_to_floor = strat2661::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
			goToFloor(elevator, go_to_floor);
		}
		
		int passCount = invitePassengers(sim, elevator);
		if (sim.tick < 1980 && floor == 0)
			++passCount;
        
        if (!passCount)
		{
			//int maxWait = 300 - std::max(0, (int) (elevator.passengers.size()) - 12) * 25;
			int maxWait = 550 - std::max(0, (int) (elevator.passengers.size()) - 12) * 50;
			int ticksToWait = std::min(std::max(0, 7200 - sim.tick - value.ticks), maxWait);
			//std::cout << "Wait " << ticksToWait << std::endl;
			for (auto && p = sim.outPassengers.begin(); p != sim.outPassengers.upper_bound(sim.tick + ticksToWait); ++p)
			{
				if (p->second.getFloor() == floor)
					++passCount;
			}
		}
		
		if (!passCount)
		{
			int go_to_floor = strat2661::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
			if (go_to_floor != floor)
			{
				goToFloor(elevator, go_to_floor);
			}
			else
			{
				// TODO
				if (dir == Direction::UP)
					goToFloor(elevator, floor + 1);
				else
					goToFloor(elevator, floor - 1);
			}
		}
	}
	
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
		for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
        {
            MyPassenger &passenger = passengerEntry.second;
            int dest_floor = passenger.dest_floor;
			
            //if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
            {
				if (floor == 0 && dirChanges == 0 && dest_floor <= firstMoveMinDest && sim.tick < 1980)
					continue;
				
                if (passenger.getFloor() == floor)
				{
					if (passenger.state == PState::WAITING_FOR_ELEVATOR || passenger.state == PState::RETURNING)
					{
						++passCount;
						passenger.set_elevator.insert(elevator.id);
					}
					else if (passenger.state == PState::MOVING_TO_ELEVATOR && passenger.elevator == elevator.id)
					{
						++passCount;
					}
				}
            }
        }
        
        return passCount;
	}
	
	Value getCargoValue(Simulator &sim, MyElevator &elevator)
	{
		Value result;
		
		std::bitset<LEVELS_COUNT> levels;
		levels.set(0);
		
		int minFloor = elevator.getFloor();
		int maxFloor = minFloor;
		int floor = elevator.getFloor();
		for (int id : elevator.passengers)
		{
			MyPassenger &pass = sim.passengers[id];
			result.points += std::abs(pass.dest_floor - pass.from_floor) * 10;
			minFloor = std::min(minFloor, pass.dest_floor);
			maxFloor = std::max(maxFloor, pass.dest_floor);
			if (pass.dest_floor != floor)
			{
				levels.set(pass.dest_floor);
			}
		}
		
		result.ticks = (maxFloor - minFloor + std::min(maxFloor - floor, floor - minFloor)) * 10 * 60 + levels.size() * 250;
		
		return result;
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

int getNearestToLevelDestinationNoRand(Simulator &sim, MyElevator& elevator, int level)
{
    int delta = 1000;
    int targetFloor = level;

	std::map<int, int> points;
	
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = sim.passengers[id];
		int d = -std::abs(pass.dest_floor - pass.from_floor);
		points[pass.dest_floor] += d;
    }
    
    auto x = std::min_element(points.begin(), points.end(),
    [level](const std::pair<int, int>& p1, const std::pair<int, int>& p2) {
        return (p1.second + 7*std::abs(level - p1.first)) < (p2.second + 7*std::abs(level - p2.first)); });

	if (x != points.end())
		return x->first;
	
    return targetFloor;
}

MyStrategy::MyStrategy(Side side) : side(side)
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

MyStrategy::~MyStrategy()
{

}

void MyStrategy::makeMove()
{
	for (MyElevator & elevator : sim.elevators)
    {
        if (elevator.side == side && elevator.ind == 0)
        {
			strategy1.makeMove(sim, elevator);
        }
        else if (elevator.side == side && elevator.ind == 1)
        {
			strategy2.makeMove(sim, elevator);
        }
        else if (elevator.side == side && elevator.ind == 2)
        {
			strategy3.makeMove(sim, elevator);
        }
        else if (elevator.side == side && elevator.ind == 3)
        {
			strategy4.makeMove(sim, elevator);
		}
    }
}

void MyStrategy::makeMove(Simulator &inputSim)
{
    sim.synchronizeWith(inputSim);
    makeMove();
    inputSim.copyCommandsFrom(sim, side);
    sim.step();
}

}

namespace strat2724 {
	#include <bitset>
#include <iostream>

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
		
		if (elevator.passengers.size() == MAX_PASSENGERS)
		{
			int go_to_floor = strat2724::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
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
			//int maxWait = 300 - std::max(0, (int) (elevator.passengers.size()) - 12) * 25;
			int maxWait = 550 - std::max(0, (int) (elevator.passengers.size()) - 12) * 50;
			int ticksToWait = std::min(std::max(0, 7200 - sim.tick - value.ticks), maxWait);
			//std::cout << "Wait " << ticksToWait << std::endl;
			for (auto && p = sim.outPassengers.begin(); p != sim.outPassengers.upper_bound(sim.tick + ticksToWait); ++p)
			{
				if (p->second.getFloor() == floor)
					++passCount;
			}
		}
		
		if (!passCount)
		{
			int go_to_floor = strat2724::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
			if (go_to_floor != floor)
			{
				goToFloor(elevator, go_to_floor);
			}
			else
			{
				// TODO
				if (dir == Direction::UP)
					goToFloor(elevator, floor + 1);
				else
					goToFloor(elevator, floor - 1);
			}
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
		for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
        {
            MyPassenger &passenger = passengerEntry.second;
            int dest_floor = passenger.dest_floor;
			
            //if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
            {
				if (floor == 0 && dirChanges == 0 && dest_floor <= firstMoveMinDest && sim.tick < 1980)
					continue;
				
                if (passenger.getFloor() == floor)
				{
					if (passenger.state == PState::WAITING_FOR_ELEVATOR || passenger.state == PState::RETURNING)
					{
						++passCount;
						passenger.set_elevator.insert(elevator.id);
					}
					else if (passenger.state == PState::MOVING_TO_ELEVATOR && passenger.elevator == elevator.id)
					{
						++passCount;
					}
				}
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

#ifndef HEADER_OLNY_INLINE
#define HEADER_OLNY_INLINE
#define HEADER_OLNY_STATIC
#endif

HEADER_OLNY_INLINE void makeMoveSimple(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				int elAbsX = std::abs(elevator.x) ;
				if (elAbsX < 70 || elAbsX < 150 && passenger.dest_floor < 5 || elAbsX >= 150 && passenger.dest_floor >= 5)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
            
            if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
				elevator.go_to_floor = -1;
		}
	}
}


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
		return rand() % 8 + 1;
	}

    return targetFloor;
}

HEADER_OLNY_INLINE int getNearestToLevelDestinationNoRand(Simulator &sim, MyElevator& elevator, int level)
{
    int targetFloor = level;

	std::map<int, int> points;
	
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = sim.passengers[id];
		int d = -std::abs(pass.dest_floor - pass.from_floor);
		points[pass.dest_floor] += d;
    }

    auto x = std::min_element(points.begin(), points.end(),
    [level](const std::pair<int, int>& p1, const std::pair<int, int>& p2) {
        return (p1.second + 7*std::abs(level - p1.first)) < (p2.second + 7*std::abs(level - p2.first)); });

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
			
			for (int tick = 0; tick < 400; ++tick)
			{
				//makeMoveSimple(enemySide, copy.sim);
				copy.makeMove();
				copy.sim.step();
			}
			
			int valLeft = copy.sim.scores[0] + copy.sim.totalCargoValue(Side::LEFT).points / 2;
			int valRight = copy.sim.scores[1] + copy.sim.totalCargoValue(Side::RIGHT).points / 2;
			// 1000 ticks
			// /3 RES: 1.09252 8088.1 8836.4 W 78 L 22 GOOD!
			// /1.5 RES: 1.09848 8163.4 8967.3 W 80 L 20 GOOD!
			// /2 RES: 1.10654 8272 9153.3 W 84 L 16 GOOD!!
			// 500 ticks
			// 300 RES: 1.13074 8058.8 9112.4 W 85 L 15 GOOD!!
			// 400 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 500 RES: 1.17981 8093.7 9549 W 92 L 8 GOOD!!
			// 600 RES: 1.16856 7998.3 9346.5 W 90 L 10 GOOD!!
			// 700 RES: 1.17386 7896.1 9268.9 W 89 L 11 GOOD!!
			// 400/2 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 400/1.7 RES: 1.19318 7929.3 9461.1 W 89 L 11 GOOD!!
			// 400/2.2 RES: 1.1988 7974 9559.2 W 91 L 9 GOOD!!
			
			int points;
			if (copy.side == Side::LEFT)
				points = valLeft - valRight;
			else
				points = valRight - valLeft;
			
			//std::cout << "PTS " << i << " " << valLeft << " " << valRight << " " << points << std::endl;
			if (i == elevator.next_floor)
				++points;
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
}


namespace strat2806 {
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
		
		if (elevator.passengers.size() == MAX_PASSENGERS)
		{
			int go_to_floor = strat2806::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
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
			//int maxWait = 300 - std::max(0, (int) (elevator.passengers.size()) - 12) * 25;
			int maxWait = 550 - std::max(0, (int) (elevator.passengers.size()) - 12) * 50;
			int ticksToWait = std::min(std::max(0, 7200 - sim.tick - value.ticks), maxWait);
			//std::cout << "Wait " << ticksToWait << std::endl;
			for (auto && p = sim.outPassengers.begin(); p != sim.outPassengers.upper_bound(sim.tick + ticksToWait); ++p)
			{
				if (p->second.getFloor() == floor)
					++passCount;
			}
		}
		
		if (!passCount)
		{
			int go_to_floor = strat2806::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
			if (go_to_floor != floor)
			{
				goToFloor(elevator, go_to_floor);
			}
			else
			{
				// TODO
				if (dir == Direction::UP)
					goToFloor(elevator, floor + 1);
				else
					goToFloor(elevator, floor - 1);
			}
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
		
		bool anyElevatorsCloser = false;
		/*for (MyElevator &e : sim.elevators)
		{
			if (e.id != elevator.id && e.ind <= elevator.ind && e.state == EState::FILLING && e.getFloor() == floor && e.side != elevator.side)
			{
				anyElevatorsCloser = true;
				break;
			}
		}*/
		
		//std::bitset<LEVELS_COUNT> levels;
		
		std::multimap<int, MyPassenger *> passengers;
		
		for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
        {
            MyPassenger &passenger = passengerEntry.second;
            int dest_floor = passenger.dest_floor;
			
            //if (elevator.ind < 3 || dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
            {
				if (floor == 0 && dirChanges == 0 && dest_floor <= firstMoveMinDest && sim.tick < 1980)
					continue;
				
                if (passenger.getFloor() == floor)
				{
					if (passenger.state == PState::WAITING_FOR_ELEVATOR || passenger.state == PState::RETURNING)
					{
						if (anyElevatorsCloser)
						{
							passenger.set_elevator.insert(elevator.id);
							++passCount;
						}
						else
						{
							passengers.insert(std::make_pair(passenger.getValue(side), &passenger));
						}
					}
					else if (passenger.state == PState::MOVING_TO_ELEVATOR && passenger.elevator == elevator.id)
					{
						++passCount;
						//levels.set(dest_floor);
					}
				}
            }
        }
		
		if (!anyElevatorsCloser)
		{
			int limit = MAX_PASSENGERS - (int) elevator.passengers.size() - passCount;
			//std::cout << "LIMIT " << limit << " " << passengers.size() << std::endl;
			
			for (auto it = passengers.rbegin(); it != passengers.rend(); ++it)
			{
				if (limit <= 0)
					break;
				
				if (limit <= 2 && it->first < 30 || limit <= 4 && it->first < 20)
					continue;
				
				it->second->set_elevator.insert(elevator.id);
				++passCount;
				
				--limit;
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

#ifndef HEADER_OLNY_INLINE
#define HEADER_OLNY_INLINE
#define HEADER_OLNY_STATIC
#endif

HEADER_OLNY_INLINE void makeMoveSimple(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				int elAbsX = std::abs(elevator.x) ;
				if (elAbsX < 70 || elAbsX < 150 && passenger.dest_floor < 5 || elAbsX >= 150 && passenger.dest_floor >= 5)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
            
            if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
				elevator.go_to_floor = -1;
		}
	}
}


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
		return rand() % 8 + 1;
	}

    return targetFloor;
}

HEADER_OLNY_INLINE int getNearestToLevelDestinationNoRand(Simulator &sim, MyElevator& elevator, int level)
{
    int targetFloor = level;

	std::map<int, int> points;
	
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = sim.passengers[id];
		int d = -std::abs(pass.dest_floor - pass.from_floor);
		points[pass.dest_floor] += d;
    }

    auto x = std::min_element(points.begin(), points.end(),
    [level](const std::pair<int, int>& p1, const std::pair<int, int>& p2) {
        return (p1.second + 7*std::abs(level - p1.first)) < (p2.second + 7*std::abs(level - p2.first)); });

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
			
			for (int tick = 0; tick < 400; ++tick)
			{
				//makeMoveSimple(enemySide, copy.sim);
				copy.makeMove();
				copy.sim.step();
			}
			
			int valLeft = copy.sim.scores[0] + copy.sim.totalCargoValue(Side::LEFT).points / 2;
			int valRight = copy.sim.scores[1] + copy.sim.totalCargoValue(Side::RIGHT).points / 2;
			// 1000 ticks
			// /3 RES: 1.09252 8088.1 8836.4 W 78 L 22 GOOD!
			// /1.5 RES: 1.09848 8163.4 8967.3 W 80 L 20 GOOD!
			// /2 RES: 1.10654 8272 9153.3 W 84 L 16 GOOD!!
			// 500 ticks
			// 300 RES: 1.13074 8058.8 9112.4 W 85 L 15 GOOD!!
			// 400 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 500 RES: 1.17981 8093.7 9549 W 92 L 8 GOOD!!
			// 600 RES: 1.16856 7998.3 9346.5 W 90 L 10 GOOD!!
			// 700 RES: 1.17386 7896.1 9268.9 W 89 L 11 GOOD!!
			// 400/2 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 400/1.7 RES: 1.19318 7929.3 9461.1 W 89 L 11 GOOD!!
			// 400/2.2 RES: 1.1988 7974 9559.2 W 91 L 9 GOOD!!
			
			int points;
			if (copy.side == Side::LEFT)
				points = valLeft - valRight;
			else
				points = valRight - valLeft;
			
			//std::cout << "PTS " << i << " " << valLeft << " " << valRight << " " << points << std::endl;
			if (i == elevator.next_floor)
				++points;
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

}

namespace strat3340 {
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
		
		if (elevator.passengers.size() == MAX_PASSENGERS)
		{
			int go_to_floor = strat3340::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
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
			//int maxWait = 300 - std::max(0, (int) (elevator.passengers.size()) - 12) * 25;
			int maxWait = 550 - std::max(0, (int) (elevator.passengers.size()) - 12) * 50;
			int ticksToWait = std::min(std::max(0, 7200 - sim.tick - value.ticks), maxWait);
			//std::cout << "Wait " << ticksToWait << std::endl;
			for (auto && p = sim.outPassengers.begin(); p != sim.outPassengers.upper_bound(sim.tick + ticksToWait); ++p)
			{
				if (p->second.getFloor() == floor)
					++passCount;
			}
		}
		
		if (!passCount)
		{
			int go_to_floor = strat3340::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
			if (go_to_floor != floor)
			{
				goToFloor(elevator, go_to_floor);
			}
			else
			{
				// TODO
				if (dir == Direction::UP)
					goToFloor(elevator, floor + 1);
				else
					goToFloor(elevator, floor - 1);
			}
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
		
		bool anyElevatorsCloser = false;
		/*for (MyElevator &e : sim.elevators)
		{
			if (e.id != elevator.id && e.ind <= elevator.ind && e.state == EState::FILLING && e.getFloor() == floor && e.side != elevator.side)
			{
				anyElevatorsCloser = true;
				break;
			}
		}*/
		
		//std::bitset<LEVELS_COUNT> levels;
		
		std::multimap<int, MyPassenger *> passengers;
		
		for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
        {
            MyPassenger &passenger = passengerEntry.second;
            int dest_floor = passenger.dest_floor;
			
            //if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
            {
				if (floor == 0 && dirChanges == 0 && dest_floor <= firstMoveMinDest && sim.tick < 1980)
					continue;
				
                if (passenger.getFloor() == floor)
				{
					if (passenger.state == PState::WAITING_FOR_ELEVATOR || passenger.state == PState::RETURNING)
					{
						if (anyElevatorsCloser)
						{
							passenger.set_elevator.insert(elevator.id);
							++passCount;
						}
						else
						{
							passengers.insert(std::make_pair(passenger.getValue(side), &passenger));
						}
					}
					else if (passenger.state == PState::MOVING_TO_ELEVATOR && passenger.elevator == elevator.id)
					{
						++passCount;
						//levels.set(dest_floor);
					}
				}
            }
        }
		
		if (!anyElevatorsCloser)
		{
			int limit = MAX_PASSENGERS - (int) elevator.passengers.size() - passCount;
			//std::cout << "LIMIT " << limit << " " << passengers.size() << std::endl;
			
			for (auto it = passengers.rbegin(); it != passengers.rend(); ++it)
			{
				if (limit <= 0)
					break;
				
				if (limit <= 2 && it->first < 30 || limit <= 4 && it->first < 20)
					continue;
				
				it->second->set_elevator.insert(elevator.id);
				++passCount;
				
				--limit;
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
	
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = sim.passengers[id];
		int d = -std::abs(pass.dest_floor - pass.from_floor);
		points[pass.dest_floor] += d;
    }

    auto x = std::min_element(points.begin(), points.end(),
    [level](const std::pair<int, int>& p1, const std::pair<int, int>& p2) {
        return (p1.second + 7*std::abs(level - p1.first)) < (p2.second + 7*std::abs(level - p2.first)); });

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
			
			for (int tick = 0; tick < 400; ++tick)
			{
				//makeMoveSimple(enemySide, copy.sim);
				copy.makeMove();
				copy.sim.step();
			}
			
			int valLeft = copy.sim.scores[0] + copy.sim.passengersTotal[0]*10 + copy.sim.totalCargoValue(Side::LEFT).points / 2;
			int valRight = copy.sim.scores[1] + copy.sim.passengersTotal[1]*10 + copy.sim.totalCargoValue(Side::RIGHT).points / 2;
			// 1000 ticks
			// /3 RES: 1.09252 8088.1 8836.4 W 78 L 22 GOOD!
			// /1.5 RES: 1.09848 8163.4 8967.3 W 80 L 20 GOOD!
			// /2 RES: 1.10654 8272 9153.3 W 84 L 16 GOOD!!
			// 500 ticks
			// 300 RES: 1.13074 8058.8 9112.4 W 85 L 15 GOOD!!
			// 400 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 500 RES: 1.17981 8093.7 9549 W 92 L 8 GOOD!!
			// 600 RES: 1.16856 7998.3 9346.5 W 90 L 10 GOOD!!
			// 700 RES: 1.17386 7896.1 9268.9 W 89 L 11 GOOD!!
			// 400/2 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 400/1.7 RES: 1.19318 7929.3 9461.1 W 89 L 11 GOOD!!
			// 400/2.2 RES: 1.1988 7974 9559.2 W 91 L 9 GOOD!!
			// *5 RES: 0.997525 9089.7 9067.2 W 41 L 59
			// *10 RES: 1.01355 9031.3 9153.7 W 52 L 47.6667
			// *20 RES: 1.0114 9086.8 9190.4 W 54 L 46
			// *30 RES: 0.990421 9259.5 9170.8 W 41 L 58
			// *40 RES: 1.00459 9193.9 9236.1 W 52 L 48
			// *50 RES: 0.993825 9311.4 9253.9 W 52 L 48
			// *60 RES: 0.994047 9306 9250.6 W 44 L 56
			// *70 RES: 0.998478 9328.5 9314.3 W 48 L 51
			// *100 RES: 0.965599 9525.8 9198.1 W 38 L 62
			
			
			int points;
			if (copy.side == Side::LEFT)
				points = valLeft - valRight;
			else
				points = valRight - valLeft;
			
			//std::cout << "PTS " << i << " " << valLeft << " " << valRight << " " << points << std::endl;
			if (i == elevator.next_floor)
				++points;
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
}

namespace strat3659 {
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
		
		Value value = sim.getCargoValueBug(elevator);
		
		int floor = elevator.getFloor();
		
		if (elevator.passengers.size() == MAX_PASSENGERS)
		{
			int go_to_floor = strat3659::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
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
			//int maxWait = 300 - std::max(0, (int) (elevator.passengers.size()) - 12) * 25;
			int maxWait = 550 - std::max(0, (int) (elevator.passengers.size()) - 12) * 50;
			int ticksToWait = std::min(std::max(0, 7200 - sim.tick - value.ticks), maxWait);
			//std::cout << "Wait " << ticksToWait << std::endl;
			for (auto && p = sim.outPassengers.begin(); p != sim.outPassengers.upper_bound(sim.tick + ticksToWait); ++p)
			{
				if (p->second.getFloor() == floor)
					++passCount;
			}
		}
		
		// 6700 RES: 1.044 9469.8 9886.5 W 70 L 30 GOOD
		// 6800 RES: 1.06317 9324.9 9914 W 75 L 24 GOOD!
		// 6900 RES: 1.06301 9324.9 9912.5 W 75 L 24 GOOD!
		// 8 RES: 1.04497 9470.8 9896.7 W 72 L 28 GOOD
		if (!passCount)
		{
			int go_to_floor = strat3659::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
			if (go_to_floor != floor)
			{
				goToFloor(elevator, go_to_floor);
			}
			else
			{
				// TODO
				if (dir == Direction::UP)
					goToFloor(elevator, floor + 1);
				else
					goToFloor(elevator, floor - 1);
			}
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
		
		bool anyElevatorsCloser = false;
		/*for (MyElevator &e : sim.elevators)
		{
			if (e.id != elevator.id && e.ind <= elevator.ind && e.state == EState::FILLING && e.getFloor() == floor && e.side != elevator.side)
			{
				anyElevatorsCloser = true;
				break;
			}
		}*/
		
		//std::bitset<LEVELS_COUNT> levels;
		
		std::multimap<int, MyPassenger *> passengers;
		
		for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
        {
            MyPassenger &passenger = passengerEntry.second;
            int dest_floor = passenger.dest_floor;
			
			if (std::abs(dest_floor - passenger.from_floor) <= 1)
				continue;
			
            //if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
            {
				if (floor == 0 && dirChanges == 0 && dest_floor <= firstMoveMinDest && sim.tick < 1980)
					continue;
				
                if (passenger.getFloor() == floor)
				{
					if (passenger.state == PState::WAITING_FOR_ELEVATOR || passenger.state == PState::RETURNING)
					{
						if (anyElevatorsCloser)
						{
							passenger.set_elevator.insert(elevator.id);
							++passCount;
						}
						else
						{
							passengers.insert(std::make_pair(passenger.getValue(side), &passenger));
						}
					}
					else if (passenger.state == PState::MOVING_TO_ELEVATOR && passenger.elevator == elevator.id)
					{
						++passCount;
						//levels.set(dest_floor);
					}
				}
            }
        }
		
		if (!anyElevatorsCloser)
		{
			int limit = MAX_PASSENGERS - (int) elevator.passengers.size() - passCount;
			//std::cout << "LIMIT " << limit << " " << passengers.size() << std::endl;
			
			for (auto it = passengers.rbegin(); it != passengers.rend(); ++it)
			{
				if (limit <= 0)
					break;
				
				if (limit <= 2 && it->first < 30 || limit <= 4 && it->first < 20)
					continue;
				
				it->second->set_elevator.insert(elevator.id);
				++passCount;
				
				--limit;
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
	
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = sim.passengers[id];
		points[pass.dest_floor] -= pass.getValue(elevator.side);
    }
    
    /*for (std::pair<const int, MyPassenger> &p : sim.outPassengers)
	{
		int floor = p.second.getFloor();
		if (floor != level)
		{
			int pts = -10;
			if (p.second.side != elevator.side)
				pts *= 2;
			if (points.count(floor))
				points[floor] += pts;
		}
	}*/
	// 0 RES: 1.06403 9456 10061.5 rem 1683.2 997.2 W 76 L 24 GOOD!
	// 30 RES: 1.03688 9647.1 10002.9 rem 1680 1098.9 W 59 L 40 GOOD
	// 10 RES: 1.04705 9618.8 10071.4 rem 1738.1 973.5 W 62 L 37 GOOD
	// 1 RES: 1.04413 9608.1 10032.1 rem 1668.4 974.9 W 69 L 30 GOOD

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
	int maxTicks = 7200 - sim.tick;
	float coef = 1.0;
	if (maxTicks < 1500)
		coef = ((float)maxTicks - 200.0f) / 1300.0f;
	
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
			
			for (int tick = 0; tick < 500; ++tick)
			{
				//makeMoveSimple(enemySide, copy.sim);
				copy.makeMove();
				copy.sim.step();
			}
			
			int valLeft = copy.sim.scores[0] + copy.sim.passengersTotal[0]*10 + copy.sim.totalCargoValue(Side::LEFT).points / 2 * coef;
			int valRight = copy.sim.scores[1] + copy.sim.passengersTotal[1]*10 + copy.sim.totalCargoValue(Side::RIGHT).points / 2 * coef;
			// 1000 ticks
			// /3 RES: 1.09252 8088.1 8836.4 W 78 L 22 GOOD!
			// /1.5 RES: 1.09848 8163.4 8967.3 W 80 L 20 GOOD!
			// /2 RES: 1.10654 8272 9153.3 W 84 L 16 GOOD!!
			// 500 ticks
			// 300 RES: 1.13074 8058.8 9112.4 W 85 L 15 GOOD!!
			// 400 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 500 RES: 1.17981 8093.7 9549 W 92 L 8 GOOD!!
			// 600 RES: 1.16856 7998.3 9346.5 W 90 L 10 GOOD!!
			// 700 RES: 1.17386 7896.1 9268.9 W 89 L 11 GOOD!!
			// 400/2 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 400/1.7 RES: 1.19318 7929.3 9461.1 W 89 L 11 GOOD!!
			// 400/2.2 RES: 1.1988 7974 9559.2 W 91 L 9 GOOD!!
			// *5 RES: 0.997525 9089.7 9067.2 W 41 L 59
			// *10 RES: 1.01355 9031.3 9153.7 W 52 L 47.6667
			// *20 RES: 1.0114 9086.8 9190.4 W 54 L 46
			// *30 RES: 0.990421 9259.5 9170.8 W 41 L 58
			// *40 RES: 1.00459 9193.9 9236.1 W 52 L 48
			// *50 RES: 0.993825 9311.4 9253.9 W 52 L 48
			// *60 RES: 0.994047 9306 9250.6 W 44 L 56
			// *70 RES: 0.998478 9328.5 9314.3 W 48 L 51
			// *100 RES: 0.965599 9525.8 9198.1 W 38 L 62
			
			// 400 RES: 1.02644 8631.8 8860 W 60 L 40
			// 450 RES: 1.01464 9333.3 9469.9 W 56 L 44
			// 500 RES: 1.02015 9245.53 9431.87 W 58.3333 L 41.3333
			// 600 RES: 1.00917 9399.1 9485.3 W 59 L 41
			
			
			int points;
			if (copy.side == Side::LEFT)
				points = valLeft - valRight;
			else
				points = valRight - valLeft;
			
			//std::cout << "PTS " << i << " " << valLeft << " " << valRight << " " << points << std::endl;
			if (i == elevator.next_floor)
				++points;
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
}

namespace strat3800 {
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
		
		Value value = sim.getCargoValueBug(elevator);
		
		int floor = elevator.getFloor();
		
		if (elevator.passengers.size() == MAX_PASSENGERS)
		{
			int go_to_floor = strat3800::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
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
			//int maxWait = 300 - std::max(0, (int) (elevator.passengers.size()) - 12) * 25;
			int maxWait = 550 - std::max(0, (int) (elevator.passengers.size()) - 12) * 50;
			int ticksToWait = std::min(std::max(0, 7200 - sim.tick - value.ticks), maxWait);
			//std::cout << "Wait " << ticksToWait << std::endl;
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
			
			/*if (passCount && passCount <= 2 && (t - sim.tick) > 200) {
				//std::cout << "IGNORE " << passCount << " " << (t - sim.tick) << std::endl;
				passCount = 0;
			}*/
			
			// 400 RES: 1.00352 9451.37 9484.6 rem 1081.03 1075.6 W 46 L 53
			// RES: 1.00352 9451.37 9484.6 rem 1081.03 1075.6 W 46 L 53
			// 200 RES: 1.01371 9412.37 9541.4 rem 1077.73 1073.87 W 54.3333 L 44.6667
			
			/*if (passCount && passCount <= 2 && doPredictions && elevator.time_on_the_floor_with_opened_doors > 40)
			{
				//std::cout << "11 " << std::endl;
				//if (elevator.time_on_the_floor_with_opened_doors % 100 == 42)
				{
					if (recalcDoorsClose(sim, elevator, strategy)) {
						std::cout << "CLOSE " << std::endl;
						passCount = 0;
					}
					else
					{
						//std::cout << "DONT CLOSE " << std::endl;
					}
				}
			}*/
		}
		
		// RES: 0.994943 8661.2 8617.4 rem 953.7 944.5 W 39 L 35
		// 100 RES: 1.0044 8900.5 8939.7 rem 987.4 981.3 W 44 L 37
		// 200 RES: 1.01427 8606.9 8729.7 rem 967.7 899.7 W 41 L 25
		// 300 RES: 1.00729 8577.4 8639.9 rem 927.7 896.5 W 36 L 27
		// 400 RES: 1.00842 8525.4 8597.2 rem 936.3 866.2 W 34 L 29
		
		
		
		if (!passCount)
		{
			int go_to_floor = strat3800::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
			if (go_to_floor != floor)
			{
				goToFloor(elevator, go_to_floor);
			}
			else
			{
				// TODO
				if (dir == Direction::UP)
					goToFloor(elevator, floor + 1);
				else
					goToFloor(elevator, floor - 1);
			}
		}
	}
	
	void recalcDestinationBeforeDoorsClose(Simulator &sim, MyElevator &elevator, MyStrategy *strategy);
	//bool recalcDoorsClose(Simulator &sim, MyElevator &elevator, MyStrategy *strategy);
	
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
		
		//std::bitset<LEVELS_COUNT> levels;
		
		std::multimap<int, MyPassenger *> passengers;
		
		for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
        {
            MyPassenger &passenger = passengerEntry.second;
            int dest_floor = passenger.dest_floor;
			
			if (std::abs(dest_floor - passenger.from_floor) <= 1)
				continue;
			
            //if (dir == Direction::UP && passenger.dest_floor > passenger.from_floor || dir == Direction::DOWN && passenger.dest_floor < passenger.from_floor)
            {
				if (floor == 0 && dirChanges == 0 && dest_floor <= firstMoveMinDest && sim.tick < 1980)
					continue;
				
                if (passenger.getFloor() == floor)
				{
					if (passenger.state == PState::WAITING_FOR_ELEVATOR || passenger.state == PState::RETURNING)
					{
						/*if (anyElevatorsCloser)
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
			
			/*for (auto it = passengers.rbegin(); it != passengers.rend(); ++it)
			{
				double dist = std::abs(it->second->x - (double) elevator.x);
				if (dist > maxDist)
					it->second->set_elevator.insert(elevator.id);
			}*/
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

HEADER_OLNY_INLINE void makeMoveSimple(Side side, Simulator &sim)
{
	for (MyElevator & elevator : sim.elevators)
	{
		if (elevator.side == side)
		{
			for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
			{
				MyPassenger &passenger = passengerEntry.second;
				int elAbsX = std::abs(elevator.x) ;
				if (elAbsX < 70 || elAbsX < 150 && passenger.dest_floor < 5 || elAbsX >= 150 && passenger.dest_floor >= 5)
				{
					if ((int) passenger.state < 4)
					{
						if (elevator.state != EState::MOVING)
						{
							elevator.go_to_floor = passenger.from_floor;
						}
						if (elevator.getFloor() == passenger.from_floor)
						{
							passenger.set_elevator.insert(elevator.id);
						}
					}
				}
			}
			
			if (elevator.passengers.size() > 0 && elevator.state != EState::MOVING) {
				int delta = 100;
				int targetFloor = 0;
				for (int id : elevator.passengers)
				{
					MyPassenger &pass = sim.passengers[id];
					int d = std::abs(elevator.getFloor() - pass.dest_floor);
					if (d < delta)
					{
						delta = d;
						targetFloor = pass.dest_floor;
					}
				}
				elevator.go_to_floor = targetFloor;
            }
            
            if (elevator.getFloor() == 0 && sim.tick < 2000 && elevator.passengers.size() < 20 || elevator.time_on_the_floor_with_opened_doors < 200 && elevator.passengers.size() < 4)
				elevator.go_to_floor = -1;
		}
	}
}


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
	
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = sim.passengers[id];
		points[pass.dest_floor] -= pass.getValue(elevator.side);
    }
    
    /*for (std::pair<const int, MyPassenger> &p : sim.outPassengers)
	{
		int floor = p.second.getFloor();
		if (floor != level)
		{
			int pts = -10;
			if (p.second.side != elevator.side)
				pts *= 2;
			if (points.count(floor))
				points[floor] += pts;
		}
	}*/
	// 0 RES: 1.06403 9456 10061.5 rem 1683.2 997.2 W 76 L 24 GOOD!
	// 30 RES: 1.03688 9647.1 10002.9 rem 1680 1098.9 W 59 L 40 GOOD
	// 10 RES: 1.04705 9618.8 10071.4 rem 1738.1 973.5 W 62 L 37 GOOD
	// 1 RES: 1.04413 9608.1 10032.1 rem 1668.4 974.9 W 69 L 30 GOOD

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
	int maxTicks = 7200 - sim.tick;
	float coef = 1.0;
	if (maxTicks < 1500)
		coef = ((float)maxTicks - 200.0f) / 1300.0f;
	
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
			
			for (int tick = 0; tick < 500; ++tick)
			{
				//makeMoveSimple(enemySide, copy.sim);
				copy.makeMove();
				copy.sim.step();
			}
			
			int valLeft = copy.sim.scores[0] + copy.sim.passengersTotal[0]*10 + copy.sim.totalCargoValue(Side::LEFT).points / 2 * coef;
			int valRight = copy.sim.scores[1] + copy.sim.passengersTotal[1]*10 + copy.sim.totalCargoValue(Side::RIGHT).points / 2 * coef;
			// 1000 ticks
			// /3 RES: 1.09252 8088.1 8836.4 W 78 L 22 GOOD!
			// /1.5 RES: 1.09848 8163.4 8967.3 W 80 L 20 GOOD!
			// /2 RES: 1.10654 8272 9153.3 W 84 L 16 GOOD!!
			// 500 ticks
			// 300 RES: 1.13074 8058.8 9112.4 W 85 L 15 GOOD!!
			// 400 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 500 RES: 1.17981 8093.7 9549 W 92 L 8 GOOD!!
			// 600 RES: 1.16856 7998.3 9346.5 W 90 L 10 GOOD!!
			// 700 RES: 1.17386 7896.1 9268.9 W 89 L 11 GOOD!!
			// 400/2 RES: 1.21191 7952.8 9638.1 W 93 L 7 GOOD!!!!
			// 400/1.7 RES: 1.19318 7929.3 9461.1 W 89 L 11 GOOD!!
			// 400/2.2 RES: 1.1988 7974 9559.2 W 91 L 9 GOOD!!
			// *5 RES: 0.997525 9089.7 9067.2 W 41 L 59
			// *10 RES: 1.01355 9031.3 9153.7 W 52 L 47.6667
			// *20 RES: 1.0114 9086.8 9190.4 W 54 L 46
			// *30 RES: 0.990421 9259.5 9170.8 W 41 L 58
			// *40 RES: 1.00459 9193.9 9236.1 W 52 L 48
			// *50 RES: 0.993825 9311.4 9253.9 W 52 L 48
			// *60 RES: 0.994047 9306 9250.6 W 44 L 56
			// *70 RES: 0.998478 9328.5 9314.3 W 48 L 51
			// *100 RES: 0.965599 9525.8 9198.1 W 38 L 62
			
			// 400 RES: 1.02644 8631.8 8860 W 60 L 40
			// 450 RES: 1.01464 9333.3 9469.9 W 56 L 44
			// 500 RES: 1.02015 9245.53 9431.87 W 58.3333 L 41.3333
			// 600 RES: 1.00917 9399.1 9485.3 W 59 L 41
			
			
			int points;
			if (copy.side == Side::LEFT)
				points = valLeft - valRight;
			else
				points = valRight - valLeft;
			
			//std::cout << "PTS " << i << " " << valLeft << " " << valRight << " " << points << std::endl;
			if (i == elevator.next_floor)
				++points;
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
}

namespace strat3950 {
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
		
		if (elevator.passengers.size() == MAX_PASSENGERS)
		{
			int go_to_floor = strat3950::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
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
				if (e.id != elevator.id && e.ind < elevator.ind && e.state == EState::FILLING && e.getFloor() == floor && e.passengers.size() < 19)
				{
					anyElevatorsCloser = true;
				}
			}*/
			
			if (!anyElevatorsCloser)
			{
				//int maxWait = 300 - std::max(0, (int) (elevator.passengers.size()) - 12) * 25;
				int maxWait = 550 - std::max(0, (int) (elevator.passengers.size()) - 12) * 50;
				int ticksToWait = std::min(std::max(0, 7200 - sim.tick - value.ticks), maxWait);
				//std::cout << "Wait " << ticksToWait << std::endl;
				
				// 450 RES: 0.995443 9296.17 9253.8 rem 1138.53 1094.03 W 50.3333 L 49
				// 500 RES: 0.996442 9276.03 9243.03 rem 1120.8 1096.4 W 52.3333 L 47
				// 550 RES: 0.994873 9211.97 9164.73 rem 1095.67 1100.7 W 52 L 47.3333
				// RES: 1.00964 9188.13 9276.73 rem 1109.53 1112.33 W 56 L 43
				
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
			}
		}
		
		// RES: 0.994943 8661.2 8617.4 rem 953.7 944.5 W 39 L 35
		// 100 RES: 1.0044 8900.5 8939.7 rem 987.4 981.3 W 44 L 37
		// 200 RES: 1.01427 8606.9 8729.7 rem 967.7 899.7 W 41 L 25
		// 300 RES: 1.00729 8577.4 8639.9 rem 927.7 896.5 W 36 L 27
		// 400 RES: 1.00842 8525.4 8597.2 rem 936.3 866.2 W 34 L 29
		
		
		int go_to_floor = strat3950::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
		
		if (!passCount/* && elevator.time_on_the_floor_with_opened_doors >= 40*/)
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
				
				/*int maxRes = -100000;
				int resFloor = -1;
				for (int i = 0; i < LEVELS_COUNT; ++i)
				{
					int res = floorsPass[i] - std::abs(elevator.getFloor() - i) * 10;
					if (res > maxRes)
					{
						maxRes = res;
						resFloor = i;
					}
				}*/
				
				//std::cout << "TODO " << resFloor << std::endl;
				
				/*if (resFloor != -1)
					goToFloor(elevator, resFloor);
				else */if (dir == Direction::UP)
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
	//bool recalcDoorsClose(Simulator &sim, MyElevator &elevator, MyStrategy *strategy);
	
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
		
		//std::bitset<LEVELS_COUNT> levels;
		
		std::multimap<int, MyPassenger *> passengers;
		
		for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
        {
            MyPassenger &passenger = passengerEntry.second;
            int dest_floor = passenger.dest_floor;
			
			if (std::abs(dest_floor - passenger.from_floor) <= 1)
				continue;
			
			/*if (elevator.time_on_the_floor_with_opened_doors < 40 && passenger.side != elevator.side)
				continue;*/
			
			/*if (sim.tick > 6500 && elevator.ind == 3 && passenger.dest_floor != 0)
				continue;*/
			
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
			
			/*for (auto it = passengers.rbegin(); it != passengers.rend(); ++it)
			{
				double dist = std::abs(it->second->x - (double) elevator.x);
				if (dist > maxDist)
					it->second->set_elevator.insert(elevator.id);
			}*/
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
	
    for (int id : elevator.passengers)
    {
        MyPassenger &pass = sim.passengers[id];
		points[pass.dest_floor] -= pass.getValue(elevator.side);
    }
    
    /*for (std::pair<const int, MyPassenger> &p : sim.outPassengers)
	{
		int floor = p.second.getFloor();
		if (floor != level)
		{
			int pts = -10;
			if (p.second.side != elevator.side)
				pts *= 2;
			if (points.count(floor))
				points[floor] += pts;
		}
	}*/
	// 0 RES: 1.06403 9456 10061.5 rem 1683.2 997.2 W 76 L 24 GOOD!
	// 30 RES: 1.03688 9647.1 10002.9 rem 1680 1098.9 W 59 L 40 GOOD
	// 10 RES: 1.04705 9618.8 10071.4 rem 1738.1 973.5 W 62 L 37 GOOD
	// 1 RES: 1.04413 9608.1 10032.1 rem 1668.4 974.9 W 69 L 30 GOOD

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
	
	/*double maxTick = 0;
	for (int i = 0; i < LEVELS_COUNT; ++i)
	{
		if (i != elevator.getFloor())
		{
			int t;
			if ( i > elevator.getFloor())
				t = (i - elevator.getFloor()) / elevator.speed;
			else
				t = (elevator.getFloor() - i) * 50;
			
			if (t > maxTick)
			{
				maxTick = t;
			}
		}
	}*/
	
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
				// RES: 1.01338 9188.43 9311.33 rem 1149.8 841.1 W 56.6667 L 43
				// RES: 1.02037 9189.1 9376.3 rem 1146.83 780 W 60.3333 L 38.3333
				// 450 RES: 1.01813 9555.4 9728.6 rem 1217.7 786.3 W 59 L 40
				// 490 RES: 1.02894 9492.8 9767.5 rem 1179.4 798.6 W 64 L 36
				// 500 RES: 1.01935 9535.6 9720.1 rem 1160.78 791.38 W 57.8 L 42
				// 510 RES: 1.04185 9474.4 9870.9 rem 1180 778.8 W 63 L 37 GOOD
				// 520 RES: 1.01404 9669.9 9805.7 rem 1163 843.3 W 54 L 46
				//makeMoveSimple(enemySide, copy.sim);
				copy.makeMove();
				copy.sim.step();
				
				/*if (el.id == 4 && i == 7)
					std::cout << "Y " << tick << " " << el.y << " " << getEStateName(el.state) << std::endl;*/
				
				if (el.state == EState::CLOSING && el.closing_or_opening_ticks == 98)
				{
					//std::cout << "RET << " << tick << std::endl;
					//break;
				}
			}
			
			int valLeft = copy.sim.scores[0] + copy.sim.passengersTotal[0]*1 + copy.sim.totalCargoValue2(Side::LEFT).points*0.5 * coef;
			int valRight = copy.sim.scores[1] + copy.sim.passengersTotal[1]*1 + copy.sim.totalCargoValue2(Side::RIGHT).points*0.5 * coef;
		
			
			int points;
			if (copy.side == Side::LEFT)
				points = valLeft - valRight;
			else
				points = valRight - valLeft;
			
			
			if (i == elevator.next_floor)
				++points;
			
			//points -= std::abs(elevator.getFloor() - i)*30;
			
			//std::cout << "PTS " << i << " " << valLeft << " " << valRight << " " << points << std::endl;
			//std::cout << "SCR " << i << " " << copy.sim.scores[0] << " " << copy.sim.scores[1] << " " << points << std::endl;
			
			for (MyElevator &e : sim.elevators)
			{
				if (e.id != elevator.id && e.state == EState::MOVING && e.next_floor == i)
				{
					if (e.side == elevator.side && e.ind < elevator.ind)
					{
						points -= 200;
					}
					/*else if (e.side != elevator.side && e.ind > elevator.ind)
					{
						points += 1;
					}*/
					
					// 100 RES: 1.03208 9524.5 9830 rem 1188.2 796.4 W 66 L 34 GOOD
					// 200 RES: 1.03209 9510.9 9816.1 rem 1200.2 766.5 W 69 L 30 GOOD
					// 250 RES: 1.03163 9506 9806.7 rem 1194.1 778.1 W 68 L 31 GOOD
				}
			}
			
			//points -= tick * 0.5;
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

}

namespace strat4559 { 
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
     
    if (elevator.passengers.size() == MAX_PASSENGERS) 
    { 
      int go_to_floor = strat4559::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor()); 
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
        if (e.id != elevator.id && e.ind < elevator.ind && e.state == EState::FILLING && e.getFloor() == floor && e.passengers.size() < 19) 
        { 
          anyElevatorsCloser = true; 
        } 
      }*/ 
       
      if (!anyElevatorsCloser) 
      { 
        //int maxWait = 300 - std::max(0, (int) (elevator.passengers.size()) - 12) * 25; 
        int maxWait = 550 - std::max(0, (int) (elevator.passengers.size()) - 12) * 50; 
        int ticksToWait = std::min(std::max(0, 7200 - sim.tick - value.ticks), maxWait); 
        //std::cout << "Wait " << ticksToWait << std::endl; 
         
        // 450 RES: 0.995443 9296.17 9253.8 rem 1138.53 1094.03 W 50.3333 L 49 
        // 500 RES: 0.996442 9276.03 9243.03 rem 1120.8 1096.4 W 52.3333 L 47 
        // 550 RES: 0.994873 9211.97 9164.73 rem 1095.67 1100.7 W 52 L 47.3333 
        // RES: 1.00964 9188.13 9276.73 rem 1109.53 1112.33 W 56 L 43 
         
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
      } 
    } 
     
    // RES: 0.994943 8661.2 8617.4 rem 953.7 944.5 W 39 L 35 
    // 100 RES: 1.0044 8900.5 8939.7 rem 987.4 981.3 W 44 L 37 
    // 200 RES: 1.01427 8606.9 8729.7 rem 967.7 899.7 W 41 L 25 
    // 300 RES: 1.00729 8577.4 8639.9 rem 927.7 896.5 W 36 L 27 
    // 400 RES: 1.00842 8525.4 8597.2 rem 936.3 866.2 W 34 L 29 
     
     
    int go_to_floor = strat4559::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor()); 
     
    if (!passCount/* && elevator.time_on_the_floor_with_opened_doors >= 40*/) 
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
         
        /*int maxRes = -100000; 
        int resFloor = -1; 
        for (int i = 0; i < LEVELS_COUNT; ++i) 
        { 
          int res = floorsPass[i] - std::abs(elevator.getFloor() - i) * 10; 
          if (res > maxRes) 
          { 
            maxRes = res; 
            resFloor = i; 
          } 
        }*/ 
         
        //std::cout << "TODO " << resFloor << std::endl; 
         
        /*if (resFloor != -1) 
          goToFloor(elevator, resFloor); 
        else */if (dir == Direction::UP) 
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
  //bool recalcDoorsClose(Simulator &sim, MyElevator &elevator, MyStrategy *strategy); 
   
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
     
    //std::bitset<LEVELS_COUNT> levels; 
     
    std::multimap<int, MyPassenger *> passengers; 
     
    for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers) 
        { 
            MyPassenger &passenger = passengerEntry.second; 
            int dest_floor = passenger.dest_floor; 
       
      if (std::abs(dest_floor - passenger.from_floor) <= 1) 
        continue; 
       
      /*if (elevator.time_on_the_floor_with_opened_doors < 40 && passenger.side != elevator.side) 
        continue;*/ 
       
      /*if (sim.tick > 6500 && elevator.ind == 3 && passenger.dest_floor != 0) 
        continue;*/ 
       
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
       
      /*for (auto it = passengers.rbegin(); it != passengers.rend(); ++it) 
      { 
        double dist = std::abs(it->second->x - (double) elevator.x); 
        if (dist > maxDist) 
          it->second->set_elevator.insert(elevator.id); 
      }*/ 
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
                 
        if (el.state == EState::CLOSING && el.closing_or_opening_ticks == 98) 
        { 
          //std::cout << "RET << " << tick << std::endl; 
          //break; 
        } 
      } 
       
      int points; 
       
      if (false && sim.tick < 5900) 
      { 
        double newPassVal = 0.0; 
        int passGone = 0; 
         
        double passCount[LEVELS_COUNT]; 
        for (int k = 0; k < LEVELS_COUNT; ++k) 
          passCount[k] = 0.0; 
         
        for (int id : el.passengers) 
        { 
          MyPassenger &pass = copy.sim.passengers[id]; 
          if (!elevator.passengers.count(id)) 
          { 
            newPassVal += pass.getValue2(elevator.side); 
          } 
        } 
         
        points = newPassVal; 
         
        for (int id : elevator.passengers) 
        { 
          if (!el.passengers.count(id)) 
          { 
            ++passGone; 
          } 
        } 
         
        if (passGone > 2) 
        { 
          points += (passGone - 2) * 30; 
        } 
        else if (passGone <= 1) 
        { 
          points -= 110; 
        } 
         
        if (passGone < 11 && i == 0) 
          points -= 1000; 
      } 
      else 
      { 
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
      } 
       
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
}

namespace strat4932 {
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
			int go_to_floor = strat4932::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
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
				if (e.id != elevator.id && e.ind < elevator.ind && e.state == EState::FILLING && e.getFloor() == floor && e.passengers.size() < 19)
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
					// RES: 400 *50  1.05554 10389 10966 rem 797.6 1291.3 W 72 L 28 GOOD!
					// 300 50 RES: 1.0576 10393.3 10992 rem 788.9 1297.1 W 73 L 27 GOOD!
					// 250 50 RES: 1.05988 10396.2 11018.7 rem 815.1 1298.6 W 72 L 28 GOOD!
					// 250 55 RES: 1.06133 10379.1 11015.6 rem 815.7 1293.8 W 72 L 28 GOOD!
					// 250 60 RES: 1.06206 10375.2 11019.1 rem 817.7 1285.5 W 72 L 28 GOOD!
					// 250 65 RES: 1.05856 10392.5 11001.1 rem 796.9 1290.5 W 73 L 27 GOOD!
					//std:: cout << (t - sim.tick) << " " << passCount << std::endl;
					passCount = 0;
				}
				
				/*if (elevator.ind == 0 && elevator.getFloor() == 8)
					std::cout << sim.tick << " Wait " << ticksToWait << " pc " << passCount << " " << elevator.time_on_the_floor_with_opened_doors << " " << maxWait << " " << std::endl;*/
			}
		}
		
		// RES: 0.994943 8661.2 8617.4 rem 953.7 944.5 W 39 L 35
		// 100 RES: 1.0044 8900.5 8939.7 rem 987.4 981.3 W 44 L 37
		// 200 RES: 1.01427 8606.9 8729.7 rem 967.7 899.7 W 41 L 25
		// 300 RES: 1.00729 8577.4 8639.9 rem 927.7 896.5 W 36 L 27
		// 400 RES: 1.00842 8525.4 8597.2 rem 936.3 866.2 W 34 L 29
		
		
		int go_to_floor = strat4932::getNearestToLevelDestinationNoRand(sim, elevator, elevator.getFloor());
		
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
				
				/*int maxRes = -100000;
				int resFloor = -1;
				for (int i = 0; i < LEVELS_COUNT; ++i)
				{
					int res = floorsPass[i] - std::abs(elevator.getFloor() - i) * 10;
					if (res > maxRes)
					{
						maxRes = res;
						resFloor = i;
					}
				}*/
				
				//std::cout << "TODO " << resFloor << std::endl;
				
				/*if (resFloor != -1)
					goToFloor(elevator, resFloor);
				else */if (dir == Direction::UP)
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
	//bool recalcDoorsClose(Simulator &sim, MyElevator &elevator, MyStrategy *strategy);
	
	void goToFloor(MyElevator &elevator, int go_to_floor)
	{
        if (go_to_floor > elevator.getFloor())
            dir = Direction::UP;
        else
            dir = Direction::DOWN;
        elevator.go_to_floor = go_to_floor;
		
		//std::cout << "GT " << elevator.ind << " " << go_to_floor << std::endl;
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
		
		//std::bitset<LEVELS_COUNT> levels;
		
		std::multimap<int, MyPassenger *> passengers;
		
		for (std::pair<const int, MyPassenger> & passengerEntry : sim.passengers)
        {
            MyPassenger &passenger = passengerEntry.second;
            int dest_floor = passenger.dest_floor;
			
			if (std::abs(dest_floor - passenger.from_floor) <= 1)
				continue;
			
			/*if (elevator.time_on_the_floor_with_opened_doors < 40 && passenger.side != elevator.side)
				continue;*/
			
			/*if (sim.tick > 6500 && elevator.ind == 3 && passenger.dest_floor != 0)
				continue;*/
			
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
			
			/*for (auto it = passengers.rbegin(); it != passengers.rend(); ++it)
			{
				double dist = std::abs(it->second->x - (double) elevator.x);
				if (dist > maxDist)
					it->second->set_elevator.insert(elevator.id);
			}*/
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
	
	/*double maxTick = 0;
	for (int i = 0; i < LEVELS_COUNT; ++i)
	{
		if (i != elevator.getFloor())
		{
			int t;
			if ( i > elevator.getFloor())
				t = (i - elevator.getFloor()) / elevator.speed;
			else
				t = (elevator.getFloor() - i) * 50;
			
			if (t > maxTick)
			{
				maxTick = t;
			}
		}
	}*/
	
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
								
				if (el.state == EState::CLOSING && el.closing_or_opening_ticks == 98)
				{
					//std::cout << "RET << " << tick << std::endl;
					//break;
				}
			}
			
			int points;
			
			if (false && sim.tick < 5900)
			{
				double newPassVal = 0.0;
				int passGone = 0;
				
				double passCount[LEVELS_COUNT];
				for (int k = 0; k < LEVELS_COUNT; ++k)
					passCount[k] = 0.0;
				
				for (int id : el.passengers)
				{
					MyPassenger &pass = copy.sim.passengers[id];
					if (!elevator.passengers.count(id))
					{
						newPassVal += pass.getValue2(elevator.side);
					}
					
					/*if (pass.dest_floor_confirmed)
					{
						passCount[pass.dest_floor]++;
					}
					else
					{
						int cnt = pass.visitedLevels.count();
						if (cnt < 1 || cnt > 6)
							std::cout << "ERR invalid visitedLevels "  << cnt << std::endl;
		
						for (int i = 1; i < LEVELS_COUNT; ++i)
						{
							if (!pass.visitedLevels.test(i))
							{
								passCount[i] += (6 - cnt) * 0.2 / (double) (LEVELS_COUNT - cnt);
							}
						}
						
						passCount[0] += (cnt - 1) * 0.2;
					}*/
				}
				
				points = newPassVal;
				
				/*double totalGrouping = 0.0;
				for (int k = 0; k < LEVELS_COUNT; ++k) {
					double grouping = (std::pow(1.5, passCount[k]) - 1.0);
					totalGrouping += grouping;
					//std::cout << grouping << std::endl;
				}
				
				//std::cout << "TG " << totalGrouping << std::endl;
				points += totalGrouping * 0.5;*/
				
				for (int id : elevator.passengers)
				{
					if (!el.passengers.count(id))
					{
						++passGone;
					}
				}
				
				if (passGone > 2)
				{
					points += (passGone - 2) * 30;
				}
				else if (passGone <= 1)
				{
					points -= 110;
					
					// 110 1.03101 9297.9 9586.2 rem 907.2 865.1 W 63 L 36 GOOD
				}
				else if (passGone <= 2)
				{
					//points -= 30;
					
					// 50 RES: 1.02466 9323.2 9553.1 rem 915.8 884.7 W 61 L 38
					// 40 RES: 1.02705 9308.6 9560.4 rem 909 842.7 W 62 L 37
					// 30 RES: 1.03101 9297.9 9586.2 rem 907.2 865.1 W 63 L 36 GOOD
					// 20 RES: 1.02309 9324.3 9539.6 rem 900.1 876.4 W 58 L 42
				}
				
				// 10 RES: 1.0314 9374.7 9669.1 rem 955.6 920.7 W 66 L 34 GOOD
				// 11 RES: 1.03258 9364.7 9669.8 rem 924.8 927.5 W 66 L 34 GOOD
				// 12 RES: 1.03253 9357.6 9662 rem 926.1 923.3 W 66 L 34 GOOD
				// 15 RES: 1.02841 9359.6 9625.5 rem 951.3 904.8 W 64 L 36
				if (passGone < 11 && i == 0)
					points -= 1000;
				
				// 90 RES: 1.00636 9419.9 9479.8 rem 914.5 973.1 W 54 L 46
				// 100 RES: 1.01301 9357.4 9479.1 rem 925.3 941.1 W 57 L 42
				// 110 RES: 1.01467 9380.5 9518.1 rem 901.5 928 W 60 L 40
				// 120 RES: 1.00926 9379.8 9466.7 rem 891.4 933.9 W 56 L 44
			}
			else
			{
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
			}
			
			if (i == elevator.next_floor)
				++points;
			
			//points -= std::abs(elevator.getFloor() - i)*30;
			
			//std::cout << "PTS " << i << " " << valLeft << " " << valRight << " " << points << std::endl;
			//std::cout << "SCR " << i << " " << copy.sim.scores[0] << " " << copy.sim.scores[1] << " " << points << std::endl;
			
			for (MyElevator &e : sim.elevators)
			{
				if (e.id != elevator.id && e.state == EState::MOVING && e.next_floor == i)
				{
					if (e.side == elevator.side && e.ind < elevator.ind)
					{
						points -= 200;
					}
					/*else if (e.side != elevator.side && elevator.passengers.size() < 10)
					{
						double enemy_ttf = e.time_to_floor;
						double my_ttf = elevator.calcTimeToFloor(i, sim);
						if (enemy_ttf > my_ttf + 45 && enemy_ttf < my_ttf + 145)
						{
							bool anyOtherEl = false;
							for (MyElevator &o : sim.elevators)
							{
								if (o.id != elevator.id && o.id != e.id)
								{
									if (o.state == EState::MOVING && e.next_floor == i && e.time_to_floor < my_ttf ||
										(o.state == EState::OPENING || o.state == EState::WAITING) && o.getFloor() == i
									)
									{
										anyOtherEl = true;
										break;
									}
								}
							}
							
							if (!anyOtherEl)
							{
								int floorPassCount = 0;
								for (auto && p = sim.outPassengers.begin(); p != sim.outPassengers.upper_bound(sim.tick + my_ttf + 145); ++p)
								{
									if (p->second.getFloor() == i && (p->first + p->second.time_to_away) > (sim.tick + my_ttf + 105) && (p->first) < (sim.tick + my_ttf + 145))
									{
										++floorPassCount;
									}
								}
								if (floorPassCount >= 10)
								{
									//std::cout << "YY " << i << " et " << enemy_ttf << " mt " << my_ttf << " e " << e.ind << " mi " << elevator.ind << " pp " << floorPassCount << std::endl;
									//points += std::min(floorPassCount, (int) (20 - elevator.passengers.size())) * 20;
								}
							}
						}
					}*/
					
					// 100 RES: 1.03208 9524.5 9830 rem 1188.2 796.4 W 66 L 34 GOOD
					// 200 RES: 1.03209 9510.9 9816.1 rem 1200.2 766.5 W 69 L 30 GOOD
					// 250 RES: 1.03163 9506 9806.7 rem 1194.1 778.1 W 68 L 31 GOOD
				}
			}
			
			//points -= tick * 0.5;
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
}

//#ifdef ENABLE_LOGGING
inline std::ostream &operator << ( std::ostream &str, const MyPassenger&p )
{
	str << "PAS(" << p.id << " " << p.x << "," << p.y << " " << getPStateName(p.state) << ")";
	return str;
}
//#endif

void compareStrategies()
{
	double totalLeft = 0;
	double totalRight = 0;
	double totalLeftRem = 0;
	double totalRightRem = 0;
	int iterations = 300;
	int won = 0;
	int loss = 0;
	
	int scoresByElevator[8] = {};
	
	for (int i = 0; i < iterations; ++i)
	{
		Simulator simulator;
		simulator.random.m_w = 251000 + i*12345;
		//StratE3 stratLeft(Side::LEFT);
		//strat2661::MyStrategy stratLeft(Side::LEFT);
		//strat2724::MyStrategy stratLeft(Side::LEFT);
		//strat2806::MyStrategy stratLeft(Side::LEFT);
		//strat3340::MyStrategy stratLeft(Side::LEFT);
		//strat3659::MyStrategy stratLeft(Side::LEFT);
		//strat3800::MyStrategy stratLeft(Side::LEFT);
		//strat3950::MyStrategy stratLeft(Side::LEFT);
		//strat4559::MyStrategy stratLeft(Side::LEFT);
		strat4932::MyStrategy stratLeft(Side::LEFT);
		stratLeft.sim.random.m_w = 30000 + i*89741;
		MyStrategy stratRight(Side::RIGHT);
		//stratRight.sim.random.m_w = 20000 + i;
		//stratRight.sim.random.m_w = 210000 + i*12345;
		//strat3340::MyStrategy stratRight(Side::RIGHT);
		for (int j = 0; j < 7200; ++j)
		{
			//makeMove(Side::LEFT, simulator);
			//makeMove3(Side::LEFT, simulator);
			//makeMove6(Side::RIGHT, simulator);
			stratLeft.makeMove(simulator);
			stratRight.makeMove(simulator);
			simulator.step();
		}
		
		totalLeft += simulator.scores[0];
		totalRight += simulator.scores[1];
		
		int val1 = simulator.totalCargoValue(Side::LEFT).points;
		int val2 = simulator.totalCargoValue(Side::RIGHT).points;
		totalLeftRem += val1;
		totalRightRem += val2;
		
		std::cout << i << " - " << simulator.scores[0] << " -- " << simulator.scores[1] << " \tW " << won << " L " << loss << " \tp1 " 
		<< simulator.passengersTotal[0] << " p2 " << simulator.passengersTotal[1] << " v1 " << val1 << " v2 " << val2 << std::endl;
		
		for (int i = 0; i < 8; ++i)
		{
			scoresByElevator[i] += simulator.scoresByElevators[i];
		}
		
		if (simulator.scores[1] > simulator.scores[0])
			++won;
		
		if (simulator.scores[1] < simulator.scores[0])
			++loss;
	}
	
	for (int i = 0; i < 8; ++i)
	{
		std::cout << "EL " << i << " - " << scoresByElevator[i] << std::endl;
	}
		
	
	std::cout << "RES: " << ((totalRight + 1) / (totalLeft + 1)) 
	<< " " << (totalLeft / iterations) << " " << (totalRight / iterations)
	<< " rem " << (totalLeftRem / iterations) << " " << (totalRightRem / iterations) 
	<< " W " << ((float) won * 100 / (float) iterations) 
	<< " L " << ((float) loss * 100 / (float) iterations) 
	<< (won > loss && totalRight > totalLeft*1.03 ? " GOOD" : "")
	<< (won > loss*1.5 && totalRight > totalLeft*1.05 ? "!" : "")
	<< (won > loss*2 && totalRight > totalLeft*1.1 ? "!" : "")
	<< (won > loss*4 && totalRight > totalLeft*1.2 ? "!" : "")
	<< (won > loss*10 && totalRight > totalLeft*1.2 ? "!" : "")
	<< std::endl;
}

/*
 
	makeMove2 / makeMove = 2.5125
	makeMove2 / empty = RES: 6.37634e+06 0 6376.34
	
	makeMove3 / makeMove2 = RES: 1.14168 4925.46 5623.28
	makeMove3 / makeMove = RES: 3.1594 2194.47 6933.21
	makeMove3 / empty = RES: 7.19979e+06 0 7199.79
	
	makeMove4 / makeMove3 = 1.03303
	makeMove4 / makeMove2 = 1.10898
	makeMove4 / makeMove = 2.96583
	makeMove4 / empty = RES: 7.04521e+06 0 7045.21
	
	makeMove6 / makeMove = RES: 3.14826 2161.13 6803.8
	makeMove6 / makeMove2 = RES: 1.58942 4275.55 6795.64
	makeMove6 / makeMove3 = RES: 1.18242 5503.3 6507.22
	makeMove6 / empty = RES: 7.11937e+06 0 7119.37
	
	StratE1 / makeMove3 = RES: 1.19731 5839.35 6991.51
	StratE1 / empty = RES: 7.81825e+06 0 7818.25
	
	StratE2 / makeMove = RES: 4.37403 1961.95 8581.64
	StratE2 / makeMove3 = RES: 1.39153 5690.72 7918.78
	StratE2 / empty = RES: 9.08612e+06 0 9086.12
	
	StratE2 / StratE1 = RES: 1.05728 5198.28 5496.02
	
	StratE3 / StratE1 = RES: 1.56216 4432.32 6924
	StratE3 / StratE2 = RES: 1.64814 4430.64 7302.33
	StratE3 / makeMove3 = RES: 1.45618 5695.04 8293.02
	StratE3 / makeMove = RES: 4.3705 1943.73 8495.07
	StratE3 / empty = RES: 8.73645e+06 0 8736.45
	
	
	//-------------------------------------------
	
	makeMove6 / makeMove3 = RES: 0.97417 7436.3 7244.22
	
	StratE3 / makeMove = RES: 4.31788 2197.65 9489.2
	StratE3 / makeMove3 = RES: 1.26638 7114.21 9009.32
	
	strat2661 / StratE3 = RES: 1.26606 7582.26 9599.58 W 98.6
	
	// strat2724 / strat2661 = RES: 1.20334 7908.47 9516.57 W 95.6667 L 4.33333 GOOD!!!!
	
	// MyStrategy / strat2724 RES: RES: 1.06211 8959.5 9515.93 W 70 L 29.6667 GOOD!
	// MyStrategy / strat2661 = RES: 1.22978 8289.23 10194 W 96.6667 L 3.33333 GOOD!!!!
	// MyStrategy / StratE3 = RES: 1.38621 7539.5 10451.3 W 100 L 0 GOOD!!!!
	
	// MyStrategy / StratE3 = RES: 1.5159 7342.07 11129.8 W 100 L 0 GOOD!!!!
	// MyStrategy / strat2661 = RES: 1.27379 8212.83 10461.4 W 98.3333 L 1.66667 GOOD!!!!
	// MyStrategy / strat2724 RES: 1.04207 9172.1 9558 W 63.3333 L 36.6667 GOOD
	// MyStrategy / strat2806 RES: RES: 1.05425 9396.6 9906.4 W 72.3333 L 27.3333 GOOD!
	// MyStrategy / empty = RES: 4.78316e+06 0 15943.9 W 100 L 0 GOOD!!!!
	
	// strat3659 / strat3340 = RES: 1.05113 9493.43 9978.83 rem 1720.13 1034.4 W 66.6667 L 32.3333 GOOD!
	// strat3659 / strat2661 = RES: 1.26638 8251.33 10449.3 rem 1090.1 1255.77 W 98 L 2 GOOD!!!!
	// strat3659 / strat2806 = RES: 1.05922 9397.97 9954.5 rem 1865.27 1021.7 W 70 L 29.6667 GOOD!
	//                          RES: 1.05689 9546.03 10089.1 rem 1698.07 1026.67 W 72 L 27.6667 GOOD!
	// strat3659 / empty RES: 4.83079e+06 0 16102.6 rem 0 1611.67 W 100 L 0 GOOD!!!!
	
	// strat3340 / strat2806 RES: 0.997664 8702.57 8682.23 rem 1573.33 1452.33 W 46.3333 L 53
	
	// strat3800 / strat3340 RES: 1.10477 9368.03 10349.6 rem 1808.43 1076.03 W 80 L 19.3333 GOOD!!
	// strat3800 / strat3659 RES: 1.05104 9804.67 10305.1 rem 1172.2 1193.33 W 66.3333 L 33 GOOD!
	
	// MyStrategy / strat3659 RES: 1.05092 9787.33 10285.7 rem 1194.83 807.967 W 68.3333 L 31 GOOD!
	// MyStrategy / strat3659bug RES: 1.05368 9786.8 10312.2 rem 1219.17 800.667 W 66.6667 L 33.3333 GOOD!
	// MyStrategy / strat3340 RES: 1.10873 9392.4 10413.7 rem 1849.73 742.467 W 83 L 16.6667 GOOD!!
	// MyStrategy / strat3800 RES: 1.01557 9159.94 9302.56 rem 1162.6 759.3 W 56.6 L 43
	                          RES: 1.01588 9543.22 9694.78 rem 1188.06 822.76 W 57.6 L 42.4
	                RES: 1.03689 9270.47 9612.43 rem 1147.43 906.933 W 63 L 36.3333 GOOD
    // MyStrategy / strat3659bug RES: 1.04707 9820.6 10282.9 rem 1187.24 854.76 W 65.6 L 34 GOOD
    // MyStrategy / empty RES: 4.81407e+06 0 16046.9 rem 0 1245.8 W 100 L 0 GOOD!!!!
    
    // strat3950 / strat3800 RES: 1.03689 9270.47 9612.43 rem 1147.43 906.933 W 63 L 36.3333 GOOD
    // strat3950 / strat3659bug RES: 1.06555 9741 10379.6 rem 1181.4 947.867 W 72 L 28 GOOD!
    
    // strat4747 / strat3800 RES: RES: 1.0297 9406.03 9685.37 rem 1133.33 944.567 W 62.6667 L 36.6667
    
    // /strat4932 /strat4559 RES: RES: 1.11724 9632.83 10762.2 rem 900.6 1315.27 W 89 L 11 GOOD!!
    
*/

// namespace random_test {
// #define N 624
// #define M 397
// #define MATRIX_A 0x9908b0dfUL   /* constant vector a */
// #define UPPER_MASK 0x80000000UL /* most significant w-r bits */
// #define LOWER_MASK 0x7fffffffUL /* least significant r bits */
// 
// typedef struct {
//     unsigned long state[N];
//     int index;
// } RandomObject;
// 
// static unsigned long
// genrand_int32(RandomObject *self)
// {
//     unsigned long y;
//     static unsigned long mag01[2]={0x0UL, MATRIX_A};
//     /* mag01[x] = x * MATRIX_A  for x=0,1 */
//     unsigned long *mt;
// 
//     mt = self->state;
//     if (self->index >= N) { /* generate N words at one time */
//         int kk;
// 
//         for (kk=0;kk<N-M;kk++) {
//             y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
//             mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
//         }
//         for (;kk<N-1;kk++) {
//             y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
//             mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
//         }
//         y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
//         mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];
// 
//         self->index = 0;
//     }
// 
//     y = mt[self->index++];
//     y ^= (y >> 11);
//     y ^= (y << 7) & 0x9d2c5680UL;
//     y ^= (y << 15) & 0xefc60000UL;
//     y ^= (y >> 18);
//     return y;
// }
// 
// float random_random(RandomObject *self)
// {
//     unsigned long a=genrand_int32(self)>>5, b=genrand_int32(self)>>6;
//     return (a*67108864.0+b)*(1.0/9007199254740992.0);
// }
// 
// /* initializes mt[N] with a seed */
// static void
// init_genrand(RandomObject *self, unsigned long s)
// {
//     int mti;
//     unsigned long *mt;
// 
//     mt = self->state;
//     mt[0]= s & 0xffffffffUL;
//     for (mti=1; mti<N; mti++) {
//         mt[mti] =
//         (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
//         /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
//         /* In the previous versions, MSBs of the seed affect   */
//         /* only MSBs of the array mt[].                                */
//         /* 2002/01/09 modified by Makoto Matsumoto                     */
//         mt[mti] &= 0xffffffffUL;
//         /* for >32 bit machines */
//     }
//     self->index = mti;
//     return;
// }
// 
// /* initialize by an array with array-length */
// /* init_key is the array for initializing keys */
// /* key_length is its length */
// void
// init_by_array(RandomObject *self, unsigned long init_key[], unsigned long key_length)
// {
//     unsigned int i, j, k;       /* was signed in the original code. RDH 12/16/2002 */
//     unsigned long *mt;
// 
//     mt = self->state;
//     init_genrand(self, 19650218UL);
//     i=1; j=0;
//     k = (N>key_length ? N : key_length);
//     for (; k; k--) {
//         mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1664525UL))
//                  + init_key[j] + j; /* non linear */
//         mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
//         i++; j++;
//         if (i>=N) { mt[0] = mt[N-1]; i=1; }
//         if (j>=key_length) j=0;
//     }
//     for (k=N-1; k; k--) {
//         mt[i] = (mt[i] ^ ((mt[i-1] ^ (mt[i-1] >> 30)) * 1566083941UL))
//                  - i; /* non linear */
//         mt[i] &= 0xffffffffUL; /* for WORDSIZE > 32 machines */
//         i++;
//         if (i>=N) { mt[0] = mt[N-1]; i=1; }
//     }
// 
//     mt[0] = 0x80000000UL; /* MSB is 1; assuring non-zero initial array */
// }
// 
// }

int main(int argc, char **argv) {
	
	/*float res = 0;
	
	for (int i = 0; i < 1000000; ++i)
	{
		random_test::RandomObject ro;
		unsigned long init_key[1];
		init_key[0] = 1;
		init_by_array(&ro, init_key, 1);
		
		float f = random_test::random_random(&ro);
		res += f;
	}
	
	std::cout << "Your seed produced: " << res << std::endl;
	
	return 0;
	
	//srand(time(0));
	srand(13);*/
	
	compareStrategies();
	return 0;
	
	
	Simulator simulator;
	int i = 11;
	simulator.random.m_w = 210000 + i*12345;
	
	//StratE3 stratLeft(Side::LEFT);
	strat4559::MyStrategy stratLeft(Side::LEFT);
	stratLeft.sim.random.m_w = 30000 + i*89741;
	MyStrategy stratRight(Side::RIGHT);
	
	g_realsimulator = &simulator;
	g_mysimulator = &stratRight.sim;
		
    Renderer renderer;
	renderer.init();
	
	vg = nvgCreateGL2(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
	int font = nvgCreateFont(vg, "sans", "/media/denis/tmp/bb_ai/nanovg/nanovg/example/Roboto-Regular.ttf");
	
	
	for (int i = 0; ; ++i) {
		if (i < 7200)
		{
			//makeMove3(Side::LEFT, simulator);
			//makeMove6(Side::RIGHT, simulator);
			stratLeft.makeMove(simulator);
			stratRight.makeMove(simulator);
			simulator.step();
		}
		/*if (simulator.passengers.count(0))
			std::cout << simulator.tick << " " << simulator.passengers[0] << std::endl;
		
		if (simulator.passengers.count(1))
			std::cout << simulator.tick << " " << simulator.passengers[1] << std::endl;*/
		
		if (i % renderTickMod == 0)
		{
			renderer.startRendering();
			renderer.finishRendering();
		}
	}
    return 0;
}

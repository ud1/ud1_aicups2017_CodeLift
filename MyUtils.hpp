
#ifndef MY_UTILS_HPP
#define MY_UTILS_HPP

#ifdef MY_IMPLEMENTATION
#define GLOBAL_VAR
#else
#define GLOBAL_VAR extern
#endif // MY_IMPLEMENTATION

#if ENABLE_LOGGING
#include <iostream>
#include <fstream>
#include <iomanip>
//GLOBAL_VAR std::ofstream outputFile("out.txt");
#define LOGC(x)  std::cout << x << std::endl;
//#define LOGF(x) outputFile << "T " << g_world->getTick() << "-" << g_self->getTeammateIndex() << "| " << g_self->getRemainingCooldownTicks() << " " << g_self->getRemainingKnockdownTicks() << "| " << g_self->getX() << " " << g_self->getY() << " " << g_self->getAngle() << "| " << g_self->getSpeedX() << " " << g_self->getSpeedY() << "| " << x << std::endl;
#define LOG(x) LOGC(g_world->getTickIndex() << " " << x);

#else
#define LOG(x)

#endif // ENABLE_LOGGING




#define _USE_MATH_DEFINES

#include <cmath>

constexpr double EPS = 1e-7;

constexpr inline double sqr ( double v )
{
	return v*v;
}

constexpr double PI = 3.14159265358979323846;
constexpr double PI05 = PI * 0.5;

inline double normalizeAngle(double angle)
{
	while ( angle > PI ) {
		angle -= 2.0 * PI;
	}

	while ( angle < -PI ) {
		angle += 2.0 * PI;
	}
	
	return angle;
}

inline double atan2_approximation2( double y, double x )
{
	if ( x == 0.0 )
	{
		if ( y > 0.0 )
			return PI05;
		if ( y == 0.0 )
			return 0.0;
		return -PI05;
	}
	
	float atan;
	float z = y/x;
	if ( std::abs( z ) < 1.0 )
	{
		atan = z/(1.0 + 0.28*z*z);
		if ( x < 0.0 )
		{
			if ( y < 0.0 )
				return atan - PI;
			return atan + PI;
		}
	}
	else
	{
		atan = PI05 - z/(z*z + 0.28);
		if ( y < 0.0 )
			return atan - PI;
	}
	return atan;
}

/*inline double angleDiff(double angle1, double angle2)
{
	return std::abs(normalizeAngle(angle1 - angle2));
}*/

struct P {
	explicit P() {}

    constexpr explicit P ( double angle ) : x(cos(angle)), y(sin(angle)) {}
	constexpr explicit P ( double x, double y ) : x ( x ), y ( y ) {}

	double dist ( P const &o ) const {
		return sqrt ( sqr ( x - o.x ) + sqr ( y - o.y ) );
	}
	
	double dist2 ( P const &o ) const {
		return sqr ( x - o.x ) + sqr ( y - o.y );
	}

	double len2() const {
		return x*x + y*y;
	}

	double len() const {
		return sqrt ( x*x + y*y );
	}

	P norm() const {
		double l = len();
		if ( l == 0.0 )
			return P ( 1.0, 0.0 );
		return P ( x / l, y / l );
	}
	
	P conj() const {
		return P(y, -x);
	}

    P conjR() const {
        return P(-y, x);
    }
	
	P &operator += (const P &o)
	{
		x += o.x;
		y += o.y;
		return *this;
	}

    P &operator -= (const P &o)
    {
        x -= o.x;
        y -= o.y;
        return *this;
    }
	
	P &operator *= (double v)
	{
		x *= v;
		y *= v;
		return *this;
	}
	
	P &operator /= (double v)
	{
		double r = 1.0 / v;
		x *= r;
		y *= r;
		return *this;
	}

	bool operator == ( const P &o ) const {
		return ( std::abs ( x - o.x ) + std::abs ( y - o.y ) ) < EPS;
	}

    bool operator != ( const P &o ) const {
        return ( std::abs ( x - o.x ) + std::abs ( y - o.y ) ) >= EPS;
    }

	P rotate ( double angle ) const {
		double cs = cos ( angle );
		double sn = sin ( angle );
		return P ( x * cs - y * sn, x * sn + y * cs );
	}

	double getAngle () const {
		double absoluteAngleTo = atan2 (this->y, this->x );
		return absoluteAngleTo;
	}
	
	double getAngleFast () const {
		double absoluteAngleTo = atan2_approximation2(this->y, this->x );
		return absoluteAngleTo;
	}
	
	P rotate(const P &sinCos) const
	{
		return P(x * sinCos.x - y * sinCos.y, y * sinCos.x + x * sinCos.y);
	}

	double x, y;
};

inline P unrotate(const P &fromDir, const P& toDir)
{
	return P(toDir.x * fromDir.x + toDir.y * fromDir.y, toDir.y * fromDir.x - toDir.x * fromDir.y).norm();
}

inline P clampDir(const P &value, const P &maxValue)
{
	if (value.x >= maxValue.x)
		return value;

	if (value.y > 0.0)
		return maxValue;

	return P(maxValue.x, -maxValue.y);
}

inline bool isAngleLessThan(const P &value1, const P &value2)
{
	return value1.x >= value2.x;
}

inline double clamp ( double v, double vmin, double vmax )
{
	if ( v < vmin )
		return vmin;
	if ( v > vmax )
		return vmax;
	return v;
}

#ifdef ENABLE_LOGGING
inline std::ostream &operator << ( std::ostream &str, const P&p )
{
	str << "(" << p.x << "," << p.y << ")";
	return str;
}
#endif

constexpr inline P operator - ( P p1, P p2 )
{
	return P ( p1.x - p2.x, p1.y - p2.y );
}

constexpr inline P operator + ( P p1, P p2 )
{
	return P ( p1.x + p2.x, p1.y + p2.y );
}

constexpr inline P operator * ( P p1, double v )
{
	return P ( p1.x * v, p1.y * v );
}

constexpr inline P operator / ( P p1, double v )
{
	return P ( p1.x / v, p1.y / v );
}

constexpr inline P operator / ( double v, P p1 )
{
    return P ( v / p1.x, v / p1.y );
}

constexpr inline P operator * ( P p1, P p2 )
{
	return P ( p1.x * p2.x, p1.y * p2.y );
}

constexpr inline P operator / ( P p1, P p2 )
{
    return P ( p1.x / p2.x, p1.y / p2.y );
}

constexpr inline double dot ( P p1, P p2 )
{
	return p1.x * p2.x + p1.y * p2.y;
}

constexpr inline double cross ( P p1, P p2 )
{
	return p1.x * p2.y - p1.y * p2.x;
}



#endif


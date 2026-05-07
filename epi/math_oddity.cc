//------------------------------------------------------------------------
//  Oddball stuff
//------------------------------------------------------------------------
// 
//  Copyright (c) 2003-2008  The EDGE Team.
// 
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//------------------------------------------------------------------------

#include "epi.h"

#include "math_oddity.h"

namespace epi
{

    // SH-4 optimized Quake 3 Fast Inverse Square Root
    float Q3FastInvSqrt(float number)
    {
        if (number <= 0.0f)
            return NAN; // Handle invalid or zero input safely

        float x2 = number * 0.5f;
        float y  = number;

        uint32_t i;
        std::memcpy(&i, &y, sizeof(i));

        // Magic number for IEEE 754 32-bit floats
        i = 0x5f3759df - (i >> 1);

        std::memcpy(&y, &i, sizeof(y));

    #if defined(DREAMCAST)
        // Inline assembly Newton-Raphson refinement using SH-4 FMAC
        // y = y * (1.5f - (x2 * y * y))
        __asm__ volatile (
            "fmul    %1, %0        \n\t" // y = y * y
            "fmul    %2, %0        \n\t" // y = y * x2
            "fneg    %0            \n\t" // y = -y
            "fadd    %3, %0        \n\t" // y = 1.5f - (x2*y*y)
            "fmul    %4, %0        \n\t" // y = y * original_y
            : "+f"(y) // %0 output/input
            : "f"(y), // %1 original y
              "f"(x2), // %2 x2
              "f"(1.5f), // %3 constant 1.5
              "f"(y) // %4 original y again
        );
    #else
        // Portable C++ Newton-Raphson refinement
        y = y * (1.5f - (x2 * y * y));
    #endif

        return y;
}

int int_sqrt(int value)
{
    /* Integer sqrt routine ("divide-and-average") */

	if (value < 0)
		I_Error("epi::int_sqrt : Negative value!\n");

	if (value < 2)
		return value;

    int est = value >> 1;

    /* Unrolled loop (needs 18 iterations) */
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;
    est = (est + value / est) >> 1;  est = (est + value / est) >> 1;

	if (est * est > value) est--;

    return est;
}

inline byte ClampByte(int value)
{
	if (value < 0)
		return 0;

	if (value > 255)
		return 255;

	return value;
}

} // namespace epi

//--- editor settings ---
// vi:ts=4:sw=4:noexpandtab

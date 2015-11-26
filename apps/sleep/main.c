/*
** Copyright 2005, Michael Noisternig. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


int main( int argc, char *argv[] )
{
    double duration = 0.0;

    if ( argc <= 1 )
        goto error;

    for ( argv++; argc > 1; argc--, argv++ ) {
        char *suffix;
        double value = strtod( *argv, &suffix );

        if ( suffix == *argv )
            goto error;

        switch ( *suffix ) {
            case 'd':
                value *= 24.0;
            case 'h':
                value *= 60.0;
            case 'm':
                value *= 60.0;
            case 's':
                if ( *(suffix+1) != '\0' )
                    goto error;
            case '\0':
                break;
            default:
                goto error;
        }

        duration += value;
    }

    if ( duration >= 1.0 ) {
        unsigned int seconds = (unsigned int)duration;
        sleep( seconds );
        duration -= (double)seconds;
    }
    if ( duration > 0.0 )
        usleep( duration * 1e6 );

    return 0;

error:
    printf( "Usage: sleep NUMBER[smhd] ...\n"
            "\n"
            "Pause for an amount of time specified by the sum of the arguments' values.\n"
            "Each argument is made up by a NUMBER which may be any arbitrary floating point\n"
            "number, followed by an optional suffix which may be 's' for seconds (the\n"
            "default), 'm' for minutes, 'h' for hours or 'd' for days.\n"
          );

    return 1;
}


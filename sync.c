//
//   Small utility to keep "in sync" freq (and mode) on SDR++ and the radio.
//
//   It is just a quick and dirty hack, controlling SDR++ via its own rigctl server
//   and the radio via hamlib.
//
//   Installing hamlib is required to run and compile this program
//   (on mac: > brew install hamlib)
//
//   Based on the code by GitHub users lwvmobile and neural75
//   https://github.com/lwvmobile/dsd-fme/blob/main/src/dsd_rigctl.c
//   https://github.com/neural75/gqrx-scanner
//
//   as well as on the test code by hamlib
//   https://hamlib.sourceforge.net/manuals/1.2.15/_2tests_2testrig_8c-example.html
//
//  Encouragements and guidance by lwvmobile and AlexandreRouma were pivotal!
//
//  Compile with:
//     gcc -o sync sync.c -lhamlib -lm
//
//   May 14, 2023 - Michele Giugliano (iv3ifz) - Initial version

// SDR++ part (here the port number is harcoded)
#define HOSTNAME "localhost"
#define PORT 4534 // SDR++'s rigctl server: localhost:port - e.g. 4534

// hamlib part (here both rig and serial port are hardcoded)
#define MY_RIG_MODEL 2037                            // Kenwood TS-590SG
#define SERIAL_PORT "/dev/cu.usbserial-0567003F3120" // Serial port

// Include of libraries
#include <stdio.h>  // printf, scanf, puts, NULL
#include <stdlib.h> // srand, rand
// #include <string.h>   // strlen, strcmp
#include <sys/time.h> //
#include <unistd.h>   // sleep and nanosleep
// #include <math.h>     // round in GetSignalLevel()
#include <signal.h> // CTRL-C handler

#include <netdb.h>      // gethostbyname
#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket, connect, send, recv
#include <sys/types.h>  // socket, connect, send, recv

#include <hamlib/rig.h> // hamlib
#include "sync.h"

// Function prototypes
int connect_SDRpp();
int send_data(int sockfd, char *buf);
int receive_data(int sockfd, char *buf);
long int GetCurrentFreq(int sockfd);
int SetFreq(int sockfd, long int freq);
int SetModulation(int sockfd, int bandwidth);

RIG *connect_rig();
int SetFreq_rig(RIG *my_rig, long int freq);
long int GetFreq_rig(RIG *my_rig);
void intHandler(int dummy);

//------------------------------------------------------------------------------

int main()
{
    long int freq_sdrpp, freq_rig;         // current frequencies
    long int freq_sdrpp_old, freq_rig_old; // old frequencies

    int milliseconds = 25;
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;

    signal(SIGINT, intHandler); // CTRL-C handler
    keepRunning = 1;            // keepRunning flag, set to 0 by CTRL-C

    int sockfd = connect_SDRpp(); // Connect to SDR++'s rigctl server

    RIG *my_rig;
    my_rig = connect_rig(); // Connect to the radio via hamlib
    // rig_set_debug HAMLIB_PARAMS((enum rig_debug_level_e debug_level));
    rig_set_debug(RIG_DEBUG_NONE); // was RIG_DEBUG_ERR

    // Initialization, at the beginning the two frequencies are set the same
    freq_sdrpp = GetCurrentFreq(sockfd);
    freq_sdrpp_old = freq_sdrpp;
    // printf("SDR++ current freq: %ld\n", freq_sdrpp);

    freq_rig = GetFreq_rig(my_rig);
    freq_rig_old = freq_rig;
    // printf("Rig current freq: %ld\n", freq_rig);

    SetFreq(sockfd, freq_rig); // Set the SDR++ frequency to the radio's one

    while (keepRunning)
    {
        freq_sdrpp = GetCurrentFreq(sockfd);
        freq_rig = GetFreq_rig(my_rig);

        if (freq_rig != freq_rig_old)
        {
            SetFreq(sockfd, freq_rig);
            freq_rig_old = freq_rig;
            freq_sdrpp_old = freq_rig;
        }
        else if (freq_sdrpp != freq_sdrpp_old)
        {
            SetFreq_rig(my_rig, freq_sdrpp);
            freq_rig_old = freq_sdrpp;
            freq_sdrpp_old = freq_sdrpp;
        }

        // sleep(1);
        nanosleep(&ts, NULL);
    }

    rig_close(my_rig);
    rig_cleanup(my_rig); /* if you care about memory */
    printf("port %s closed ok \n", SERIAL_PORT);

} // end main()
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
//

// SDR++ part (here the port number is hardcoded)
#define HOSTNAME "localhost"
#define PORT 4534 // SDR++'s rigctl server: localhost:port - e.g. 4534

// hamlib part (here both rig and serial port are hardcoded)
#define MY_RIG_MODEL 2037                            // Kenwood TS-590SG
#define SERIAL_PORT "/dev/cu.usbserial-0567003F3120" // Serial port

// Include of libraries
#include <stdio.h>    // printf, scanf, puts, NULL
#include <stdlib.h>   //
#include <string.h>   // strlen, strcmp
#include <sys/time.h> //
#include <unistd.h>   // sleep and nanosleep
#include <signal.h>   // CTRL-C handler

#include <netdb.h>      // gethostbyname
#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket, connect, send, recv
#include <sys/types.h>  // socket, connect, send, recv

#include <hamlib/rig.h> // hamlib
#include "sync.h"       // custom header, containing the ad hoc functions

// Function prototypes ---------------------------------------------------------
int connect_SDRpp();                                      // Connect to SDR++'s rigctl server
int send_data(int sockfd, char *buf);                     // Send data to SDR++'s rigctl server
int receive_data(int sockfd, char *buf);                  // Receive data from SDR++'s rigctl server
long int GetCurrentFreq(int sockfd);                      // Get the freq from SDR++'s rigctl server
int SetFreq(int sockfd, long int freq);                   // Set the freq on SDR++'s rigctl server
int SetModulation(int sockfd, char *mode, int bandwidth); // Set the mode on SDR++'s rigctl server

RIG *connect_rig();                          // Connect to the radio via hamlib
long int GetFreq_rig(RIG *my_rig);           //  Get the freq from the radio via hamlib
int SetFreq_rig(RIG *my_rig, long int freq); // Set the freq on the radio via hamlib
char *GetModulation_rig(RIG *my_rig);        // Get the mode on the radio via hamlib

void intHandler(int dummy); // CTRL-C handler
//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    // Variables declaration
    long int freq_sdrpp, freq_rig;         // current frequency(ies)
    long int freq_sdrpp_old, freq_rig_old; // previous frequencies
    char mode_rig[4], mode_rig_old[4];     // current mode of the radio
    int bandwidth;                         // bandwidth of the SDR++ mode

    int milliseconds = 25;                        // milliseconds to sleep
    struct timespec ts;                           // struct for nanosleep
    ts.tv_sec = milliseconds / 1000;              // seconds to sleep
    ts.tv_nsec = (milliseconds % 1000) * 1000000; // nanoseconds to sleep

    //-----------------------------
    // Let's go! First we connect to SDR++'s rigctl server, otherwise we exit
    int sockfd = connect_SDRpp(); // Connect to SDR++'s rigctl server

    // Then we connect to the radio via hamlib, otherwise we exit
    RIG *my_rig;
    my_rig = connect_rig();        // Connect to the radio via hamlib
    rig_set_debug(RIG_DEBUG_NONE); // was RIG_DEBUG_ERR - Set the debug level
    //-----------------------------

    //-----------------------------
    // INIT: the freqs of radio & SDR++ are set to same value (i.e. the radio's)
    freq_rig = GetFreq_rig(my_rig); // Get the freq from the radio via hamlib
    freq_rig_old = freq_rig;        // previous freq of the radio set to current one
    // printf("Rig current freq: %ld\n", freq_rig);

    SetFreq(sockfd, freq_rig); // Set the SDR++ frequency to the radio's one

    // Get the mode of the radio via hamlib
    sprintf(mode_rig, "%s", GetModulation_rig(my_rig));
    // printf("Rig current mode: %s\n", mode_rig);
    memccpy(mode_rig_old, mode_rig, 0, 3); // previous mode of the radio set to current one
    SetModulation(sockfd, mode_rig, 3000); // Set the SDR++ mode to the radio's on
    //--------------------------------

    // Main loop - we keep running until CTRL-C is pressed
    signal(SIGINT, intHandler); // CTRL-C handler
    keepRunning = 1;            // keepRunning flag, set to 0 by CTRL-C
    while (keepRunning)         // Bidirectional synchronisation of frequencies
    {
        freq_sdrpp = GetCurrentFreq(sockfd); // Get the freq from SDR++'s rigctl server
        freq_rig = GetFreq_rig(my_rig);      // Get the freq from the radio via hamlib

        if (freq_rig != freq_rig_old) // the radio's freq has changed
        {
            SetFreq(sockfd, freq_rig); // Set the SDR++ frequency to the radio's one
            freq_rig_old = freq_rig;   // previous freq of the radio set to current one
            freq_sdrpp_old = freq_rig; // previous freq of SDR++ set to current one
        }
        else if (freq_sdrpp != freq_sdrpp_old) // the SDR++'s freq has changed
        {
            SetFreq_rig(my_rig, freq_sdrpp); // Set the radio's frequency to the SDR++'s one
            freq_rig_old = freq_sdrpp;       // previous freq of the radio set to current one
            freq_sdrpp_old = freq_sdrpp;     // previous freq of SDR++ set to current one
        }

        // Unidirectional synchronisation of the mode
        sprintf(mode_rig, "%s", GetModulation_rig(my_rig)); // Get the mode of the radio via hamlib
        if (strcmp(mode_rig, mode_rig_old) != 0)            // if rig's mode has changed
        {
            switch (mode_rig[0])
            {
            case 'F':
                bandwidth = 2600; // default value
                break;
            case 'A':
                bandwidth = 5000; // default value
                break;
            case 'L':
                bandwidth = 3000; // default value
                break;
            case 'U':
                bandwidth = 3000; // default value
                break;
            case 'C':
                bandwidth = 500; // default value
                break;
            }
            SetModulation(sockfd, mode_rig, bandwidth); // Set the SDR++ mode to the radio's one
            memccpy(mode_rig_old, mode_rig, 0, 3);      // previous mode of the radio set to current one
        }

        // sleep(1);            // sleep for 1 second
        nanosleep(&ts, NULL); // sleep for 25 milliseconds
    }                         // end while

    rig_close(my_rig);
    rig_cleanup(my_rig); /* if you care about memory */
    printf("port %s closed ok \n", SERIAL_PORT);

} // end main()
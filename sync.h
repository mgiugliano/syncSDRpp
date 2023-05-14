//
// sync.h - Header file for sync.c
//

#define BUFSIZE 1024 // Buffer size

static volatile int keepRunning = 1; //  CTRL-C handler

void intHandler(int dummy)
{
    keepRunning = 0;
} // end intHandler()

// Function to connect to the SDR++'s own rigctl server, running on localhost
int connect_SDRpp()
{
    int sockfd, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        fprintf(stderr, "ERROR opening socket\n");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(HOSTNAME);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host as %s\n", HOSTNAME);
        // exit(0);
        return (0); // return 0, check on other end and configure pulse input
    }

    /* build the server's Internet address */
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(PORT);

    /* connect: create a connection with the server */
    if (connect(sockfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    {
        fprintf(stderr, "ERROR opening socket\n");
        exit(2);
    }

    return sockfd;
} // end connect_SDRpp()
//------------------------------------------------------------------------------

int send_data(int sockfd, char *buf)
{
    int n = write(sockfd, buf, strlen(buf));
    if (n < 0)
        fprintf(stderr, "ERROR writing to socket");
    return 0;
} // end send_data()
//------------------------------------------------------------------------------

int receive_data(int sockfd, char *buf)
{
    int n = read(sockfd, buf, BUFSIZE);
    if (n < 0)
        fprintf(stderr, "ERROR reading from socket");
    buf[n] = '\0';
    return 0;
} // end receive_data()
//------------------------------------------------------------------------------

// GQRX Protocol
long int GetCurrentFreq(int sockfd)
{
    long int freq = 0;
    char buf[BUFSIZE];
    char *ptr;
    char *token;

    send_data(sockfd, "f\n");
    receive_data(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0)
        return freq;

    token = strtok(buf, "\n");
    freq = strtol(token, &ptr, 10);
    // fprintf (stderr, "\nRIGCTL VFO Freq: [%ld]\n", freq);
    return freq;
} // end GetCurrentFreq()
//------------------------------------------------------------------------------

int SetFreq(int sockfd, long int freq)
{
    char buf[BUFSIZE];

    sprintf(buf, "F %ld\n", freq);
    send_data(sockfd, buf);
    receive_data(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0)
        return 1;

    return 0;
} // end SetFreq()
//------------------------------------------------------------------------------

char *GetModulation(int sockfd)
{
    char buf[BUFSIZE];
    char *ptr;
    char *token;

    send_data(sockfd, "m\n");
    receive_data(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0)
        return "";

    token = strtok(buf, "\n");
    return token;
} // end GetModulation()

//------------------------------------------------------------------------------

int SetModulation(int sockfd, char *mode, int bandwidth)
{
    char buf[BUFSIZE];
    // the bandwidth is now a user/system based configurable variable
    // sprintf(buf, "M FM %d\n", bandwidth);
    sprintf(buf, "M %s %d\n", mode, bandwidth);
    send_data(sockfd, buf);
    receive_data(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0)
        return 1;

    return 0;
} // end SetModulation()
//------------------------------------------------------------------------------

// Function to connect to the radio via hamlib and set the frequency
RIG *connect_rig()
{
    /* handle to rig (nstance) */
    int retcode; /* generic return code from functions */
    RIG *my_rig; /* handle to rig (nstance) */

    my_rig = rig_init(MY_RIG_MODEL);

    if (!my_rig)
    {
        fprintf(stderr, "Unknown rig num: %d\n", MY_RIG_MODEL);
        fprintf(stderr, "Please check riglist.h\n");
        exit(1); /* whoops! something went wrong (mem alloc?) */
    }

    strncpy(my_rig->state.rigport.pathname, SERIAL_PORT, HAMLIB_FILPATHLEN - 1);

    retcode = rig_open(my_rig);
    if (retcode != RIG_OK)
    {
        printf("rig_open: error = %s\n", rigerror(retcode));
        exit(2);
    }

    printf("Port %s opened ok\n", SERIAL_PORT);

    return my_rig;
} // end connect_rigctld()
//------------------------------------------------------------------------------

int SetFreq_rig(RIG *my_rig, long int freq)
{
    int retcode; /* generic return code from functions */

    retcode = rig_set_freq(my_rig, RIG_VFO_CURR, freq);
    if (retcode != RIG_OK)
    {
        printf("rig_set_freq: error = %s\n", rigerror(retcode));
        exit(2);
    }

    return 0;
} // end SetFreq_rig()
//------------------------------------------------------------------------------

long int GetFreq_rig(RIG *my_rig)
{
    int retcode; /* generic return code from functions */
    freq_t freq; /* frequency  */

    retcode = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);
    if (retcode != RIG_OK)
    {
        printf("rig_get_freq: error = %s\n", rigerror(retcode));
        exit(2);
    }

    return freq;
} // end GetFreq_rig()
//------------------------------------------------------------------------------

char *GetModulation_rig(RIG *my_rig)
{
    // rmode (in theory) can be any of the following:
    // RIG_MODE_NONE, RIG_MODE_AM, RIG_MODE_CW, RIG_MODE_USB, RIG_MODE_LSB,
    // RIG_MODE_RTTY, RIG_MODE_FM, RIG_MODE_WFM, RIG_MODE_CWR, RIG_MODE_RTTYR,
    // RIG_MODE_DSB, RIG_MODE_SAM, RIG_MODE_SSB, RIG_MODE_SSTV, RIG_MODE_FAX,
    // RIG_MODE_PKTLSB, RIG_MODE_PKTUSB, RIG_MODE_PKTFM, RIG_MODE_ECSSUSB,
    // RIG_MODE_ECSSLSB, RIG_MODE_FAXS, RIG_MODE_HELL, RIG_MODE_FSK, RIG_MODE_PAC,
    // RIG_MODE_GTOR, RIG_MODE_THROB, RIG_MODE_MFSK, RIG_MODE_AFSK, RIG_MODE_DRM,
    // RIG_MODE_SAL, RIG_MODE_SAT, RIG_MODE_TOR, RIG_MODE_ALE, RIG_MODE_OPDS,
    // RIG_MODE_FLEX, RIG_MODE_MAX

    int retcode;   /* generic return code from functions */
    rmode_t rmode; /* radio mode of operation */
    pbwidth_t width;

    retcode = rig_get_mode(my_rig, RIG_VFO_CURR, &rmode, &width);
    if (retcode != RIG_OK)
    {
        printf("rig_get_mode: error = %s\n", rigerror(retcode));
        exit(2);
    }

    switch (rmode)
    {
    case RIG_MODE_AM:
        // printf("RIG_MODE_AM\n");
        return "AM";
        break;
    case RIG_MODE_FM:
        // printf("RIG_MODE_FM\n");
        return "FM";
        break;
    case RIG_MODE_CW:
        // printf("RIG_MODE_CW\n");
        return "CW";
        break;
    case RIG_MODE_CWR:
        // printf("RIG_MODE_CWR\n");
        return "CW";
        break;
    case RIG_MODE_USB:
        // printf("RIG_MODE_USB\n");
        return "USB";
        break;
    case RIG_MODE_LSB:
        // printf("RIG_MODE_LSB\n");
        return "LSB";
        break;
    }
    return "AM";
} // end GetModulation_rig()

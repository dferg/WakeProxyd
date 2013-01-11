// Standard includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <linux/if_ether.h>
#include <unistd.h>
#include <arpa/inet.h>

// WiringPi library
#include "wiringPi.h"

// Buffer sizing
#define MAC_ADDRESS_SIZE   6
#define MAGIC_PACKET_SIZE  (17*MAC_ADDRESS_SIZE)
#define RECV_BUFFER_SIZE   2048

// GPIO definitions
#define GPIO_IN_POWER_OFF_STATE     22
#define GPIO_OUT_WAKE_SLEEP_STROBE  17

// Timeouts
#define WAIT_FOR_POWER_ON   10   // in seconds

// Path to config file
#define CONFIG_FILE_PATH  "/etc/wake_proxyd.conf"

// Preformed buffer for quick memcmp() comparisons
static unsigned char magic_packet[MAGIC_PACKET_SIZE];

// Returns the power state of the computer (0 = OFF, 1 = ON)
int is_power_off() {
  return digitalRead(GPIO_IN_POWER_OFF_STATE);
}

// Pulses the WAKE/SLEEP pin, i.e. the power switch
//   - used the wake the computer from sleep
//   - could also be used to put the computer to sleep when on
void pulse_wake_sleep_pin() {
  digitalWrite(GPIO_OUT_WAKE_SLEEP_STROBE, HIGH);
  usleep(100 * 1000);  // 100 ms
  digitalWrite(GPIO_OUT_WAKE_SLEEP_STROBE, LOW);
}

// Check the packet for WOL
int match_packet (int buf_size, unsigned char *buf_ptr) {
  // Check that the packet is large enough to be a magic packet
  if (buf_size < MAGIC_PACKET_SIZE) {
    return -1;
  }

  // Verify that the destination Ethernet address is "FF FF FF FF FF FF"
  if (memcmp(buf_ptr, magic_packet, MAC_ADDRESS_SIZE) != 0) {
    return -1;
  }

  // Search the packet for the magic stream: 0xff's repeated 6 times + mac address repeated 16 times
  int i;
  for (i = 0; i <= (buf_size - MAGIC_PACKET_SIZE); i++) {
    if (memcmp(buf_ptr + i, magic_packet, MAGIC_PACKET_SIZE) == 0) {
      return 0;
    }
  }
  return -1;
}

int is_blank_line(const char *line) {
  while(*line) {
    if (!isspace(*line)) return 0;
    line++;
  }
  return 1;
}

#define LINE_MAX 80
int read_config(unsigned char *mac_address) {
  FILE *in;
  char line[LINE_MAX];
  int lineno = 0;
  if ((in = fopen(CONFIG_FILE_PATH, "r")) != NULL) {
    while(fgets(line, LINE_MAX, in) != NULL) {
      lineno++;
      if ((line[0] != '\0') && (line[0] != '#') && !is_blank_line(line)) {
        // Parse the string
        unsigned int h[MAC_ADDRESS_SIZE];
        if (sscanf(line, "%02X:%02X:%02X:%02X:%02X:%02X", &h[0], &h[1], &h[2], &h[3], &h[4], &h[5]) != MAC_ADDRESS_SIZE) {
          printf("In config file %s line %d, bad mac address format (%s)\n", CONFIG_FILE_PATH, lineno, line);
          exit (EXIT_FAILURE);
        }
       
        // Copy the address to the destination buffer (converting int to char in the process)
        int i;
        for(i = 0; i < MAC_ADDRESS_SIZE; i++) {
          mac_address[i] = h[i];
        }
        return 0;
      }
    }
  }
  return 1;
}

int main(int argc, char *argv[])
{
  // Make stdout unbuffered in case we are daemonized
  setvbuf(stdout,NULL,_IONBF,0);

  // Build the magic packet that we will use for comparison when a packet arrives
  //   - able to leverage optimized memcmp() instead of custom compare logic

  // Start with 6 0xff's
  memset(magic_packet, 0xff, MAC_ADDRESS_SIZE);

  // Read the config file and place the result in the compare buffer
  if (read_config(&magic_packet[MAC_ADDRESS_SIZE])) {
    printf("Could not parse the config file %s for a mac address\n", CONFIG_FILE_PATH);
    exit (EXIT_FAILURE);
  }

  // Replicate the mac address 15 more times in the compare buffer
  int i, j;
  for (j=2; j<17; j++) {
    for (i=0; i<MAC_ADDRESS_SIZE;i++) {
      magic_packet[j*MAC_ADDRESS_SIZE + i] = magic_packet[MAC_ADDRESS_SIZE + i];
    }
  }

#if 0
  // Debug compare packets
  for(i = 0; i < 17; i++) {
    printf("Magic %2d: %02x:%02x:%02x:%02x:%02x:%02x\n", i,
      magic_packet[i*MAC_ADDRESS_SIZE + 0], magic_packet[i*MAC_ADDRESS_SIZE + 1], magic_packet[i*MAC_ADDRESS_SIZE + 2], magic_packet[i*MAC_ADDRESS_SIZE + 3], magic_packet[i*MAC_ADDRESS_SIZE + 4], magic_packet[i*MAC_ADDRESS_SIZE + 5]);
  }
#endif

  // Initialize GPIO's
  if (wiringPiSetupGpio () == -1)
  {
    printf("Unable to initialize GPIO mode.\n");
    exit (EXIT_FAILURE) ;
  }

  // Set pin directions
  digitalWrite(GPIO_OUT_WAKE_SLEEP_STROBE, LOW);  // set initially low!
  pinMode(GPIO_OUT_WAKE_SLEEP_STROBE, OUTPUT);
  pinMode(GPIO_IN_POWER_OFF_STATE, INPUT);
  printf("At start, computer is %s\n", is_power_off() ? "OFF" : "ON");

  // Create a socket that listens for all traffic
  //   - we don't need promiscuous mode though since WOL is broadcast
  //   - this is good since it cuts down on the packet processing load
  int sock = socket( AF_PACKET, SOCK_RAW, htons(ETH_P_ALL) );

  // Allocate a buffer for packet reception
  unsigned char *buf_ptr = malloc(RECV_BUFFER_SIZE);

  // Loop here eternally
  while (1)
  {
    // Wait for a packet to arrive
    // TODO: fork this off so that we do not block incoming WOL's when already processing a packet
    //   - technically bad things could happen if we got a flood of WOL's
    size_t buf_size = recvfrom(sock, (void *)buf_ptr, 2048, 0, NULL, NULL);
    if (buf_size < 0)
    {
      printf("recvfrom failed\n");
      exit(EXIT_FAILURE);
    }

    // Check the packet to see if it is a WOL for the proxied computer
    if (match_packet(buf_size, buf_ptr)==0) {
      if (is_power_off()) {
        // Power is off and the WOL is for the proxied computer, so attempt to wake it up
        printf("WOL received from %02x:%02x:%02x:%02x:%02x:%02x; WAKING!\n",
          buf_ptr[MAC_ADDRESS_SIZE + 0], buf_ptr[MAC_ADDRESS_SIZE + 1], buf_ptr[MAC_ADDRESS_SIZE + 2], 
          buf_ptr[MAC_ADDRESS_SIZE + 3], buf_ptr[MAC_ADDRESS_SIZE + 4], buf_ptr[MAC_ADDRESS_SIZE + 5]);

        // Pulse the wake-up pin and give it a second to bring up the power
        pulse_wake_sleep_pin();
        sleep(1);

        // Power should be up, but... wait a bit to see if it does come up
        for(i = 0; (i < WAIT_FOR_POWER_ON) && is_power_off(); i++) {
          printf("Computer still not awake; WAITING!\n");
          sleep(1);
        }

        // Verify that power has come up
        if (is_power_off()) {
          printf("Could not wake computer!\n");
        }
      } else {
        // Ignore any WOL packets if the power is already on
        printf("WOL received from %02x:%02x:%02x:%02x:%02x:%02x; IGNORING! (already awake)\n",
          buf_ptr[MAC_ADDRESS_SIZE + 0], buf_ptr[MAC_ADDRESS_SIZE + 1], buf_ptr[MAC_ADDRESS_SIZE + 2], 
          buf_ptr[MAC_ADDRESS_SIZE + 3], buf_ptr[MAC_ADDRESS_SIZE + 4], buf_ptr[MAC_ADDRESS_SIZE + 5]);
      }
    }
  }
}


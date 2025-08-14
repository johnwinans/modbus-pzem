// NOTE: This was mostly written by Grok.
// No Copyright.  Do with it as you wish!

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <modbus/modbus.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define SERIAL_PORT "/dev/ttyUSB0"
#define BAUD_RATE 9600
#define SLAVE_ADDRESS 1 					// Default PZEM address
#define REG_START_ADDR 0x0000 				// Input registers start
#define NUM_REGISTERS 10 					// Read 10 registers

// delay between readings in usec
#define	PERIOD		(2000000)
//#define	PERIOD		(15000000)

int main() {
    modbus_t *ctx = NULL;
    uint16_t registers[NUM_REGISTERS];
    int rc;


    // Create Modbus context
    ctx = modbus_new_rtu(SERIAL_PORT, BAUD_RATE, 'N', 8, 1);
    if (ctx == NULL) {
        fprintf(stderr, "Error: Unable to create Modbus context: %s\n", modbus_strerror(errno));
        return 1;
    }

    // Set slave address
    if (modbus_set_slave(ctx, SLAVE_ADDRESS) != 0) {
        fprintf(stderr, "Error: Failed to set slave address: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return 1;
    }

    // Connect to the device
    if (modbus_connect(ctx) != 0) {
        fprintf(stderr, "Error: Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return 1;
    }

    // Set response timeout (2 sec)
    modbus_set_response_timeout(ctx, 2, 0);

    printf("time,volt,amp,watt,kwh,freq,pwrfct\n");

    while (1) {
        // Read input registers
        rc = modbus_read_input_registers(ctx, REG_START_ADDR, NUM_REGISTERS, registers);
        if (rc == -1) {
            fprintf(stderr, "Error: Failed to read registers: %s\n", modbus_strerror(errno));
			// piece of crap seems to seize up.  repoen it
    		modbus_close(ctx);
    		modbus_connect(ctx);
            usleep(PERIOD);
            continue;
        }

        // Parse data (based on PZEM-004T V3.0 register map)
        float voltage = registers[0] / 10.0;                    				// 0: V
        float current = ((uint32_t)registers[2] << 16 | registers[1]) / 1000.0; // 1-2: A
        float power = ((uint32_t)registers[4] << 16 | registers[3]) / 10.0; 	// 3-4: W
        float energy = ((uint32_t)registers[6] << 16 | registers[5]) / 1000.0; 	// 5-6: kWh
        float frequency = registers[7] / 10.0;                  				// 7: Hz
        float power_factor = registers[8] / 100.0;              				// 8: Unitless

#if 1
        time_t rawtime;
        struct tm *info;
        char buffer[80];

        // Get the current time
        time(&rawtime);
        info = localtime(&rawtime);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", info);
        printf("%s,%.1f,%.3f,%.1f,%.3f,%.1f,%.2f\n",
			buffer,
			voltage,
			current,
			power,
			energy,
			frequency,
			power_factor );
		fflush( stdout );
#else
        printf("PZEM-004T V3.0,");
        printf("Voltage: %.1f V\n", voltage);
        printf("Current: %.3f A\n", current);
        printf("Power: %.1f W\n", power);
        printf("Energy: %.3f kWh\n", energy);
        printf("Frequency: %.1f Hz\n", frequency);
        printf("Power Factor: %.2f\n", power_factor);
#endif

        // Wait before next reading
        usleep(PERIOD);
    }

    // Cleanup (unreachable in this loop)
    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}

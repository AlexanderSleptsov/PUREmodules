/*
	Code for Time of Flight Module
	Created by Pure Engineering.
*/
#ifndef VL53L0_H
#define VL53L0_H

#include "Arduino.h"
#include "Wire.h"

class VL53L0
{
	public:


		typedef enum {
			
			DEVICE_ADDRESS = 0x29

		} UnitAddress;

		VL53L0();
		void write_byte(uint8_t addr, uint8_t subAddress, uint8_t data);
		int read_byte(uint8_t addr, uint8_t subAddress);
    	int read_2bytes(uint8_t addr, uint8_t subAddress);

    };
#endif


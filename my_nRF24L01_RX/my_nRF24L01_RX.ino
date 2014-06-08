/********************************************************************************
*	RC Boat receiver using nRF24L01+											*
*																				*
*																				*
*	The following links proved helpful in using the nrf24L01					*
*	NRF24L01 With Arduinios SPI Library http://www.elecfreaks.com/480.html		*
*	NRF24L01 Module Demo For Arduino http://www.elecfreaks.com/203.html			*
*																				*
*																				*
*	CS		- Arduino digital pin 8,  nRF24L01 pin 3 (CE)						*
*	CSN 	- Arduino digital pin 9,  nRF24L01 pin 4 (CSN)						*
*	MOSI 	- Arduino digital pin 11, nRF24L01 pin 6 (MOSI) 					*
*	MISO 	- Arduino digital pin 12, nRF24L01 pin 7 (MISO)						*
*	CLK 	- Arduino digital pin 13, nRF24L01 pin 5 (SCK) 						*
*																				*
/*******************************************************************************/

#include <SPI.h>
#include "nRF24L01.h"

#define TX_ADR_WIDTH	5
#define TX_PLOAD_WIDTH	9

#define CE				8
#define CSN				9
#define IRQ				10
#define MOSI                            11
#define MISO                            12

int EN_A  = 5;      // to 1,2EN of L293D
int EN_B  = 6;     // to 3,4EN of L293D
int IN_A1 = 2;      // to 1A of L293D
int IN_A2 = 3;      // to 2A of L293D
int IN_B1 = 4;      // to 3A of L293D
int IN_B2 = 7;      // to 4A of L293D

int speed = 100;
boolean dir = 1;  // 1 (true) indicates forward, 0 indicates reverse

// Define a static TX address
unsigned char TX_ADDRESS[TX_ADR_WIDTH]  = { 0x34, 0x43, 0x10, 0x10, 0x01 };

unsigned char rx_buf[TX_PLOAD_WIDTH] = { 0 };	// initialize value
unsigned char tx_buf[TX_PLOAD_WIDTH] = { 0 };

int x_val = 0;
int y_val = 0;
int left_speed = 0;
int right_speed = 0;

unsigned long time = 0;
//**************************************************
void setup() 
{
	Serial.begin( 9600 );
	init_motors();
	pinMode( CE,  OUTPUT );
	pinMode( CSN, OUTPUT );
	pinMode( IRQ, INPUT );
    pinMode( MOSI, OUTPUT );
    pinMode( MISO, INPUT );
	SPI.begin();
	delay( 50 );
	init_io();		// Initialize IO port	
	unsigned char status = SPI_Read( STATUS );
	Serial.println( "*** Get Status ***" );
	Serial.print( "status = " );    
	Serial.println( status, HEX );			// read the modes status register, 
											// the default value should be 'E'
	Serial.println( "******************" );
	RX_Mode();			// set RX mode
}

//**************************************************
void loop() 
{
	int k = 0;
	int i = 0, j = 0, fail = 0, found = 0;
	int lim = TX_PLOAD_WIDTH;
	unsigned char value[lim];

	
	for( i = 0; i < lim; i++ ) {
		rx_buf[i] = 0;
	}
	
	unsigned char status = SPI_Read( STATUS );			// read register STATUS's value
	if( status & RX_DR )								// if receive data ready (TX_DS) interrupt
	{
		SPI_Read_Buf( R_RX_PAYLOAD, rx_buf, TX_PLOAD_WIDTH );	// read playload to rx_buf
		SPI_RW_Reg( FLUSH_RX, 0 );								// clear RX_FIFO

		for ( i = 0; i < lim ; i++ )  value[i] = 0;
		
		i = 0;      j = 0;
		while( (rx_buf[i++] != '/') && !fail ) {
			if ( i > lim ) { fail = 1; break; }
			if ( rx_buf[i] == '/' )  found = 1;
		}
		
		if ( found == 1 && !fail ) {
			while( (rx_buf[i] != '*') && !fail ) {
				if ( i > lim ) { fail = 1; break; }
				value[j++] = rx_buf[i++]; 
				if ( rx_buf[i] == '*' )  found = 2;
			}
        
			if ( found == 2 && !fail ) {
				x_val = value[0];
				y_val = value[1];
				
				time = millis();
				
				if ( y_val < 130 || y_val > 140 ) {
					if ( y_val < 130 ) {
						y_val = map( y_val, 130, 0, 0, 255 );
						dir = 0;
					}
					else if ( y_val > 140 ) {
						y_val = map( y_val, 140, 255, 0, 255 );
						dir = 1;
					}
					
					left_speed = y_val;
					right_speed = y_val;
					
					if ( x_val < 120 || x_val > 130 ) {
						if ( x_val < 120 ) {	// turn left
							x_val = map( x_val, 120, 0, 0, y_val );
							left_speed -= x_val;
						}
						else if ( x_val > 130 ) {	// turn right
							x_val = map( x_val, 130, 255, 0, y_val );
							right_speed -= x_val;
						}
					}
				} else {
					left_speed = 0;
					right_speed = 0;
				}
				
				set_left_motor(left_speed, dir);
                set_right_motor(right_speed, dir);                                
				
                Serial.print("speed: ");  
				Serial.print(left_speed);
				Serial.print(", ");  
				Serial.print(right_speed);
				Serial.print(", direction:");
				Serial.println(dir);
				  
				x_val = 0; 
				y_val = 0; 
				left_speed = right_speed = 0;
			}
		}
 
		if ( fail ) {
            Serial.println("FAIL");
            stop_motors();
        }
		
/*
		for( i=0; i < strlen((char *)rx_buf); i++ )
		{
			//Serial.print( rx_buf[i], HEX );	// print rx_buf as HEX
			Serial.write( rx_buf[i] );			// print rx_buf as Ascii Characters
		}
		//Serial.println( " " );
*/
	}
	
	SPI_RW_Reg( W_REGISTER+STATUS, status );	// clear RX_DR or TX_DS or MAX_RT interrupt flag
	
	if ( millis() - time > 2000 ) {
		// stop
        stop_motors();
		//Serial.println("timeout");
	}

//	delay( 1000 );
}

//**************************************************
void init_io (void )
{
	digitalWrite( IRQ, 0 );
	digitalWrite( CE, 0 );		// chip enable
	digitalWrite( CSN, 1 );		// SPI disable	
}

//**************************************************
unsigned char SPI_Read( unsigned char reg )
{
	unsigned char reg_val;

	digitalWrite( CSN, 0 );		// CSN low, initialize SPI communication...
	SPI_RW( reg );				// Select register to read from..
	reg_val = SPI_RW( 0 );		// ..then read register value
	digitalWrite( CSN, 1 );		// CSN high, terminate SPI communication

	return( reg_val );			// return register value
}

//**************************************************
unsigned char SPI_RW( unsigned char Byte )
{
	return SPI.transfer( Byte );
}

//**************************************************
unsigned char SPI_Read_Buf( unsigned char reg, unsigned char *pBuf, unsigned char bytes )
{
	unsigned char status, i;

	digitalWrite( CSN, 0 );		// Set CSN low, init SPI tranaction
	status = SPI_RW( reg );		// Select register to write to and read status unsigned char

	for( i = 0; i < bytes; i++ )
	{
		pBuf[i] = SPI_RW( 0 );		// Perform SPI_RW to read unsigned char from nRF24L01
	}

	digitalWrite( CSN, 1 );		// Set CSN high again

	return( status );				// return nRF24L01 status unsigned char
}

//**************************************************
unsigned char SPI_Write_Buf( unsigned char reg, unsigned char *pBuf, unsigned char bytes )
{
	unsigned char status, i;

	digitalWrite( CSN, 0 );		// Set CSN low, init SPI tranaction
	status = SPI_RW( reg );		// Select register to write to and read status unsigned char
	for( i = 0; i < bytes; i++ )	// then write all unsigned char in buffer(*pBuf)
	{
		SPI_RW( *pBuf++ );
	}
	digitalWrite( CSN, 1 );			// Set CSN high again
	return( status );				// return nRF24L01 status unsigned char
}

//**************************************************
unsigned char SPI_RW_Reg( unsigned char reg, unsigned char value )
{
	unsigned char status;

	digitalWrite( CSN, 0 );			// CSN low, init SPI transaction
	status = SPI_RW( reg );			// select register
	SPI_RW( value );				// ..and write value to it..
	digitalWrite( CSN, 1 );			// CSN high again

	return( status );				// return nRF24L01 status unsigned char
}

//**************************************************
void RX_Mode( void )
{
	digitalWrite( CE, 0 );

	SPI_Write_Buf( W_REGISTER + RX_ADDR_P0, TX_ADDRESS, TX_ADR_WIDTH );	// use same address as transmitting device

	SPI_RW_Reg( W_REGISTER + EN_AA, 0x01 );				// Enable auto acknowledgement data pipe 0
	SPI_RW_Reg( W_REGISTER + EN_RXADDR, 0x01 );			// Enable data pipe 0
	SPI_RW_Reg( W_REGISTER + RF_CH, 40 );				// Select RF channel 40
	SPI_RW_Reg( W_REGISTER + RX_PW_P0, TX_PLOAD_WIDTH );// Select same RX payload width as TX Payload width  
	SPI_RW_Reg( W_REGISTER + RF_SETUP, 0x07 );			// TX_PWR:0dBm, Datarate:2Mbps, LNA:HCURR
	SPI_RW_Reg( W_REGISTER + CONFIG, 0x0f );			// POWER UP, enable 2-byte CRC & PRX as RX/TX control
														// RX_DR, TX_DS & MAX_RT interrupts enabled
	digitalWrite( CE, 1 );
}

void init_motors() {
  // set output modes
  pinMode(IN_A1, OUTPUT);
  pinMode(IN_A2, OUTPUT);
  pinMode(IN_B1, OUTPUT);
  pinMode(IN_B2, OUTPUT);
  pinMode(EN_A, OUTPUT);
  pinMode(EN_B, OUTPUT);
  
  // initialize ports to safely turn off the motors
  stop_motors();
}

void stop_motors(){
  set_left_motor( 0, dir);
  set_right_motor( 0, dir);
}

void set_left_motor(int speed, boolean dir) {
  speed = constrain(speed, 0, 255);
  analogWrite(EN_A, speed);     // PWM on enable lines
  digitalWrite(IN_A1, dir);
  digitalWrite(IN_A2, ! dir);
}

void set_right_motor(int speed, boolean dir) {
  speed = constrain(speed, 0, 255);  
  analogWrite(EN_B, speed);
  digitalWrite(IN_B1, dir);
  digitalWrite(IN_B2, ! dir);
}

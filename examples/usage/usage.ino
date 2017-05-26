// Example usage for TftpServer library by Micah L. Abelson.

#include "TftpServer.h"
#include "SdFat.h"
TftpServer tftpServer;

// Primary SPI with DMA
// SCK => A3, MISO => A4, MOSI => A5, SS => A2 (default)
//const uint8_t SD_CS_PIN = A2;
//SdFat sd;

// Secondary SPI with DMA
// SCK => D4, MISO => D3, MOSI => D2, SS => D5
const uint8_t SD_CS_PIN = D5;
SdFat sd(1);

void setup() {

	// initialize file system.
	if (!sd.begin (SD_CS_PIN, SPI_FULL_SPEED)) {
		sd.initErrorPrint();
	}
	
	// Let's put some data on your card
	std::string fileName = "DATATEST.TXT";

	if (!sd.exists (fileName.c_str())) {

		File myFile;

		// open the file for write
		if (!myFile.open (fileName.c_str(), O_RDWR | O_CREAT)) {
			
			sd.errorHalt ("opening mytest.txt for write failed");
		}

		for (int i = 0; i < 1000; ++i) {
			myFile.println ("testing 1, 2, 3.");
		}

		myFile.printlnf ("fileSize: %d", myFile.fileSize());

		// close the file:
		myFile.close();
	}

	// start the TFTP server and pass the SdFat file system
	tftpServer.begin(&sd);
}

void loop() {
  
  	if (tftpServer.checkForPacket()) {
		
		tftpServer.processRequest();
	}
}
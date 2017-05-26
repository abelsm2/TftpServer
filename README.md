# TftpServer

A Particle library for TftpServer

This is a work in progress.  The library should be functional for TFTP GET requests but the examples do not work yet.

## Usage

Connect an SD card on SPI or SPI1, add the TftpServer library to your project and follow this example:

NOTE:  comment out or uncomment the appropriate SdFat instance depending on whether you are using SPI or SPI1

```
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
```

Using a TFTP client, connect to the IP address assigned to your device and use the get command

See the [examples](examples) folder for more details.

## Documentation

This is a VERY minimal implementation of a TFTP Server for the Particle
environment (Tested on the P0 and P1).  The initial version will only support
the GET methods and does not implement any methods for PUT.  This made the most
sense during development since it seemed like there was more need to get files
off of an SD card than there were to put them on.

The server is opened on port 69 by default.  The clientConnected method should
be run in loop as often as possible to improve responsiveness and to avoid having
the client send duplicate requests due to retransmission timeouts.  Once TRUE is
returned by clientConnected() it means a UDP packet was received on port 69 and
you can call handleClientRequest() to do the rest.  The function will block until
the client request is taken care of and then control will pass back to the calling
function.

In order to have files to send, this library relies on the SdFat-Particle
library.  A pointer to an SdFat object is passed as part of begin() so the
TFTP server will have access to the SD card without having to create it's own
instance of the file system.  This also makes the library agnostic as to which
SPI instance is used by SdFat (SPI vs. SPI1).

It would be best to ensure that no files are open prior to passing control off to
the TFTP server.  Since files are opened and closed as part of the GET/PUT process,
it could potentially cause file corruption if there was already a file open.

The attempt has been made to stick as close to the TFTP protocol as possible.
Timeouts were implemented as best I could figure out because the
specification doesn't cover timeouts and retransmission explicitly.  Also, to make
like simpler with the limited number of sockets available, the transfer ID for the
server is kept at 69 rather than choosing a new random port number.

The specification can be located at: https://tools.ietf.org/html/rfc1350

@note A buffer size of 516 bytes is allocated for TFTP transfers
@note Library developed using ARM GCC 5.3

From RFC 1350:

TFTP Formats

  Type   Op #     Format without header

         2 bytes    string   1 byte     string   1 byte
         -----------------------------------------------
  RRQ/  | 01/02 |  Filename  |   0  |    Mode    |   0  |
  WRQ    -----------------------------------------------
         2 bytes    2 bytes       n bytes
         ---------------------------------
  DATA  | 03    |   Block #  |    Data    |
         ---------------------------------
         2 bytes    2 bytes
         -------------------
  ACK   | 04    |   Block #  |
         --------------------
         2 bytes  2 bytes        string    1 byte
         ----------------------------------------
  ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |
         ----------------------------------------

## LICENSE

Licensed under the MIT License

Copyright (c) 2017 Micah L. Abelson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

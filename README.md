# TftpServer

A Particle library for TftpServer

This is a work in progress.  The library should be functional but the examples do not work yet and you might need to verify the include to SdFat.

## Usage

Connect an SD card on SPI or SPI1, add the TftpServer library to your project and follow the usage example in the [examples](examples) folder for more details.

After flashing the example to your device, use a TFTP client to connect to the IP address assigned to your Particle device.  Once there you can use the GET or PUT commands with either NETASCII or OCTET methods.

For example, using the windows TFTP client to download the file created in the example using NETASCII, the command would be:
```
>tftp 192.168.1.XX GET DATATEST.TXT DATATEST.TXT
```
And to download the same file using the OCTET (binary) method, the command would be:
```
>tftp -i 192.168.1.XX GET DATATEST.TXT DATATEST.TXT
```
You can also upload any files from your computer to the SD card attached to the particle device.  Unless you are uploading text files you should be sure to transfer them using the OCTET method.  For example, to upload a PDF file called `myfile` the command would be:
```
>tftp -i 192.168.1.XX PUT myfile.pdf myfile.pdf
```

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

<b>Note:</b> A buffer size of 516 bytes is allocated for TFTP transfers
<b>Note:</b> Library developed using ARM GCC 5.3

From RFC 1350:

TFTP Formats
<pre>
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
</pre>
## Future Work
While the library will accept write requests in NETASCII format, it does not
currently do anything to the received library.  This does not strictly conform
to the TFTP standard but was implemented this way since it was assumed the same
operating system that is requesting a write will also be the same system that 
will request a read.  In a more general application this is probably a bad
assumption but in the use case of a Particle device, this might not be so bad.

Pull requests to address this are certainly welcome if a condition is found where 
this behavior is not desired.
		 
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

/**
 * TftpServer library by Micah L. Abelson
 *
 * @name TftpServer.cpp
 * @author Micah Abelson
 * @date May 20, 2017
 *
 * MIT License
 *
 * Copyright (c) 2017 Micah L. Abelson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <TftpServer.h>

const uint32_t INITIAL_TIMEOUT = 50; // milliseconds
const uint32_t TIMEOUT_MIN = 50; 	 // milliseconds
const uint32_t TIMEOUT_MAX = 10000;  // milliseconds
const uint8_t MAX_RETRANSMISSIONS = 8;

// TFTP data packets use 512 bytes of data and 4 bytes of header
const size_t UDP_BUFFER_SIZE = 516;
uint8_t udpBuffer [UDP_BUFFER_SIZE];

// Start your engines!
bool TftpServer::begin (SdFat* sd, bool serialDebug, uint16_t portNumber) {

	m_localPort = portNumber;

	// ensure that WiFi is running before trying to start UDP
	if (!WiFi.ready()) {

		WiFi.connect();

		waitUntil (WiFi.ready);
	}

	// start UDP at the specified port number
	m_tftp.begin (m_localPort);

	// pointers to the file system from main application
	m_sd = sd;

	// Send errors and timeout messages to Serial
	m_serialDebug = serialDebug;

	return true;
}

// shut it down
void TftpServer::stop() {

	m_tftp.stop();

	if (m_file.isOpen()) m_file.close();
}

bool TftpServer::checkForPacket() {

	// check for a packet
	m_bufferCount = m_tftp.receivePacket (udpBuffer, UDP_BUFFER_SIZE);

	// the buffer has data in it so we have a packet!
	if (m_bufferCount > 0) {

		// get information on the packet sender
		m_remoteIpAddress = m_tftp.remoteIP();
		m_remotePort = m_tftp.remotePort();

		return true;
	}

	// There was a UDP error, restart UDP
	else if (m_bufferCount < 0) {

		if (m_serialDebug) Serial.printlnf ("***ERROR: TFTP receivePacket error %d", m_bufferCount);

		// reinitialize UDP to clear the error
		m_tftp.begin (m_localPort);

	}

	return false;
}

// Take care of all your client's needs!
void TftpServer::processRequest() {

	m_timeout = INITIAL_TIMEOUT;
	m_rtt = INITIAL_TIMEOUT;

	// start from the beginning of the buffer
	m_bufferPosition = 0;

	if (m_serialDebug) Serial.print ("Handling Incoming TFTP Request... ");

	// 1st 2 bytes of incoming packet are the opcode
	m_opCode = readWord();

	/// Read Request
	if (m_opCode == RRQ) {

		handleReadRequest();
	}

	/// Write Request
	else if (m_opCode == WRQ) {

		handleWriteRequest();
	}

	/// Send error for illegal TFTP operation (only RRQ and WRQ are valid for initial request)
	else {

		if (m_serialDebug) Serial.println ("***ERROR: Initial Request is not RRQ or WRQ!");

		// Send error message to originator
		sendError (ILLEGAL_OPERATION, m_errorIllegalOperation, "***ERROR: Initial Request is not RRQ or WRQ!");
	}

	if (m_serialDebug)
		Serial.printlnf ("Timed out on %u packets out of %d total blocks (%.3f %%)",
			m_droppedPacket, m_blockNumber,	static_cast<float> (m_droppedPacket) / static_cast<float>(m_blockNumber) * 100.0);
}

// adaptive timeout
void TftpServer::updateTimeout() {

	// update the RTT based on network conditions
	m_rtt = m_rtt * 0.9 + (m_rttCalcFinish - m_rttCalcStart) * 0.1;

	// add some head room to the current average for some allowance in varying conditions
	m_timeout = 2 * m_rtt;

	// constraining it on the low end helped with short spikes in faster networks.
	m_timeout = constrain (m_timeout, TIMEOUT_MIN, TIMEOUT_MAX);
}

// WRQ
void TftpServer::handleWriteRequest() {

	if (m_serialDebug) Serial.println("Write Request!");

	// the transfer ID for the remote client is the same as their port number
	m_transferId = m_remotePort;

	// initialize variables
	bool transferComplete = false;

	// let's track dropped packets if we are debugging
	if (m_serialDebug) m_droppedPacket = 0;

	// Read the file name requested
	readText (m_fileName);

	// Read the desired transfer mode (OCTET or NETASCII)
	readText (m_transferMode);

	// convert transfer mode to all caps
	for (uint8_t i = 0; i < 10; ++i) {
		m_transferMode[i] = toupper (m_transferMode[i]);
	}

	// make sure the file does not exist
	if (!m_sd->exists (m_fileName.c_str())) {

		// Open a file with the desired filename
		m_file.open(m_fileName.c_str(), O_CREAT | O_WRITE);

		if (!m_file.isOpen()) {

			// Send error message as an ACK that there was an issue
			sendError (ACCESS_VIOLATION, m_errorAccessViolation, "TFTP file create error (SD Error)");

			// close the file
			m_file.close();

			// return
			return;
		}
	}

	else {

		// Send error message as an ACK that file already exists
		sendError (FILE_EXISTS, m_errorFileAlreadyExists, "***ERROR: File Already Exists!");

		return;
	}

	// send an ACK that the write request is accepted
	sendAck(0);

	// 1st data packet should be block 1
	m_blockNumber = 1;

	while (!transferComplete) {

		// check for a data packet
		if (checkForPacket()) {

			// start from the beginning of the buffer
			m_bufferPosition = 0;

			// verify the message came from someone we expect
			if (m_remotePort != m_tftp.remotePort()) {

				// Send error message to the unknown sender that this transfer ID is invalid
				// don't kill the connection for this type of error
				sendError (UNKNOWN_ID, m_errorUnknownTransferId, "***ERROR: Unknown Transfer ID",
						m_tftp.remoteIP(), m_tftp.remotePort());
			}

			else {

				// 1st 2 bytes of incoming packet are the opcode
				m_opCode = readWord();

				// if this is a DATA block then get the block number and write to SD
				if (m_opCode == DATA) {

					// make sure the block number matches
					if (m_blockNumber == readWord()) {

						// check to see if this is the last data packet
						if (m_bufferCount >= 0 && m_bufferCount < 512) {

							transferComplete = true;
						}

						// write the file as binary if OCTET mode was requested and do nothing
						// if NETASCII is selected (just accept the file as if it was binary)
						if (m_transferMode.compare ("OCTET") == 0 ||
								m_transferMode.compare ("NETASCII") == 0) {

							// write the file starting from the 5th byte in the buffer
							if (m_file.write (&udpBuffer[4], m_bufferCount) < 0) {

								// Send error message as an ACK that there was an issue
								sendError (ACCESS_VIOLATION, m_errorAccessViolation, "TFTP file write error (SD Error)");

								// close the file
								m_file.close();

								// return
								return;
							}

							else {

								// force data to be written to SD
								m_file.sync();

								// ACK the block just written
								sendAck (m_blockNumber++);
							}
						}

						else {

							// Send error message as an ACK
							sendError (ILLEGAL_OPERATION, m_errorIllegalOperation, "***ERROR: Illegal TFTP Transfer Mode!");
						}
					}

					else {

						// Ignore this packet.  The block number doesn't match so it might be a duplicate packet
					}
				}

				// this is not a DATA packet and one was expected so ignore it
				else {

					if (m_serialDebug) Serial.println ("***ERROR: Received something other than DATA");
				}
			}
		}
	}

	// close the file
	m_file.close();
}

// RRQ
void TftpServer::handleReadRequest() {

	if (m_serialDebug) Serial.println("Read Request!");

	// the transfer ID for the remote client is the same as their port number
	m_transferId = m_remotePort;

	// let's track dropped packets if we are debugging
	if (m_serialDebug) m_droppedPacket = 0;

	// Read the file name requested
	readText (m_fileName);

	// Read the desired transfer mode (OCTET or NETASCII)
	readText (m_transferMode);

	// convert transfer mode to all caps
	for (uint8_t i = 0; i < 10; ++i) {
		m_transferMode[i] = toupper (m_transferMode[i]);
	}

	// check that the file exists
	if (m_sd->exists (m_fileName.c_str())) {

		// open the requested file
		m_file = m_sd->open (m_fileName.c_str(), O_READ);

		if (!m_file.isOpen()) {

			// Send error message as an ACK that there was an issue
			sendError (ACCESS_VIOLATION, m_errorAccessViolation, "TFTP file open error (SD Error)");

			// close the file
			m_file.close();

			// return
			return;
		}
	}

	// Error: file does not exist
	else {

		// Send error message as an ACK
		sendError (FILE_NOT_FOUND, m_errorFileNotFound, "***ERROR: File Not Found!");
	}

	// initialize variables
	m_blockNumber = 0;
	bool sendData = true;
	bool receivedFinalAck = false;
	bool transferComplete = false;
	bool ignoreTime = false;

	// track if we found a lone \n and didn't have enough room to insert \r
	bool startNextPacketWithNewLine = false;

	// track if we found a lone \r and didn't have enough room to insert \0
	bool startNextPacketWithNull = false;

	// track if we found a \r\n and should not insert \r
	bool dontInsertCarriageReturn = false;

	// loop until the entire file is sent
	while (!transferComplete || !receivedFinalAck) {

		// only do these things if we are ready
		if (sendData) {

			m_blockSize = 0;

			// Send the file as binary if OCTET mode was requested
			if (m_transferMode.compare ("OCTET") == 0) {

				// read the next 512 byte block from the file (this is a binary read)
				m_blockSize = m_file.read (&udpBuffer[4], 512);

				// verify there was a good read
				if (m_blockSize < 0) {

					// Send error message as an ACK that there was an issue
					sendError (ACCESS_VIOLATION, m_errorAccessViolation, "TFTP File Read Error (SD Error)");

					// close the file
					m_file.close();

					// return
					return;
				}
			}

			// Convert the file to NVT ASCII if NETASCII mode was requested
			else if (m_transferMode.compare ("NETASCII") == 0) {

				if (startNextPacketWithNewLine) {

					// put the \n from the \r\n combo from the previous packet at the start of the packet
					udpBuffer [4 + m_blockSize++] = '\n';

					// reset for the next packet
					startNextPacketWithNewLine = false;
				}

				else if (startNextPacketWithNull) {

					// put the \n from the \r\n combo from the previous line at the start of the packet
					udpBuffer [4 + m_blockSize++] = '\0';

					// reset for the next packet
					startNextPacketWithNull = false;
				}

				// fill up the buffer with 512 bytes of data or stop at EOF
				while (m_blockSize < 512 && m_file.peek() != EOF) {

					// grab the next character
					uint8_t c = m_file.read();

					if (c < 0) {

						// Send error message as an ACK that there was an issue
						sendError (ACCESS_VIOLATION, m_errorAccessViolation, "TFTP File Read Error (SD Error)");

						// close the file
						m_file.close();

						// return
						return;
					}

					// check for a \r\n sequence so we don't insert an extra \r
					if (c == '\r' && m_file.peek() == '\n') {

						dontInsertCarriageReturn = true;

						// put the \r character into the buffer
						udpBuffer [4 + m_blockSize++] = c;
					}

					// replace \n with \r\n as long as it's not already part of \r\n
					else if (c == '\n' && !dontInsertCarriageReturn) {

						// insert the \r
						udpBuffer [4 + m_blockSize++] = '\r';

						// check to see if we reached the end of the buffer
						if (m_blockSize == 512) {

							// set so the start of the next packet buffer will be the \n
							startNextPacketWithNewLine = true;

							// get out of the loop for this packet
							break;
						}

						else {

							// we have space in the buffer so write the \n
							udpBuffer [4 + m_blockSize++] = '\n';
						}
					}

					// replace lone \r with \r\0
					else if (c == '\r' && m_file.peek() != '\n') {

						// write the \r
						udpBuffer [4 + m_blockSize++] = '\r';

						// check to see if we reached the end of the buffer
						if (m_blockSize == 512) {

							// set so the start of the next buffer will be the \0
							startNextPacketWithNull = true;

							// get out of the loop for this packet
							break;
						}

						else {

							// we have space in the buffer so write the \0
							udpBuffer [4 + m_blockSize++] = '\0';
						}
					}

					else {

						// put the next character into the buffer
						udpBuffer [4 + m_blockSize++] = c;
					}
				}
			}

			// The transfer mode doesn't match anything so respond with an error.
			else {

				// Send error message as an ACK
				sendError (ILLEGAL_OPERATION, m_errorIllegalOperation, "***ERROR: Illegal TFTP Transfer Mode!");
			}

			// check for EOF
			if (m_blockSize >= 0 && m_blockSize < 512) {

				// Reached end of file
				transferComplete = true;
			}

			// increment the file block number
			m_blockNumber++;

			// send the data packet
			sendDataPacket ();

			// start the clock for calculating round trip time
			m_rttCalcStart = millis();
			m_resendStart = m_rttCalcStart;

			// reset for the new data packet just sent out
			m_numberOfRetransmissions = 0;

			// don't proceed with the next block until valid ACK
			sendData = false;

			// reset flag so we don't ignore time
			ignoreTime = false;

		}

		// check for a new UDP message (looking for an ACK)
		else if (checkForPacket()) {

			// start from the beginning of the buffer
			m_bufferPosition = 0;

			uint16_t ackBlockNumber = 0;

			// verify the message came from someone we expect
			if (m_remotePort != m_tftp.remotePort()) {

				// Send error message to the unknown sender that this transfer ID is invalid
				// don't kill the connection for this type of error
				sendError (UNKNOWN_ID, m_errorUnknownTransferId, "***ERROR: Unknown Transfer ID",
						m_tftp.remoteIP(), m_tftp.remotePort());
			}

			else {

				// 1st 2 bytes of incoming packet are the opcode
				m_opCode = readWord();

				// if this is an ACK then get the block number
				if (m_opCode == ACK) {

					// ACK block number is the next 2 bytes
					ackBlockNumber = readWord();

					// check to see if we got an ACK for the correct block
					// Does not allow previous blocks to be re-sent since
					// m_blockNumber is incremented as soon as data is buffered,
					// so it should prevent Sorcerer's Apprentice Syndrome.
					if (ackBlockNumber == m_blockNumber) {

						// stop the RTT clock because we got an ACK (only if it's the 1st one)
						if (!ignoreTime) {

							m_rttCalcFinish = millis();

							// keep updating timeout based on current network conditions
							updateTimeout();
						}

						// This is the ACK we are looking for... send the next block
						sendData = true;

						if (transferComplete) {

							// this is the ACK for the EOF!
							receivedFinalAck = true;
						}
					}
				}

				// this is not an ACK and one was expected so ignore it
				else {

					if (m_serialDebug) Serial.println ("***ERROR: Received something other than ACK");

				}
			}
		}

		// check to see if we should re-send the last data packet
		else if ((millis() - m_resendStart) > m_timeout) {

			if (m_serialDebug) Serial.printlnf ("***ERROR: Timeout (%u ms).  Re-sending Data packet %u\t RTT: %f",
					m_timeout, m_blockNumber, m_rtt);

			// send the same data packet again
			sendDataPacket ();

			// reset the timer
			m_resendStart = millis();

			// ignore time data for resent packets
			ignoreTime = true;

			// increase the transmission count for the exponential back-off
			m_numberOfRetransmissions++;

			// increase the timeout exponentially with each retransmission
			m_timeout *= 2;

			// track dropped packets only for debug output
			if (m_serialDebug) m_droppedPacket++;

			m_timeout = constrain (m_timeout, TIMEOUT_MIN, TIMEOUT_MAX);

			// check to see if we should give up
			if (m_numberOfRetransmissions >= MAX_RETRANSMISSIONS) {

				// tell the client we are not getting along
				sendError (NOT_DEFINED, m_errorTimeoutOnSend, "***ERROR: Timeout on Send");

				// get us out of here.
				sendData = false;
				receivedFinalAck = true;
				transferComplete = true;
			}
		}
	}

	// close the file
	m_file.close();
}

// Send a data packet
bool TftpServer::sendDataPacket () {

	uint16_t opCode = DATA;

	// First 2 bytes of data message are opcode
	udpBuffer[0] = static_cast <uint8_t> (opCode >> 8);
	udpBuffer[1] = static_cast <uint8_t> (opCode);

	// Next 2 bytes of ACK message are the block number
	udpBuffer[2] = (static_cast <uint8_t> (m_blockNumber >> 8));
	udpBuffer[3] = (static_cast <uint8_t> (m_blockNumber));

	// send the buffer and check for send errors
	if (m_tftp.sendPacket (udpBuffer, 4 + m_blockSize, m_remoteIpAddress, m_remotePort) < 0) {

		if (m_serialDebug) Serial.println ("***ERROR: Send Failure on sendDataPacket!");

		return false;
	}

	return true;
}

// Send an ACK to the client
bool TftpServer::sendAck (uint16_t blockNumber) {

	uint16_t opCode = ACK;

	// First 2 bytes of ACK message are opcode
	udpBuffer[0] = static_cast <uint8_t> (opCode >> 8);
	udpBuffer[1] = static_cast <uint8_t> (opCode);

	// Last 2 bytes of ACK message are the block number
	udpBuffer[2] = (static_cast <uint8_t> (blockNumber >> 8));
	udpBuffer[3] = (static_cast <uint8_t> (blockNumber));

	// send the buffer and check for send errors
	if (m_tftp.sendPacket (udpBuffer, 4, m_remoteIpAddress, m_remotePort) < 0) {

		if (m_serialDebug) Serial.println ("***ERROR: Send Failure on sendAck!");

		return false;
	}

	return true;
}

// send an error message to a client
bool TftpServer::sendError (uint16_t errorCode, const std::string errorMessage, const char* debugMessage) {

	if (!sendError (errorCode, errorMessage, debugMessage, m_remoteIpAddress, m_remotePort)) {

		return false;
	}

	return true;
}

// send an error message to a client
bool TftpServer::sendError (uint16_t errorCode, const std::string errorMessage, const char* debugMessage,
		IPAddress remoteIpAddress, uint16_t remotePort) {

	if (m_serialDebug) Serial.println (debugMessage);

	uint16_t opCode = ERROR;

	// First 2 bytes of error message are opcode
	udpBuffer[0] = static_cast <uint8_t> (opCode >> 8);
	udpBuffer[1] = static_cast <uint8_t> (opCode);

	// Next 2 bytes of error message are the error code
	udpBuffer[2] = (static_cast <uint8_t> (errorCode >> 8));
	udpBuffer[3] = (static_cast <uint8_t> (errorCode));

	// Next set of bytes is an error message
	for (uint8_t i = 0; i < errorMessage.length(); ++i) {

		udpBuffer[4 + i] = errorMessage[i];
	}

	// last byte of error packet is a 0
	udpBuffer[4 + errorMessage.length()] = 0;

	// send the buffer and check for send errors
	if (m_tftp.sendPacket (udpBuffer, 5 + errorMessage.length(), remoteIpAddress, remotePort) < 0) {

		if (m_serialDebug) Serial.println ("***ERROR: Send Failure on sendError!");

		return false;
	}

	return true;
}

// read next 2 bytes from the buffer
uint16_t TftpServer::readWord() {

	uint16_t MSB = 0;
	uint16_t LSB = 0;

	MSB = static_cast <uint16_t> (udpBuffer [m_bufferPosition++] << 8);
	LSB = udpBuffer [m_bufferPosition++];

	return (MSB | LSB);
}

void TftpServer::readText (std::string& buffer) {

	buffer.clear();

	// read in characters of a string until a 0 byte (NULL) is found
	while (udpBuffer [m_bufferPosition] != 0)
		buffer.push_back (udpBuffer [m_bufferPosition++]);

	// move past the 0
	m_bufferPosition++;
}

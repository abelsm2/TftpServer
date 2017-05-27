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
 *
 * @brief Minimal implementation of TFTP server for Particle devices
 *
 * This is a VERY minimal implementation of a TFTP Server for the Particle
 * environment (Tested on the P0 and P1).  The initial version will only support
 * the GET methods and does not implement any methods for PUT.  This made the most
 * sense during development since it seemed like there was more need to get files
 * off of an SD card than there were to put them on.
 *
 * The server is opened on port 69 by default.  The clientConnected method should
 * be run in loop as often as possible to improve responsiveness and to avoid having
 * the client send duplicate requests due to retransmission timeouts.  Once TRUE is
 * returned by clientConnected() it means a UDP packet was received on port 69 and
 * you can call handleClientRequest() to do the rest.  The function will block until
 * the client request is taken care of and then control will pass back to the calling
 * function.
 *
 * In order to have files to send, this library relies on the SdFat-Particle
 * library.  A pointer to an SdFat object is passed as part of begin() so the
 * TFTP server will have access to the SD card without having to create it's own
 * instance of the file system.  This also makes the library agnostic as to which
 * SPI instance is used by SdFat (SPI vs. SPI1).
 *
 * It would be best to ensure that no files are open prior to passing control off to
 * the TFTP server.  Since files are opened and closed as part of the GET/PUT process,
 * it could potentially cause file corruption if there was already a file open.
 *
 * The attempt has been made to stick as close to the TFTP protocol as possible.
 * Timeouts were implemented as best I could figure out because the
 * specification doesn't cover timeouts and retransmission explicitly.  Also, to make
 * like simpler with the limited number of sockets available, the transfer ID for the
 * server is kept at 69 rather than choosing a new random port number.
 *
 * The specification can be located at: https://tools.ietf.org/html/rfc1350
 *
 * @note A buffer size of 516 bytes is allocated for TFTP transfers
 *
 * @note Library developed using ARM GCC 5.3
 *
 * From RFC 1350:
 *
 * TFTP Formats
 *
 *  Type   Op #     Format without header
 *
 *         2 bytes    string   1 byte     string   1 byte
 *         -----------------------------------------------
 *  RRQ/  | 01/02 |  Filename  |   0  |    Mode    |   0  |
 *  WRQ    -----------------------------------------------
 *         2 bytes    2 bytes       n bytes
 *         ---------------------------------
 *  DATA  | 03    |   Block #  |    Data    |
 *         ---------------------------------
 *         2 bytes    2 bytes
 *         -------------------
 *  ACK   | 04    |   Block #  |
 *         --------------------
 *         2 bytes  2 bytes        string    1 byte
 *         ----------------------------------------
 *  ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |
 *         ----------------------------------------
 */

#ifndef _TFTPSERVER_H_
#define _TFTPSERVER_H_

#include <SdFat.h>

/**
 * @class TftpServer
 */
class TftpServer {

public:

	/**
	 * Start the TFTP server.
	 *
	 * @param sd pointer to an SdFat instance for access to the file system
	 * @param portNum TFTP port number.  69 by default.
	 * @param serialDebug Set true for debug information on serial.  False by default.
	 * @return True if UDP.begin() is successful, false otherwise.
	 *
	 * @note Default port number for TFTP protocol is port 69 and should not be changed
	 * unless you can change the port number expected on the TFTP client.
	 */
	bool begin(SdFat* sd, bool serialDebug = false, uint16_t portNum = 69);
	
	/**
	 * Stop the TFTP server and the UDP instance created within as well as any
	 * files that may be open.
	 *
	 * @note this will free up the UDP socket used by the library
	 *
	 * @warning this will require calling begin again before any TFTP transfers can take place
	 */
	void stop();

	/**
	 * Check to see if a packet has arrived at the TFTP port.
	 *
	 * @return True if a packet has been received, false otherwise.
	 *
	 * @note This method should be used in loop() in conjunction with processRequest()
	 *
	 * @see processRequest()
	 */
	bool checkForPacket();

	/**
	 * Parse incoming client connection and react accordingly.
	 *
	 * This method will respond to a client connecting to the TFTP server by
	 * performing the required actions.
	 *
	 * @note This method will block loop() until finished with the clients request
	 */
	void processRequest();


private:

	/**
	 * @enum opCodes_t
	 * enum to contain all the possible opcodes in the TFTP protocol
	 */
	enum opCodes_t {
		RRQ   = 1, ///< Read request (RRQ)
		WRQ   = 2, ///< Write request (WRQ)
		DATA  = 3, ///< Data (DATA)
		ACK   = 4, ///< Acknowledgment (ACK)
		ERROR = 5  ///< Error (ERROR)
	};

	/**
	 * @enum errorCodes_t
	 * enum to contain all the different error codes in the TFTP protocol
	 */
	enum errorCodes_t {
		NOT_DEFINED       = 0, ///< Not defined, see error message (if any).
		FILE_NOT_FOUND    = 1, ///< File not found.
		ACCESS_VIOLATION  = 2, ///< Access violation.
		DISK_FULL         = 3, ///< Disk full or allocation exceeded.
		ILLEGAL_OPERATION = 4, ///< Illegal TFTP operation.
		UNKNOWN_ID        = 5, ///< Unknown transfer ID.
		FILE_EXISTS       = 6, ///< File already exists.
		NO_USER           = 7, ///< No such user.
	};

	// UDP variables
	UDP m_tftp;
	int16_t m_bufferCount;
	uint16_t m_bufferPosition;
	uint16_t m_localPort;
	IPAddress m_remoteIpAddress;
	uint16_t m_remotePort;

	// TFTP variables
	uint16_t m_opCode;
	uint16_t m_errorCode;
	uint16_t m_blockNumber;
	uint16_t m_blockSize;
	uint16_t m_transferId;
	uint32_t m_droppedPacket;

	// Round Trip Time (RTT) calculation variables
	float m_rtt;
	uint32_t m_rttCalcStart;
	uint32_t m_resendStart;
	uint32_t m_rttCalcFinish;
	uint32_t m_timeout;
	uint8_t m_numberOfRetransmissions;

	// File handling
	File m_file;
	SdFat* m_sd;
	std::string m_fileName;
	std::string m_errorString;
	std::string m_transferMode;

	// debug output
	bool m_serialDebug;

	// TFTP human readable error messages
	const std::string m_errorFeatureNotSupported = "feature not supported";
	const std::string m_errorFileNotFound = "file not found";
	const std::string m_errorBadOpcodeReceived = "bad opcode received";
	const std::string m_errorAccessViolation = "access violation";
	const std::string m_errorDiskFull = "disk full or allocation exceeded";
	const std::string m_errorIllegalOperation = "illegal tftp operation";
	const std::string m_errorUnknownTransferId = "unknown transfer id";
	const std::string m_errorFileAlreadyExists = "file already exists";
	const std::string m_errorNoSuchUser = "no such user";
	const std::string m_errorNetasciiNotSupported = "netascii not supported";
	const std::string m_errorTimeoutOnSend = "timeout on send";

	/**
	 * Adaptive updating of the UDP round trip time
	 *
	 * @note timeout is constrained between 10 ms and 500 ms (just because)
	 */
	void updateTimeout();

	/**
	 * Read a 2 byte variable from the buffer
	 *
	 * @return Single word (2 bytes) from incoming buffer
	 */
	uint16_t readWord();

	/**
	 * Read the next section of text from a TFTP message
	 *
	 * @param buffer string to store the text within
	 */
	void readText(std::string& buffer);

	/**
	 * Handles all the transactions for reading a file from the SD card and sending it
	 * to the client.
	 */
	void handleReadRequest();

	/**
	 * Handles all the transactions for getting a file from the client and storing it
	 * on the SD card.
	 */
	void handleWriteRequest();

	/**
	 * Send a data packet to the client.
	 *
	 * @param txBuffer The full buffer that is to be sent.
	 * @return True on success or False on send error.
	 */
	bool sendDataPacket ();

	/**
	 * This method will generate an ACK message
	 *
	 * @param blockNumber File block number the ACK message refers to
	 * @return True on success or False on send error.
	 */
	bool sendAck (uint16_t blockNumber);

	/**
	 * Send an error code and message to a client
	 *
	 * @param errorCode Code corresponding to the TFTP error type
	 * @param errorMessage String corresponding to the error type
	 * @param debugMessage String to send to serial when serialDebug is set TRUE in begin()
	 * @return True on success or False on send error.
	 */
	bool sendError (uint16_t errorCode, const std::string errorMessage, const char* debugMessage);

	/**
	 * Send an error code and message to a client
	 *
	 * @param errorCode Code corresponding to the TFTP error type
	 * @param errorMessage String corresponding to the error type
	 * @param debugMessage String to send to serial when serialDebug is set TRUE in begin()
	 * @param remoteIpAddress IP address to send error message.  Default is sender of RRQ/WRQ
	 * @param remotePort Port number to send error message.  Default is sender of RRQ/WRQ
	 * @return True on success or False on send error.
	 */
	bool sendError (uint16_t errorCode, const std::string errorMessage, const char* debugMessage,
			IPAddress remoteIpAddress, uint16_t remotePort);

};

#endif /* _TFTPSERVER_H_ */

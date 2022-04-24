/*
    nanoMODBUS - A compact MODBUS RTU/TCP C library for microcontrollers
    Copyright (C) 2022  Valerio De Benedetto (@debevv)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/** @file */

/*! \mainpage nanoMODBUS - A compact MODBUS RTU/TCP C library for microcontrollers
 * nanoMODBUS is a small C library that implements the Modbus protocol. It is especially useful in resource-constrained
 * system like microcontrollers.
 *
 * GtiHub: <a href="https://github.com/debevv/nanoMODBUS">https://github.com/debevv/nanoMODBUS</a>
 *
 * API reference: \link nanomodbus.h \endlink
 *
 */

#ifndef NANOMODBUS_H
#define NANOMODBUS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * nanoMODBUS errors.
 * Values <= 0 are library errors, > 0 are modbus exceptions.
 */
typedef enum nmbs_error {
    // Library errors
    NMBS_ERROR_TRANSPORT = -4,        /**< Transport error */
    NMBS_ERROR_TIMEOUT = -3,          /**< Read/write timeout occurred */
    NMBS_ERROR_INVALID_RESPONSE = -2, /**< Received invalid response from server */
    NMBS_ERROR_INVALID_ARGUMENT = -1, /**< Invalid argument provided */
    NMBS_ERROR_NONE = 0,              /**< No error */

    // Modbus exceptions
    NMBS_EXCEPTION_ILLEGAL_FUNCTION = 1,      /**< Modbus exception 1 */
    NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,  /**< Modbus exception 2 */
    NMBS_EXCEPTION_ILLEGAL_DATA_VALUE = 3,    /**< Modbus exception 3 */
    NMBS_EXCEPTION_SERVER_DEVICE_FAILURE = 4, /**< Modbus exception 4 */
} nmbs_error;

/**
 * Return whether the nmbs_error is a modbus exception
 * @e nmbs_error to check
 */
#define nmbs_error_is_exception(e) ((e) > 0 && (e) < 5)


/**
 * Bitfield consisting of 2000 coils/discrete inputs
 */
typedef uint8_t nmbs_bitfield[250];

/**
 * Read a bit from the nmbs_bitfield bf at position b
 */
#define nmbs_bitfield_read(bf, b) ((bool) ((bf)[(b) / 8] & (0x1 << ((b) % 8))))

/**
 * Write value v to the nmbs_bitfield bf at position b
 */
#define nmbs_bitfield_write(bf, b, v)                                                                                  \
    (((bf)[(b) / 8]) = ((v) ? (((bf)[(b) / 8]) | (0x1 << ((b) % 8))) : (((bf)[(b) / 8]) & ~(0x1 << ((b) % 8)))))

/**
 * Reset (zero) the whole bitfield
 */
#define nmbs_bitfield_reset(bf) memset(bf, 0, sizeof(nmbs_bitfield))


/**
 * Modbus transport type.
 */
typedef enum nmbs_transport {
    NMBS_TRANSPORT_RTU = 1,
    NMBS_TRANSPORT_TCP = 2,
} nmbs_transport;


/**
 * nanoMODBUS platform configuration struct.
 * Passed to nmbs_server_create() and nmbs_client_create().
 *
 * read_byte() and write_byte() are the platform-specific methods that read/write data to/from a serial port or a TCP connection.
 * Both methods should block until the requested byte is read/written.
 * If your implementation uses a read/write timeout, and the timeout expires, the methods should return 0.
 * Their return values should be:
 * - `1` in case of success
 * - `0` if no data is available immediately or after an internal timeout expiration
 * - `-1` in case of error
 *
 * sleep() is the platform-specific method to pause for a certain amount of milliseconds.
 *
 * These methods accept a pointer to arbitrary user-data, which is the arg member of this struct.
 * After the creation of an instance it can be changed with nmbs_set_platform_arg().
 */
typedef struct nmbs_platform_conf {
    nmbs_transport transport;                         /*!< Transport type */
    int (*read_byte)(uint8_t* b, int32_t, void* arg); /*!< Byte read transport function pointer */
    int (*write_byte)(uint8_t b, int32_t, void* arg); /*!< Byte write transport function pointer */
    void (*sleep)(uint32_t milliseconds, void* arg);  /*!< Sleep function pointer */
    void* arg;                                        /*!< User data, will be passed to functions above */
} nmbs_platform_conf;


/**
 * Modbus server request callbacks. Passed to nmbs_server_create().
 */
typedef struct nmbs_callbacks {
    nmbs_error (*read_coils)(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out);
    nmbs_error (*read_discrete_inputs)(uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out);
    nmbs_error (*read_holding_registers)(uint16_t address, uint16_t quantity, uint16_t* registers_out);
    nmbs_error (*read_input_registers)(uint16_t address, uint16_t quantity, uint16_t* registers_out);
    nmbs_error (*write_single_coil)(uint16_t address, bool value);
    nmbs_error (*write_single_register)(uint16_t address, uint16_t value);
    nmbs_error (*write_multiple_coils)(uint16_t address, uint16_t quantity, const nmbs_bitfield coils);
    nmbs_error (*write_multiple_registers)(uint16_t address, uint16_t quantity, const uint16_t* registers);
} nmbs_callbacks;


/**
 * nanoMODBUS client/server instance type. All struct members are to be considered private, it is not advisable to read/write them directly.
 */
typedef struct nmbs_t {
    struct {
        uint8_t buf[260];
        uint16_t buf_idx;

        uint8_t unit_id;
        uint8_t fc;
        uint16_t transaction_id;
        bool broadcast;
        bool ignored;
    } msg;

    nmbs_callbacks callbacks;

    int32_t byte_timeout_ms;
    int32_t read_timeout_ms;
    uint32_t byte_spacing_ms;

    nmbs_platform_conf platform;

    uint8_t address_rtu;
    uint8_t dest_address_rtu;
    uint16_t current_tid;
} nmbs_t;

/**
 * Modbus broadcast address. Can be passed to nmbs_set_destination_rtu_address().
 */
static const uint8_t NMBS_BROADCAST_ADDRESS = 0;

#ifndef NMBS_CLIENT_DISABLED
/** Create a new Modbus client.
 * @param nmbs pointer to the nmbs_t instance where the client will be created.
 * @param platform_conf nmbs_platform_conf struct with platform configuration.
 *
* @return NMBS_ERROR_NONE if successful, NMBS_ERROR_INVALID_ARGUMENT otherwise.
 */
nmbs_error nmbs_client_create(nmbs_t* nmbs, const nmbs_platform_conf* platform_conf);
#endif

#ifndef NMBS_SERVER_DISABLED
/** Create a new Modbus server.
 * @param nmbs pointer to the nmbs_t instance where the client will be created.
 * @param address_rtu RTU address of this server. Can be 0 if transport is not RTU.
 * @param platform_conf nmbs_platform_conf struct with platform configuration.
 * @param callbacks nmbs_callbacks struct with server request callbacks.
 *
 * @return NMBS_ERROR_NONE if successful, NMBS_ERROR_INVALID_ARGUMENT otherwise.
 */
nmbs_error nmbs_server_create(nmbs_t* nmbs, uint8_t address_rtu, const nmbs_platform_conf* platform_conf,
                              const nmbs_callbacks* callbacks);
#endif

/** Set the request/response timeout.
 * If the target instance is a server, sets the timeout of the nmbs_server_poll() function.
 * If the target instance is a client, sets the response timeout after sending a request. In case of timeout, the called method will return NMBS_ERROR_TIMEOUT.
 * @param nmbs pointer to the nmbs_t instance
 * @param timeout_ms timeout in milliseconds. If < 0, the timeout is disabled.
 */
void nmbs_set_read_timeout(nmbs_t* nmbs, int32_t timeout_ms);

/** Set the timeout between the reception of two consecutive bytes.
 * @param nmbs pointer to the nmbs_t instance
 * @param timeout_ms timeout in milliseconds. If < 0, the timeout is disabled.
 */
void nmbs_set_byte_timeout(nmbs_t* nmbs, int32_t timeout_ms);

/** Set the spacing between two sent bytes. This value is ignored when transport is not RTU.
 * @param nmbs pointer to the nmbs_t instance
 * @param timeout_ms time spacing in milliseconds.
 */
void nmbs_set_byte_spacing(nmbs_t* nmbs, uint32_t spacing_ms);

/** Set the pointer to user data argument passed to platform functions.
 * @param nmbs pointer to the nmbs_t instance
 * @param arg user data argument
 */
void nmbs_set_platform_arg(nmbs_t* nmbs, void* arg);

#ifndef NMBS_CLIENT_DISABLED
/** Set the recipient server address of the next request on RTU transport.
 * @param nmbs pointer to the nmbs_t instance
 * @param address server address
 */
void nmbs_set_destination_rtu_address(nmbs_t* nmbs, uint8_t address);
#endif

#ifndef NMBS_SERVER_DISABLED
/** Handle incoming requests to the server.
 * This function should be called in a loop in order to serve any incoming request. Its maximum duration, in case of no
 * received request, is the value set with nmbs_set_read_timeout() (unless set to < 0).
 * @param nmbs pointer to the nmbs_t instance
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_server_poll(nmbs_t* nmbs);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Send a FC 01 (0x01) Read Coils request
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of coils
 * @param coils_out nmbs_bitfield where the coils will be stored
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_coils(nmbs_t* nmbs, uint16_t address, uint16_t quantity, nmbs_bitfield coils_out);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Send a FC 02 (0x02) Read Discrete Inputs request
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of inputs
 * @param inputs_out nmbs_bitfield where the discrete inputs will be stored
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_discrete_inputs(nmbs_t* nmbs, uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Send a FC 03 (0x03) Read Holding Registers request
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers_out array where the registers will be stored
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_holding_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, uint16_t* registers_out);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Send a FC 04 (0x04) Read Input Registers request
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers_out array where the registers will be stored
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_read_input_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, uint16_t* registers_out);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Send a FC 05 (0x05) Write Single Coil request
 * @param nmbs pointer to the nmbs_t instance
 * @param address coil address
 * @param value coil value
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_single_coil(nmbs_t* nmbs, uint16_t address, bool value);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Send a FC 06 (0x06) Write Single Register request
 * @param nmbs pointer to the nmbs_t instance
 * @param address register address
 * @param value register value
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_single_register(nmbs_t* nmbs, uint16_t address, uint16_t value);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Send a FC 15 (0x0F) Write Multiple Coils
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of coils
 * @param coils bitfield of coils values
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_multiple_coils(nmbs_t* nmbs, uint16_t address, uint16_t quantity, const nmbs_bitfield coils);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Send a FC 16 (0x10) Write Multiple Registers
 * @param nmbs pointer to the nmbs_t instance
 * @param address starting address
 * @param quantity quantity of registers
 * @param registers array of registers values
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_write_multiple_registers(nmbs_t* nmbs, uint16_t address, uint16_t quantity, const uint16_t* registers);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Send a raw Modbus PDU.
 * CRC on RTU will be calculated and sent by this function.
 * @param nmbs pointer to the nmbs_t instance
 * @param fc request function code
 * @param data request data. It's up to the caller to convert this data to network byte order
 * @param data_len length of the data parameter
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_send_raw_pdu(nmbs_t* nmbs, uint8_t fc, const void* data, uint32_t data_len);
#endif

#ifndef NMBS_CLIENT_DISABLED
/** Receive a raw response Modbus PDU.
 * @param nmbs pointer to the nmbs_t instance
 * @param data_out response data. It's up to the caller to convert this data to host byte order.
 * @param data_out_len length of the data_out parameter
 *
 * @return NMBS_ERROR_NONE if successful, other errors otherwise.
 */
nmbs_error nmbs_receive_raw_pdu_response(nmbs_t* nmbs, void* data_out, uint32_t data_out_len);
#endif

#ifndef NMBS_STRERROR_DISABLED
/** Convert a nmbs_error to string
 * @param error error to be converted
 *
 * @return string representation of the error
 */
const char* nmbs_strerror(nmbs_error error);
#endif


#endif    //NANOMODBUS_H
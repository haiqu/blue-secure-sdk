/*******************************************************************************
*   Ledger Blue - Secure firmware
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/


#ifndef OS_H
#define OS_H

/**
 * Quality development guidelines:
 * - NO header defined per arch and included in common if needed per arch,
 * define below
 * - exception model
 * - G_ prefix for RAM vars
 * - N_ prefix for NVRAM vars (mandatory for x86 link script to operate
 * correctly)
 * - C_ prefix for ROM   constants (mandatory for x86 link script to operate
 * correctly)
 * - extensive use of * and arch specific C modifier
 */

// Arch definitions

#define macro_offsetof
#define OS_LITTLE_ENDIAN
#define NATIVE_64BITS
#define WIDE // const
#define WIDE_AS_INT unsigned long int
#define REENTRANT(x) x //

#include <setjmp.h>
#define __MPU_PRESENT 1 // THANKS ST FOR YOUR HARDWORK
#include <core_sc000.h>
#include <product.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef WIDE_NULL
#define WIDE_NULL ((void WIDE *)0)
#endif

// Placement Independence Code reference
// function that align the deferenced value in a rom struct to use it depending
// on the execution address
// can be used even if code is executing at the same place where it had been
// linked
#ifndef PIC
#define PIC(x) pic((unsigned int)x)
unsigned int pic(unsigned int linked_address);
#endif

#ifndef SYSCALL
// #define SYSCALL syscall
#define SYSCALL
#endif

#ifndef SHARED
// #define SHARED shared
#define SHARED
#endif

#ifndef PERMISSION
#define PERMISSION(name)
#endif

#ifndef PLENGTH
#define PLENGTH(len)
#endif

/**
 * Application has been loaded using a secure channel opened using the
 * bootloader's issuer
 * public key. This application is ledger legit.
 */
#define APPLICATION_FLAG_ISSUER 0x1

/**
 * Flag which combined with ::APPLICATION_FLAG_ISSUER.
 * The application is given full nvram access after the global seed has been
 * destroyed.
 */
#define APPLICATION_FLAG_BOLOS_UPGRADE 0x2

/**
 * Mark this application as defaultly booting along with the bootloader (no
 * application menu displayed)
 * Only one application can have this at a time. It is managed by the bootloader
 * interface.
 */
#define APPLICATION_FLAG_AUTOBOOT 0x4

// must be set on one application in the registry which is used
#define APPLICATION_FLAG_BOLOS_UX 0x8

#define APPLICATION_FLAG_SEED_RAW 0x10
#define APPLICATION_FLAG_SHARED_NVRAM 0x20
#define APPLICATION_FLAG_GLOBAL_PIN 0x40

/**
 * Application is disabled (during its upgrade or whatever)
 */

#define APPLICATION_FLAG_DISABLED 0x8000

#define APPLICATION_FLAG_NEG_MASK 0xFFFF0000UL

/* ----------------------------------------------------------------------- */
/* -                            TYPES                                    - */
/* ----------------------------------------------------------------------- */

// error type definition
typedef unsigned short exception_t;

// convenience declaration
typedef struct try_context_s try_context_t;

// structure to reduce the code size generated for the close try (on stm7)
struct try_context_s {
    // current exception context
    jmp_buf jmp_buf;

    // previous exception contexts (if null, then will fail the same way as
    // before, segv, therefore don't mind chaining)
    try_context_t *previous;

    // current exception if any
    exception_t ex;
};

/* ----------------------------------------------------------------------- */
/* -                            GLOBALS                                  - */
/* ----------------------------------------------------------------------- */

// the global apdu buffer
#define IO_APDU_BUFFER_SIZE (5 + 255)
extern unsigned char G_io_apdu_buffer[IO_APDU_BUFFER_SIZE];

extern try_context_t *G_try_last_open_context;

/* ----------------------------------------------------------------------- */
/* -                            ENTRY POINT                              - */
/* ----------------------------------------------------------------------- */

// os entry point
void app_main(void);

/* ----------------------------------------------------------------------- */
/* -                            OS FUNCTIONS                             - */
/* ----------------------------------------------------------------------- */
#define os_swap_u16(u16)                                                       \
    ((((unsigned short)(u16) << 8) & 0xFF00U) |                                \
     (((unsigned short)(u16) >> 8) & 0x00FFU))

#define os_swap_u32(u32)                                                       \
    (((unsigned long int)(u32) >> 24) |                                        \
     (((unsigned long int)(u32) << 8) & 0x00FF0000UL) |                        \
     (((unsigned long int)(u32) >> 8) & 0x0000FF00UL) |                        \
     ((unsigned long int)(u32) << 24))

REENTRANT(void os_memmove(void *dst, void WIDE *src, unsigned short length));
#define os_memcpy os_memmove

void os_memset(void *dst, unsigned char c, unsigned short length);

char os_memcmp(const void WIDE *buf1, const void WIDE *buf2,
               unsigned short length);

void os_xor(void *dst, void WIDE *src1, void WIDE *src2, unsigned short length);

// patch point, address used to dispatch, no index
REENTRANT(void patch(void));

// receive the chip
REENTRANT(void reset(void));

// send tx_len bytes (atr or rapdu) and retrieve the length of the next command
// apdu (over the requested channel)
#define CHANNEL_APDU 0
#define CHANNEL_KEYBOARD 1
#define CHANNEL_SPI 2
#define IO_RESET_AFTER_REPLIED 0x80
#define IO_RECEIVE_DATA 0x40
#define IO_RETURN_AFTER_TX 0x20
#define IO_FLAGS 0xF0
unsigned short io_exchange(unsigned char channel_and_flags,
                           unsigned short tx_len);

typedef enum {
    IO_APDU_MEDIA_NONE = 0, // not correctly in an apdu exchange
    IO_APDU_MEDIA_USB_HID = 1,
    IO_APDU_MEDIA_BLE,
    IO_APDU_MEDIA_NFC,
} io_apdu_media_t;

extern volatile io_apdu_media_t G_io_apdu_media;
/**
 * Return 1 when the event has been processed, 0 else
 */
// io callback in the application called when an interrupt based channel has
// received data to be processed
unsigned char io_event(unsigned char channel);

/**
 * Function takes 0 for first call. Returns 0 when timeout has occured. Returned
 * value is passed as argument for next call, acting as a timeout context.
 */
unsigned short io_timeout(unsigned short last_timeout);

// write in persistent memory, to make things easy keep a layout of the memory
// in a structure and update fields upon needs
// NOTE: accept copy from far memory to another far memory.
// @param src_adr NULL to fill with 00's
SYSCALL void nvm_write(void WIDE *dst_adr PLENGTH(src_len),
                       void WIDE *src_adr PLENGTH(src_len),
                       unsigned int src_len);

/* ----------------------------------------------------------------------- */
/* -                            EXCEPTIONS                               - */
/* ----------------------------------------------------------------------- */

// workaround to make sure defines are replaced by their value for example
#define CPP_CONCAT(x, y) CPP_CONCAT_x(x, y)
#define CPP_CONCAT_x(x, y) x##y

// -----------------------------------------------------------------------
// - BEGIN TRY
// -----------------------------------------------------------------------

#define BEGIN_TRY_L(L)                                                         \
    {                                                                          \
        try_context_t __try##L;

// -----------------------------------------------------------------------
// - TRY
// -----------------------------------------------------------------------
#define TRY_L(L)                                                               \
    __try                                                                      \
        ##L.previous = G_try_last_open_context;                                \
    __try                                                                      \
        ##L.ex = setjmp(__try##L.jmp_buf);                                     \
    G_try_last_open_context = &__try##L;                                       \
    if (__try##L.ex == 0) {
// -----------------------------------------------------------------------
// - EXCEPTION CATCH
// -----------------------------------------------------------------------
#define CATCH_L(L, x)                                                          \
    goto CPP_CONCAT(__FINALLY, L);                                             \
    }                                                                          \
    else if (__try##L.ex == x) {                                               \
        G_try_last_open_context = __try##L.previous;

// -----------------------------------------------------------------------
// - EXCEPTION CATCH OTHER
// -----------------------------------------------------------------------
#define CATCH_OTHER_L(L, e)                                                    \
    goto CPP_CONCAT(__FINALLY, L);                                             \
    }                                                                          \
    else {                                                                     \
        exception_t e;                                                         \
        e = __try##L.ex;                                                       \
        __try                                                                  \
            ##L.ex = 0;                                                        \
        G_try_last_open_context = __try##L.previous;

// -----------------------------------------------------------------------
// - EXCEPTION CATCH ALL
// -----------------------------------------------------------------------
#define CATCH_ALL_L(L)                                                         \
    goto CPP_CONCAT(__FINALLY, L);                                             \
    }                                                                          \
    else {                                                                     \
        __try                                                                  \
            ##L.ex = 0;                                                        \
        G_try_last_open_context = __try##L.previous;

// -----------------------------------------------------------------------
// - FINALLY
// -----------------------------------------------------------------------
#define FINALLY_L(L)                                                           \
    goto CPP_CONCAT(__FINALLY, L);                                             \
    }                                                                          \
    CPP_CONCAT(__FINALLY, L) : G_try_last_open_context = __try##L.previous;

// -----------------------------------------------------------------------
// - END TRY
// -----------------------------------------------------------------------
#define END_TRY_L(L)                                                           \
    if (__try##L.ex != 0) {                                                    \
        THROW_L(L, __try##L.ex);                                               \
    }                                                                          \
    }

// -----------------------------------------------------------------------
// - CLOSE TRY
// -----------------------------------------------------------------------
#define CLOSE_TRY_L(L)                                                         \
    G_try_last_open_context = G_try_last_open_context->previous

// -----------------------------------------------------------------------
// - EXCEPTION THROW
// -----------------------------------------------------------------------
#define THROW_L(L, x) longjmp(G_try_last_open_context->jmp_buf, x)

// Default macros when nesting is not used.
#define THROW(x) THROW_L(EX, x)
#define BEGIN_TRY BEGIN_TRY_L(EX)
#define TRY TRY_L(EX)
#define CATCH(x) CATCH_L(EX, x)
#define CATCH_OTHER(e) CATCH_OTHER_L(EX, e)
#define CATCH_ALL CATCH_ALL_L(EX)
#define FINALLY FINALLY_L(EX)
#define CLOSE_TRY CLOSE_TRY_L(EX)
#define END_TRY END_TRY_L(EX)

#define EXCEPTION 1
#define INVALID_PARAMETER 2
#define EXCEPTION_OVERFLOW 3
#define EXCEPTION_SECURITY 4
#define INVALID_CRC 5
#define INVALID_CHECKSUM 6
#define INVALID_COUNTER 7
#define NOT_SUPPORTED 8
#define INVALID_STATE 9
#define TIMEOUT 10
#define EXCEPTION_PIC 11
#define EXCEPTION_APPEXIT 12

// -----------------------------------------------------------------------
// - BASIC MATHS
// -----------------------------------------------------------------------
#define U2(hi, lo) ((((hi)&0xFF) << 8) | ((lo)&0xFF))
#define U4(hi3, hi2, lo1, lo0)                                                 \
    ((((hi3)&0xFF) << 24) | (((hi2)&0xFF) << 16) | (((lo1)&0xFF) << 8) |       \
     ((lo0)&0xFF))
#define U2BE(buf, off) ((((buf)[off] & 0xFF) << 8) | ((buf)[off + 1] & 0xFF))
#define U4BE(buf, off) ((U2BE(buf, off) << 16) | (U2BE(buf, off + 2) & 0xFFFF))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#ifdef macro_offsetof
#define offsetof(type, field) ((unsigned int)&(((type *)NULL)->field))
#endif

/* ----------------------------------------------------------------------- */
/* -                          CRYPTO FUNCTIONS                           - */
/* ----------------------------------------------------------------------- */
#include "cx.h"

/**
 BOLOS RAM LAYOUT
                msp                          psp                   psp
 | bolos ram <-os stack-| bolos ux ram <-ux_stack-| app ram <-app stack-|

 ux and app are seen as applications.
 os is not an application (it calls ux upon user inputs)
**/

/* ----------------------------------------------------------------------- */
/* -                        BOLOS UX DEFINITIONS                         - */
/* ----------------------------------------------------------------------- */
typedef enum bolos_ux_e {
    BOLOS_UX_INITIALIZE =
        0, // tag to be processed by the UX from the serial line

    BOLOS_UX_EVENT, // tag to be processed by the UX from the serial line

    BOLOS_UX_BOOT_NOT_PERSONALIZED,
    BOLOS_UX_BOOT_RECOVERY,
    BOLOS_UX_BOOT_ONBOARDING,
    BOLOS_UX_BOOT,
    BOLOS_UX_BOOT_UNSIGNED_WIPE,
    BOLOS_UX_DASHBOARD,

    // cleanup screen displayed by the previous ux, useful for
    BOLOS_UX_BLANK_PREVIOUS,

    BOLOS_UX_LOADER, // display loader screen or advance it

    BOLOS_UX_VALIDATE_PIN,
    BOLOS_UX_WIPED_DEVICE,
    BOLOS_UX_CONSENT_UPGRADE,
    BOLOS_UX_CONSENT_APP_ADD,
    BOLOS_UX_CONSENT_APP_DEL,
    BOLOS_UX_CONSENT_APP_UPG,
    BOLOS_UX_CONSENT_FOREIGN_KEY,
    BOLOS_UX_APPEXIT,
    BOLOS_UX_KEYBOARD,

    BOLOS_UX_SETTINGS,
    /*
    BOLOS_UX_INPUT_TEXT , // how to pass param (title?text?icon?)
    BOLOS_UX_INPUT_VALUE ,
    BOLOS_UX_INPUT_YESNO ,
    BOLOS_UX_INPUT_CONFIRMCANCEL ,
    */
} bolos_ux_t;

#define BOLOS_UX_OK 0xB0105011
#define BOLOS_UX_CANCEL 0xB0105022
#define BOLOS_UX_ERROR 0xB0105033
#define BOLOS_UX_CONTINUE 0

/* ----------------------------------------------------------------------- */
/* -                       APPLICATION FUNCTIONS                         - */
/* ----------------------------------------------------------------------- */

typedef void (*appmain_t)(void);

// application slot description
typedef struct application_s {
    // application crc over the application structure
    unsigned short crc;

    // nvram start address for this application (to check overlap when loading,
    // and mpu lock)
    unsigned char *nvram_begin;
    // nvram stop address (exclusive) for this application (to check overlap
    // when loading, and mpu lock)
    unsigned char *nvram_end;

    // address of the main address, must be set according to BLX spec ( ORed
    // with 1 when jumping into Thumb code
    appmain_t main;

    // special flags for this application
    unsigned int flags;

    // the timeout before autobooting this application, meaningless when flags
    // has not the ::APPLICATION_AUTOBOOT value.
    unsigned int autoboot_timeout;

    unsigned int seed_derivation_first_level;

// for fancy display upon the boot display, \0 terminated.
#define APPLICATION_NAME_MAXLEN 32
    unsigned char name[APPLICATION_NAME_MAXLEN + 1];

    // SHA256 reserved space for the bootloader to store the application's code
    // hash.
    unsigned char hash[32];

} application_t;

// Structure that defines the parameters to exchange with the BOLOS UX
// application
typedef struct bolos_ux_params_s {
    bolos_ux_t ux_id;
    // length of parameters in the u union to be copied during the syscall
    unsigned int len;

    // structure to overlay parameters for each BOLOS_UX
    union {
        struct {
            unsigned int app_idx;
        } appexitb;

        struct {
            application_t appentry;
        } appdel;

        struct {
            application_t appentry;
        } appadd;

        // pass the whole load application to that the os version and hash are
        // available
        struct {
            application_t upgrade;
        } upgrade;

        struct {
            cx_ecfp_public_key_t host_pubkey;
        } foreign_key;

        struct {
            unsigned int keycode;
        } keyboard;

        struct {
            unsigned int x;
            unsigned int y;
            unsigned int width;
            unsigned int height;
        } loader;

    } u;

} bolos_ux_params_t;

// any application can wipe the global pin, global seed, user's keys
SYSCALL void os_perso_wipe(void);
SYSCALL PERMISSION(APPLICATION_FLAG_BOLOS_UX) void os_perso_set_pin(
    unsigned char *pin PLENGTH(length), unsigned int length);
SYSCALL PERMISSION(APPLICATION_FLAG_BOLOS_UX) void os_perso_set_seed(
    unsigned char *seed PLENGTH(length), unsigned int length);
SYSCALL PERMISSION(APPLICATION_FLAG_BOLOS_UX) void os_perso_finalize(void);

// checked in the ux flow to avoid asking the pin for example
SYSCALL PERMISSION(APPLICATION_FLAG_BOLOS_UX) unsigned int os_perso_isonboarded(
    void);

// nvram shared zone access right => MPU opening at application switch, using a
// flags in the registry

// Global PIN
#define DEFAULT_PIN_RETRIES 3
SYSCALL PERMISSION(
    APPLICATION_FLAG_GLOBAL_PIN) unsigned int os_global_pin_is_validated(void);
// return 1 if validated
SYSCALL
    PERMISSION(APPLICATION_FLAG_GLOBAL_PIN) unsigned int os_global_pin_check(
        unsigned char *pin_buffer PLENGTH(pin_length),
        unsigned char pin_length);
SYSCALL
    PERMISSION(APPLICATION_FLAG_GLOBAL_PIN) void os_global_pin_invalidate(void);
SYSCALL PERMISSION(
    APPLICATION_FLAG_GLOBAL_PIN) unsigned int os_global_pin_retries(void);

SYSCALL
    PERMISSION(APPLICATION_FLAG_BOLOS_UX) unsigned int os_registry_count(void);
// return any entry, activated or not, enabled or not, empty or not. to enable
// full control
SYSCALL PERMISSION(APPLICATION_FLAG_BOLOS_UX) void os_registry_get(
    unsigned int index,
    application_t *out_application_entry PLENGTH(sizeof(application_t)));

// called by bolos initializer before entering bolos ux the first time
void os_sched_init(void);
void bolos_main(void);

// special constant to request execution of the bolos_ux
SYSCALL PERMISSION(APPLICATION_FLAG_BOLOS_UX) void os_sched_exec(
    unsigned int application_index);
SYSCALL void os_sched_exit(unsigned int exit_code);

// the bolos_ux declares the buffer to be used to store the ux parameter to be
// displayed
SYSCALL PERMISSION(APPLICATION_FLAG_BOLOS_UX) void os_ux_register(
    bolos_ux_params_t *parameter_ram_pointer
        PLENGTH(sizeof(bolos_ux_params_t)));

// return !0 when ux scenario has ended, else 0
// while not ended, all display/touch/ticker and other events are to be
// processed by the ux. only io is not processed.
// when returning !0 the application must send a general status (or continue its
// command flow)
SYSCALL unsigned int
os_ux(bolos_ux_params_t *params PLENGTH(sizeof(bolos_ux_params_t)));

// process all possible messages while waiting for a ux to finish,
// unprocessed messages are replied with a generic general status
// when returning the application must send a general status (or continue its
// command flow)
unsigned int os_ux_blocking(bolos_ux_params_t *params);

/* ----------------------------------------------------------------------- */
/* -                            IO FUNCTIONS                             - */
/* ----------------------------------------------------------------------- */
typedef REENTRANT(void (*io_send_t)(unsigned char *buffer,
                                    unsigned short length));

typedef REENTRANT(unsigned short (*io_recv_t)(unsigned char *buffer,
                                              unsigned short maxlenth));

typedef enum io_usb_hid_receive_status_e {
    IO_USB_APDU_RESET,
    IO_USB_APDU_MORE_DATA,
    IO_USB_APDU_RECEIVED,
} io_usb_hid_receive_status_t;

extern volatile unsigned int G_io_usb_hid_total_length;

void io_usb_hid_init(void);

io_usb_hid_receive_status_t
io_usb_hid_receive(io_send_t sndfct, unsigned char *buffer, unsigned short l);

unsigned short io_usb_hid_exchange(io_send_t sndfct, unsigned short sndlength,
                                   io_recv_t rcvfct, unsigned char flags);

/* ----------------------------------------------------------------------- */
/* -                          DEBUG FUNCTIONS                           - */
/* ----------------------------------------------------------------------- */
void screen_printf(const char *format, ...);

// emit a single byte
void screen_printc(unsigned char const c);

// syscall test
// SYSCALL void dummy_1(unsigned int* p PLENGTH(2+len+15+ len + 16 +
// sizeof(io_send_t) + 1 ), unsigned int len);

#endif // OS_H
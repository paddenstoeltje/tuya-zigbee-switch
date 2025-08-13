#ifndef _RELAY_H_
#define _RELAY_H_

#include "types.h"

typedef void (*ev_relay_callback_t)(void *, u8);


typedef struct
{
  u32                 pin;
  u32                 off_pin;
  u8                  on_high;
  u8                  on;
  ev_relay_callback_t on_change;
  void *              callback_param;
  u32                 turn_on_time;      // Timestamp when relay was turned on
  u8                  pending_off;       // Flag indicating off command is pending
  u32                 min_on_time_ms;    // Minimum on-time in milliseconds (default 75ms)
} relay_t;


/**
 * @brief      Initialize relay (set initial state)
 * @param	   *relay - Relay to use
 * @return     none
 */
void relay_init(relay_t *relay);


/**
 * @brief      Enable the relay
 * @param	   *relay - Relay to use
 * @return     none
 */
void relay_on(relay_t *relay);

/**
 * @brief      Disable the relay
 * @param	   *relay - Relay to use
 * @return     none
 */
void relay_off(relay_t *relay);

/**
 * @brief      Close the relay
 * @param	   *relay - Relay to use
 * @return     none
 */
void relay_toggle(relay_t *relay);

/**
 * @brief      Process relay timing - should be called periodically to handle minimum on-time
 * @param	   *relay - Relay to use
 * @return     none
 */
void relay_process_timing(relay_t *relay);

/**
 * @brief      Set minimum on-time for the relay
 * @param	   *relay - Relay to use
 * @param      min_time_ms - Minimum on-time in milliseconds
 * @return     none
 */
void relay_set_min_on_time(relay_t *relay, u32 min_time_ms);

#endif

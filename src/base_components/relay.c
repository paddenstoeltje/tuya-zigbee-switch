#include "relay.h"
#include "tl_common.h"
#include "millis.h"


void relay_init(relay_t *relay)
{
  relay->turn_on_time = 0;
  relay->pending_off = 0;
  relay->min_on_time_ms = 75;  // Default 75ms minimum on-time
  relay_off(relay);
}

void relay_on(relay_t *relay)
{
  printf("relay_on\r\n");
  drv_gpio_write(relay->pin, relay->on_high);
  if (relay->off_pin)
  {
    drv_gpio_write(relay->off_pin, !relay->on_high);
  }
  relay->on = 1;
  relay->turn_on_time = millis();  // Record when relay was turned on
  relay->pending_off = 0;          // Clear any pending off request
  if (relay->on_change != NULL)
  {
    relay->on_change(relay->callback_param, 1);
  }
}

void relay_off(relay_t *relay)
{
  // If relay is currently on, check if minimum on-time has elapsed
  if (relay->on && relay->turn_on_time > 0)
  {
    u32 elapsed_time = millis() - relay->turn_on_time;
    if (elapsed_time < relay->min_on_time_ms)
    {
      // Minimum on-time hasn't elapsed, mark for delayed off
      printf("relay_off - minimum on-time not met, delaying off (elapsed: %dms, required: %dms)\r\n", 
             elapsed_time, relay->min_on_time_ms);
      relay->pending_off = 1;
      return;
    }
  }
  
  // Actually turn off the relay
  printf("relay_off\r\n");
  drv_gpio_write(relay->pin, !relay->on_high);
  if (relay->off_pin)
  {
    drv_gpio_write(relay->off_pin, relay->on_high);
  }
  relay->on = 0;
  relay->pending_off = 0;
  relay->turn_on_time = 0;  // Reset timestamp
  if (relay->on_change != NULL)
  {
    relay->on_change(relay->callback_param, 0);
  }
}

void relay_toggle(relay_t *relay)
{
  printf("relay_toggle\r\n");
  if (relay->on)
  {
    relay_off(relay);
  }
  else
  {
    relay_on(relay);
  }
}

void relay_process_timing(relay_t *relay)
{
  // Check if there's a pending off request and minimum on-time has elapsed
  if (relay->pending_off && relay->on && relay->turn_on_time > 0)
  {
    u32 elapsed_time = millis() - relay->turn_on_time;
    if (elapsed_time >= relay->min_on_time_ms)
    {
      printf("relay_process_timing - minimum on-time met, executing delayed off\r\n");
      relay_off(relay);
    }
  }
}

void relay_set_min_on_time(relay_t *relay, u32 min_time_ms)
{
  relay->min_on_time_ms = min_time_ms;
  printf("relay_set_min_on_time - set to %dms\r\n", min_time_ms);
}

#include "relay.h"
#include "tl_common.h"
#include "millis.h"


void relay_init(relay_t *relay)
{
  relay_off(relay);
}

//WSA: zet aan en direct terug uit zodat de lamp van staat veranderd
void relay_on(relay_t *relay)
{
  //Zet aan
  printf("relay_on\r\n");
  drv_gpio_write(relay->pin, relay->on_high);
  if (relay->off_pin)
  {
    drv_gpio_write(relay->off_pin, !relay->on_high);
  }
  relay->on = 1;
  if (relay->on_change != NULL)
  {
    relay->on_change(relay->callback_param, 1);
  }
  // wacht 75ms
  WaitMs(75);

  //Zet uit
  printf("relay_off\r\n");
  drv_gpio_write(relay->pin, !relay->on_high);
  if (relay->off_pin)
  {
    drv_gpio_write(relay->off_pin, relay->on_high);
  }
  relay->on = 0;
  if (relay->on_change != NULL)
  {
    relay->on_change(relay->callback_param, 0);
  }
  // wacht 75ms
  WaitMs(75);

}

//WSA: zet aan en direct terug uit zodat de lamp van staat veranderd
void relay_off(relay_t *relay)
{
  //Zet aan
  printf("relay_on\r\n");
  drv_gpio_write(relay->pin, relay->on_high);
  if (relay->off_pin)
  {
    drv_gpio_write(relay->off_pin, !relay->on_high);
  }
  relay->on = 1;
  if (relay->on_change != NULL)
  {
    relay->on_change(relay->callback_param, 1);
  }
  // wacht 75ms
  WaitMs(75);
  
  //Zet uit
  printf("relay_off\r\n");
  drv_gpio_write(relay->pin, !relay->on_high);
  if (relay->off_pin)
  {
    drv_gpio_write(relay->off_pin, relay->on_high);
  }
  relay->on = 0;
  if (relay->on_change != NULL)
  {
    relay->on_change(relay->callback_param, 0);
  }
  // wacht 75ms
  WaitMs(75);
}

//WSA: toggle relay zet de relay gewoon aan. Ik misbruik deze cluster zodat ik de brightness kan veranderen.
//     nadien gewoon de relay_off nog eens callen als brightness_stop. dan gaat de relay nog eens aan (was al aan) en terug uit.
void relay_toggle(relay_t *relay)
{
  printf("relay_on\r\n");
  drv_gpio_write(relay->pin, relay->on_high);
  if (relay->off_pin)
  {
    drv_gpio_write(relay->off_pin, !relay->on_high);
  }
  relay->on = 1;
  if (relay->on_change != NULL)
  {
    relay->on_change(relay->callback_param, 1);
  }
}

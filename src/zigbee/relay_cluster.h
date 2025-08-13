#ifndef _RELAY_CLUSTER_H_
#define _RELAY_CLUSTER_H_

#include "tl_common.h"
#include "zb_common.h"
#include "zcl_include.h"

#include "endpoint.h"
#include "base_components/relay.h"
#include "base_components/led.h"

// Structure to track sequence numbers from different sources
typedef struct
{
  u16 srcAddr;        // Source address
  u8  lastSeqNum;     // Last seen sequence number
  u8  isValid;        // Whether this entry is valid
} seq_tracker_entry_t;

#define MAX_SEQ_TRACKERS 4  // Support up to 4 different sources

typedef struct
{
  u8            endpoint;
  u8            startup_mode;
  u8            indicator_led_mode;
  zclAttrInfo_t attr_infos[4];
  relay_t *     relay;
  led_t *       indicator_led;
  seq_tracker_entry_t seq_trackers[MAX_SEQ_TRACKERS];  // Sequence number tracking
} zigbee_relay_cluster;

void relay_cluster_add_to_endpoint(zigbee_relay_cluster *cluster, zigbee_endpoint *endpoint);

void relay_cluster_on(zigbee_relay_cluster *cluster);
void relay_cluster_off(zigbee_relay_cluster *cluster);
void relay_cluster_toggle(zigbee_relay_cluster *cluster);

void relay_cluster_report(zigbee_relay_cluster *cluster);

void update_relay_clusters();

void relay_cluster_callback_attr_write_trampoline(u8 clusterId, zclWriteCmd_t *pWriteReqCmd);

// Sequence number checking functions
bool relay_cluster_check_sequence(zigbee_relay_cluster *cluster, u16 srcAddr, u8 seqNum);
void relay_cluster_init_sequence_tracking(zigbee_relay_cluster *cluster);
void relay_cluster_update_sequence_tracker(zigbee_relay_cluster *cluster, u16 srcAddr, u8 seqNum);

#endif

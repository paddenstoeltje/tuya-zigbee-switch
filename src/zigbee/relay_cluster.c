#include "tl_common.h"
#include "zb_common.h"
#include "endpoint.h"
#include "relay_cluster.h"
#include "cluster_common.h"
#include "configs/nv_slots_cfg.h"
#include "custom_zcl/zcl_onoff_indicator.h"
#include "base_components/millis.h"

status_t relay_cluster_callback(zigbee_relay_cluster *cluster, zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t relay_cluster_callback_trampoline(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
void relay_cluster_on_relay_change(zigbee_relay_cluster *cluster, u8 state);
void relay_cluster_on_write_attr(zigbee_relay_cluster *cluster, zclWriteCmd_t *pWriteReqCmd);
void relay_cluster_store_attrs_to_nv(zigbee_relay_cluster *cluster);
void relay_cluster_load_attrs_from_nv(zigbee_relay_cluster *cluster);
void relay_cluster_handle_startup_mode(zigbee_relay_cluster *cluster);
void sync_indicator_led(zigbee_relay_cluster *cluster);
zigbee_relay_cluster *relay_cluster_by_endpoint[10];

void relay_cluster_callback_attr_write_trampoline(u8 clusterId, zclWriteCmd_t *pWriteReqCmd)
{
  relay_cluster_on_write_attr(relay_cluster_by_endpoint[clusterId], pWriteReqCmd);
}

void update_relay_clusters() {
  for (int i =0; i < 10; i++) {
    if (relay_cluster_by_endpoint[i] != NULL) {
      sync_indicator_led(relay_cluster_by_endpoint[i]);
    }
  }
}

void relay_cluster_add_to_endpoint(zigbee_relay_cluster *cluster, zigbee_endpoint *endpoint)
{
  relay_cluster_by_endpoint[endpoint->index] = cluster;
  cluster->endpoint = endpoint->index;
  relay_cluster_load_attrs_from_nv(cluster);
  relay_cluster_init_sequence_tracking(cluster);  // Initialize sequence tracking

  cluster->relay->callback_param = cluster;
  cluster->relay->on_change      = (ev_relay_callback_t)relay_cluster_on_relay_change;

  relay_cluster_handle_startup_mode(cluster);
  sync_indicator_led(cluster);

  SETUP_ATTR(0, ZCL_ATTRID_ONOFF, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, cluster->relay->on);
  SETUP_ATTR(1, ZCL_ATTRID_START_UP_ONOFF, ZCL_DATA_TYPE_ENUM8, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, cluster->startup_mode);
  if (cluster->indicator_led != NULL)
  {
    SETUP_ATTR(2, ZCL_ATTRID_ONOFF_INDICATOR_MODE, ZCL_DATA_TYPE_ENUM8, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, cluster->indicator_led_mode);
    SETUP_ATTR(3, ZCL_ATTRID_ONOFF_INDICATOR_STATE, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, cluster->indicator_led->on);
  }

  zigbee_endpoint_add_cluster(endpoint, 1, ZCL_CLUSTER_GEN_ON_OFF);
  zcl_specClusterInfo_t *info = zigbee_endpoint_reserve_info(endpoint);
  info->clusterId           = ZCL_CLUSTER_GEN_ON_OFF;
  info->manuCode            = MANUFACTURER_CODE_NONE;
  info->attrNum             = cluster->indicator_led != NULL ? 4 : 2;
  info->attrTbl             = cluster->attr_infos;
  info->clusterRegisterFunc = zcl_onOff_register;
  info->clusterAppCb        = relay_cluster_callback_trampoline;
}

status_t relay_cluster_callback_trampoline(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
  return(relay_cluster_callback(relay_cluster_by_endpoint[pAddrInfo->dstEp], pAddrInfo, cmdId, cmdPayload));
}

status_t relay_cluster_callback(zigbee_relay_cluster *cluster, zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
  // Check sequence number to prevent duplicate commands
  if (!relay_cluster_check_sequence(cluster, pAddrInfo->srcAddr, pAddrInfo->seqNum))
  {
  printf("Dropping duplicate command from addr %d, seq %d\r\n", pAddrInfo->srcAddr, pAddrInfo->seqNum);
    return(ZCL_STA_SUCCESS);
  }

  if (cmdId == ZCL_CMD_ONOFF_ON)
  {
    relay_cluster_on(cluster);
    relay_cluster_off(cluster);
  }
  else if (cmdId == ZCL_CMD_ONOFF_OFF)
  {
    relay_cluster_on(cluster);
    relay_cluster_off(cluster);
  }
  else if (cmdId == ZCL_CMD_ONOFF_TOGGLE)
  {
    //Reset all sequence trackers
    printf("Resetting all sequence trackers\r\n");
    for (u8 i = 0; i < MAX_SEQ_TRACKERS; i++)
    {
        cluster->seq_trackers[i].lastSeqNum = 0;
    }
    
    relay_cluster_on(cluster);
  }
  else
  {
    printf("Unknown command: %d\r\n", cmdId);
  }

  return(ZCL_STA_SUCCESS);
}

void sync_indicator_led(zigbee_relay_cluster *cluster)
{
  if (cluster->indicator_led_mode == ZCL_ONOFF_INDICATOR_MODE_MANUAL)
  {
    return;
  }
  if (cluster->indicator_led != NULL)
  {
    u8 turn_on_led = cluster->relay->on;
    if (cluster->indicator_led_mode == ZCL_ONOFF_INDICATOR_MODE_OPPOSITE)
    {
      turn_on_led = !turn_on_led;
    }
    if (turn_on_led)
    {
      led_on(cluster->indicator_led);
    }
    else
    {
      led_off(cluster->indicator_led);
    }
  }
}

void relay_cluster_on(zigbee_relay_cluster *cluster)
{
  relay_on(cluster->relay);
  // sync_indicator_led and relay_cluster_report will be called by relay_cluster_on_relay_change callback
}

void relay_cluster_off(zigbee_relay_cluster *cluster)
{
  relay_off(cluster->relay);
  // sync_indicator_led and relay_cluster_report will be called by relay_cluster_on_relay_change callback
}

void relay_cluster_toggle(zigbee_relay_cluster *cluster)
{

  relay_toggle(cluster->relay);
  // sync_indicator_led and relay_cluster_report will be called by relay_cluster_on_relay_change callback
}

void relay_cluster_report(zigbee_relay_cluster *cluster)
{
  if (zb_isDeviceJoinedNwk())
  {
    epInfo_t dstEpInfo;
    TL_SETSTRUCTCONTENT(dstEpInfo, 0);

    dstEpInfo.profileId   = HA_PROFILE_ID;
    dstEpInfo.dstAddrMode = APS_DSTADDR_EP_NOTPRESETNT;

    zclAttrInfo_t *pAttrEntry;
    pAttrEntry = zcl_findAttribute(cluster->endpoint, ZCL_CLUSTER_GEN_ON_OFF, ZCL_ATTRID_ONOFF);
    zcl_sendReportCmd(cluster->endpoint, &dstEpInfo, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR,
                      ZCL_CLUSTER_GEN_ON_OFF, pAttrEntry->id, pAttrEntry->type, pAttrEntry->data);
    if (cluster->indicator_led != NULL)
    {
      pAttrEntry = zcl_findAttribute(cluster->endpoint, ZCL_CLUSTER_GEN_ON_OFF, ZCL_ATTRID_ONOFF_INDICATOR_STATE);
      zcl_sendReportCmd(cluster->endpoint, &dstEpInfo, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR,
                        ZCL_CLUSTER_GEN_ON_OFF, pAttrEntry->id, pAttrEntry->type, pAttrEntry->data);
    }
  }
}

void relay_cluster_on_relay_change(zigbee_relay_cluster *cluster, u8 state)
{
  // Always sync LED and report state when relay changes
  sync_indicator_led(cluster);
  relay_cluster_report(cluster);
  
  if (cluster->startup_mode == ZCL_START_UP_ONOFF_SET_ONOFF_TOGGLE ||
      cluster->startup_mode == ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS)
  {
    relay_cluster_store_attrs_to_nv(cluster);
  }
}

void relay_cluster_on_write_attr(zigbee_relay_cluster *cluster, zclWriteCmd_t *pWriteReqCmd)
{
  for (int index = 0; index < pWriteReqCmd->numAttr; index++)
  {
    if (pWriteReqCmd->attrList[index].attrID == ZCL_ATTRID_ONOFF_INDICATOR_STATE)
    {
      if (cluster->indicator_led->on)
      {
        led_on(cluster->indicator_led);
      }
      else
      {
        led_off(cluster->indicator_led);
      }
    }
  }
  if (cluster->indicator_led_mode != ZCL_ONOFF_INDICATOR_MODE_MANUAL)
  {
    sync_indicator_led(cluster);
  }

  relay_cluster_store_attrs_to_nv(cluster);
}

typedef struct
{
  u8 on_off;
  u8 startup_mode;
  u8 indicator_led_mode;
  u8 indicator_led_on;
} zigbee_relay_cluster_config;



zigbee_relay_cluster_config nv_config_buffer;

void relay_cluster_store_attrs_to_nv(zigbee_relay_cluster *cluster)
{
  nv_config_buffer.on_off             = cluster->relay->on;
  nv_config_buffer.startup_mode       = cluster->startup_mode;
  nv_config_buffer.indicator_led_mode = cluster->indicator_led_mode;
  nv_config_buffer.indicator_led_on   = cluster->indicator_led->on;

  nv_flashWriteNew(1, NV_MODULE_ZCL, NV_ITEM_ZCL_RELAY_CONFIG(cluster->endpoint), sizeof(zigbee_relay_cluster_config), (u8 *)&nv_config_buffer);
}

void relay_cluster_load_attrs_from_nv(zigbee_relay_cluster *cluster)
{
  nv_sts_t st = nv_flashReadNew(1, NV_MODULE_ZCL, NV_ITEM_ZCL_RELAY_CONFIG(cluster->endpoint), sizeof(zigbee_relay_cluster_config), (u8 *)&nv_config_buffer);

  if (st != NV_SUCC)
  {
    return;
  }
  cluster->startup_mode       = nv_config_buffer.startup_mode;
  cluster->indicator_led_mode = nv_config_buffer.indicator_led_mode;
}

void relay_cluster_handle_startup_mode(zigbee_relay_cluster *cluster)
{
  nv_sts_t st = nv_flashReadNew(1, NV_MODULE_ZCL, NV_ITEM_ZCL_RELAY_CONFIG(cluster->endpoint), sizeof(zigbee_relay_cluster_config), (u8 *)&nv_config_buffer);

  if (st != NV_SUCC)
  {
    return;
  }

  u8 prev_on = nv_config_buffer.on_off;
  if (cluster->startup_mode == ZCL_START_UP_ONOFF_SET_ONOFF_TO_OFF)
  {
    relay_cluster_off(cluster);
  }
  if (cluster->startup_mode == ZCL_START_UP_ONOFF_SET_ONOFF_TO_ON)
  {
    relay_cluster_on(cluster);
  }
  if (cluster->startup_mode == ZCL_START_UP_ONOFF_SET_ONOFF_TOGGLE)
  {
    if (prev_on)
    {
      relay_cluster_off(cluster);
    }
    else
    {
      relay_cluster_on(cluster);
    }
  }
  if (cluster->startup_mode == ZCL_START_UP_ONOFF_SET_ONOFF_TO_PREVIOUS)
  {
    if (prev_on)
    {
      relay_cluster_on(cluster);
    }
    else
    {
      relay_cluster_off(cluster);
    }
  }

  // Restore indicator led
  if (cluster->indicator_led != NULL)
  {
    if (cluster->indicator_led_mode == ZCL_ONOFF_INDICATOR_MODE_MANUAL)
    {
      if (nv_config_buffer.indicator_led_on)
      {
        led_on(cluster->indicator_led);
      }
      else
      {
        led_off(cluster->indicator_led);
      }
    }
  }
}

void relay_cluster_init_sequence_tracking(zigbee_relay_cluster *cluster)
{
  // Initialize all sequence trackers as invalid
  for (u8 i = 0; i < MAX_SEQ_TRACKERS; i++)
  {
    cluster->seq_trackers[i].srcAddr = 0;
    cluster->seq_trackers[i].lastSeqNum = 0;
    cluster->seq_trackers[i].isValid = 0;
    cluster->seq_trackers[i].lastTimestamp = 0;
  }
  printf("Initialized sequence tracking for endpoint %d\r\n", cluster->endpoint);
}

bool relay_cluster_check_sequence(zigbee_relay_cluster *cluster, u16 srcAddr, u8 seqNum)
{

  // Never do sequence tracking for coordinator (address 0x0000)
  if (srcAddr == 0x0000) {
    return true;
  }

  // Get current time in seconds using millis()
  u32 now = millis() / 1000;

  // Allow sequence number 0 for safety (fresh starts, resets, etc.)
  if (seqNum == 0)
  {
  printf("Allowing low sequence number %d from addr %d (safety)\r\n", seqNum, srcAddr);
    relay_cluster_update_sequence_tracker(cluster, srcAddr, seqNum);
    return true;
  }

  // Find existing tracker for this source address
  for (u8 i = 0; i < MAX_SEQ_TRACKERS; i++)
  {
    if (cluster->seq_trackers[i].isValid && cluster->seq_trackers[i].srcAddr == srcAddr)
    {
      u8 lastSeq = cluster->seq_trackers[i].lastSeqNum;
      u32 lastTs = cluster->seq_trackers[i].lastTimestamp;
      bool isNewer = false;
      // Accept if 15s have passed since last message from this device
      if (now && lastTs && (now - lastTs >= 15)) {
  printf("Accepting message from addr %d due to timeout (now=%d, last=%d)\r\n", srcAddr, (int)now, (int)lastTs);
        cluster->seq_trackers[i].lastSeqNum = seqNum;
        cluster->seq_trackers[i].lastTimestamp = now;
        return true;
      }
      // Handle rollover (255 -> 0)
      if (seqNum > lastSeq)
      {
        isNewer = true;
      }
      else if (seqNum < lastSeq)
      {
        if ((lastSeq - seqNum) > 5)
        {
          isNewer = true;
        }
      }
      // If seqNum == lastSeq, it's a duplicate (isNewer stays false)
      if (isNewer)
      {
  printf("Accept seq %d from addr %d (prev: %d)\r\n", seqNum, srcAddr, lastSeq);
  printf("Current time: %d\r\n", (int)now);
        cluster->seq_trackers[i].lastSeqNum = seqNum;
        cluster->seq_trackers[i].lastTimestamp = now;
        return true;
      }
      else
      {
  printf("Rejecting old/duplicate sequence %d from addr %d (last: %d)\r\n", seqNum, srcAddr, lastSeq);
        return false;
      }
    }
  }

  // Source address not found, add it to a free slot
  for (u8 i = 0; i < MAX_SEQ_TRACKERS; i++)
  {
    if (!cluster->seq_trackers[i].isValid)
    {
      cluster->seq_trackers[i].srcAddr = srcAddr;
      cluster->seq_trackers[i].lastSeqNum = seqNum;
      cluster->seq_trackers[i].isValid = 1;
      cluster->seq_trackers[i].lastTimestamp = now;
  printf("Added new source addr %d with seq %d to tracker slot %d\r\n", srcAddr, seqNum, i);
      return true;
    }
  }

  // No free slots, replace the first one (simple FIFO replacement)
  cluster->seq_trackers[0].srcAddr = srcAddr;
  cluster->seq_trackers[0].lastSeqNum = seqNum;
  cluster->seq_trackers[0].isValid = 1;
  cluster->seq_trackers[0].lastTimestamp = now;
  printf("Replaced tracker slot 0 with addr %d, seq %d\r\n", srcAddr, seqNum);
  return true;
}

void relay_cluster_update_sequence_tracker(zigbee_relay_cluster *cluster, u16 srcAddr, u8 seqNum)
{
  // Never do sequence tracking for coordinator (address 0x0000)
  if (srcAddr == 0x0000) {
    return;
  }

  u32 now = millis() / 1000;

  // Find existing tracker for this source address
  for (u8 i = 0; i < MAX_SEQ_TRACKERS; i++)
  {
    if (cluster->seq_trackers[i].isValid && cluster->seq_trackers[i].srcAddr == srcAddr)
    {
      cluster->seq_trackers[i].lastSeqNum = seqNum;
      cluster->seq_trackers[i].lastTimestamp = now;
      return;
    }
  }

  // Source address not found, add it to a free slot
  for (u8 i = 0; i < MAX_SEQ_TRACKERS; i++)
  {
    if (!cluster->seq_trackers[i].isValid)
    {
      cluster->seq_trackers[i].srcAddr = srcAddr;
      cluster->seq_trackers[i].lastSeqNum = seqNum;
      cluster->seq_trackers[i].isValid = 1;
      cluster->seq_trackers[i].lastTimestamp = now;
      return;
    }
  }

  // No free slots, replace the first one
  cluster->seq_trackers[0].srcAddr = srcAddr;
  cluster->seq_trackers[0].lastSeqNum = seqNum;
  cluster->seq_trackers[0].isValid = 1;
  cluster->seq_trackers[0].lastTimestamp = now;
}

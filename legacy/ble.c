#include "ble.h"

#include "layout.h"
#include "rtt_log.h"
#include "sys.h"
#include "timer.h"
#include "usart.h"

// static usart_msg ble_usart_msg;
static usart_test_msg test_msg;

static bool get_ble_name = false;
static bool get_ble_ver = false;
static bool get_ble_battery = false;
static bool ble_connect = false;
static bool ble_switch = true;
static bool get_ble_switch = false;
static uint8_t get_test_state = 0;
static char ble_name[BLE_NAME_LEN + 1] = {0};
static char ble_ver[6] = {0};
static uint8_t read_status = UARTSTATE_IDLE;

static uint8_t calXor(uint8_t *buf, uint32_t len) {
  uint8_t tmp = 0;
  uint32_t i;
  for (i = 0; i < len; i++) {
    tmp ^= buf[i];
  }
  return tmp;
}

static void ble_cmd_packet(uint8_t *value, uint8_t value_len) {
  uint8_t cmd[64] = {0};
  cmd[0] = 0x5a;
  cmd[1] = 0xa5;
  cmd[2] = ((value_len + 1) >> 8) & 0xff;
  cmd[3] = (value_len + 1) & 0xff;
  memcpy(cmd + 4, value, value_len);
  cmd[value_len + 4] = calXor(cmd, value_len + 4);
  ble_usart_send(cmd, value_len + 5);
}

void ble_request_info(uint8_t type) {
  uint8_t cmd[64] = {0};
  cmd[0] = type;
  cmd[1] = 0x01;
  cmd[2] = 0x01;
  ble_cmd_packet(cmd, 3);
}

void ble_ctl_onoff(void) {
  uint8_t cmd[64] = {0};
  cmd[0] = BLE_CMD_ONOFF_BLE;
  cmd[1] = 0x01;
  cmd[2] = 0x03;
  ble_cmd_packet(cmd, 0x03);
}

void change_ble_sta(uint8_t mode) {
  uint8_t cmd[64] = {0};
  cmd[0] = BLE_CMD_ONOFF_BLE;
  cmd[1] = 0x01;
  cmd[2] = mode;
  ble_cmd_packet(cmd, 0x03);
}

bool ble_connect_state(void) { return ble_connect; }
bool ble_name_state(void) { return get_ble_name; }
bool ble_ver_state(void) { return get_ble_ver; }
bool ble_battery_state(void) { return get_ble_battery; }
bool ble_switch_state(void) { return get_ble_switch; }
bool test_get_state(void) { return get_test_state; }
char *ble_get_name(void) { return ble_name; }
char *ble_get_ver(void) { return ble_ver; }

void ble_set_switch(bool flag) { ble_switch = flag; }
bool ble_get_switch(void) { return ble_switch; }

void ble_reset(void) {
  ble_power_off();
  delay_ms(100);
  ble_power_on();
}

void resp_host_ctl(uint8_t type) {
  uint8_t cmd[2] = {0};
  cmd[0] = 0x5A;
  cmd[1] = type;
  ble_usart_send(cmd, 2);
}
void ctl_device(void) {
  // static uint8_t status[2] = {0};
  // if ((status[0] != read_status)) {
  //   status[0] = read_status;
  //   status[1] = get_test_state;
  //   ble_usart_send(status, 2);
  // }
#if 1
  switch (get_test_state) {
    case CTL_DEVICE_CANCEL:
      ctl_device_c_power_on();
      delay_ms(200);
      ctl_device_c_power_off();
      resp_host_ctl(CTL_DEVICE_CANCEL);
      get_test_state = 0;
      break;
    case CTL_DEVICE_UP:
      ctl_device_up_on();
      delay_ms(200);
      ctl_device_up_off();
      resp_host_ctl(CTL_DEVICE_UP);
      get_test_state = 0;
      break;
    case CTL_DEVICE_DOWN:
      ctl_device_down_on();
      delay_ms(200);
      ctl_device_down_off();
      resp_host_ctl(CTL_DEVICE_DOWN);
      get_test_state = 0;
      break;
    case CTL_DEVICE_OK:
      ctl_device_ok_on();
      delay_ms(200);
      ctl_device_ok_off();
      resp_host_ctl(CTL_DEVICE_OK);
      get_test_state = 0;
      break;
    case CTL_DEVICE_POWER:
      ctl_device_c_power_on();
      delay_ms(2000);
      ctl_device_c_power_off();
      resp_host_ctl(CTL_DEVICE_POWER);
      get_test_state = 0;
      break;
      ///////////////////////
    case CANCEL_PRESS:
      ctl_device_c_power_on();
      resp_host_ctl(CANCEL_PRESS);
      get_test_state = 0;
      break;
    case CACEL_LIFT:
      ctl_device_c_power_off();
      resp_host_ctl(CACEL_LIFT);
      get_test_state = 0;
      break;
    case UP_PRESS:
      ctl_device_up_on();
      resp_host_ctl(UP_PRESS);
      get_test_state = 0;
      break;
    case UP_LIFT:
      ctl_device_up_off();
      resp_host_ctl(UP_LIFT);
      get_test_state = 0;
      break;
    case DOWN_PRESS:
      ctl_device_down_on();
      resp_host_ctl(DOWN_PRESS);
      get_test_state = 0;
      break;
    case DOWN_LIFT:
      ctl_device_down_off();
      resp_host_ctl(DOWN_LIFT);
      get_test_state = 0;
      break;
    case OK_PRESS:
      ctl_device_ok_on();
      resp_host_ctl(OK_PRESS);
      get_test_state = 0;
      break;
    case OK_LIFT:
      ctl_device_ok_off();
      resp_host_ctl(OK_LIFT);
      get_test_state = 0;
      break;
    default:

      break;
  }
#endif
}
void ble_uart_poll(void) {
  static uint8_t buf[128] = {0};
  static uint8_t index = 0;
  if (ble_read_byte(buf + index) == false) {
    return;
  }
  index++;
  if (index > sizeof(buf)) {
    index = 0;
    read_status = UARTSTATE_IDLE;
    return;
  }
  if (read_status == UARTSTATE_IDLE) {
    if (index >= 2) {
      if ((buf[0] != 0x5A) && ((buf[1] != 0x5A))) {
        index = 0;
        return;
      }
      read_status = UARTSTATE_READ_LEN;
    }
  } else if (read_status == UARTSTATE_READ_LEN) {
    if (index >= 4) {
      test_msg.len = (buf[2] << 8) + buf[3];
      if (test_msg.len != 2) {
        index = 0;
        read_status = UARTSTATE_IDLE;
        return;
      }
      read_status = UARTSTATE_READ_DATA;
    }
  } else if (read_status == UARTSTATE_READ_DATA) {
    if (index == test_msg.len + 4) {
      test_msg.type = buf[4];
      test_msg.cmd = buf[5];
      read_status = UARTSTATE_READ_FINISHED;
    }
  }
  if (read_status == UARTSTATE_READ_FINISHED) {
    index = 0;
    read_status = UARTSTATE_IDLE;
    switch (test_msg.type) {
      case 0x01:  //功能按键
        switch (test_msg.cmd) {
          case CTL_DEVICE_CANCEL:  //取消
            get_test_state = CTL_DEVICE_CANCEL;
            break;
          case CTL_DEVICE_UP:  //向上
            get_test_state = CTL_DEVICE_UP;
            break;
          case CTL_DEVICE_DOWN:  //向下
            get_test_state = CTL_DEVICE_DOWN;
            break;
          case CTL_DEVICE_OK:  //确认
            get_test_state = CTL_DEVICE_OK;
            break;
          case CTL_DEVICE_POWER:  //重启
            get_test_state = CTL_DEVICE_POWER;

          case CANCEL_PRESS:
            get_test_state = CANCEL_PRESS;
            break;
          case CACEL_LIFT:
            get_test_state = CACEL_LIFT;
            break;
          case UP_PRESS:
            get_test_state = UP_PRESS;
            break;
          case UP_LIFT:
            get_test_state = UP_LIFT;
            break;
          case DOWN_PRESS:
            get_test_state = DOWN_PRESS;
            break;
          case DOWN_LIFT:
            get_test_state = DOWN_LIFT;
            break;
          case OK_PRESS:
            get_test_state = OK_PRESS;
            break;
          case OK_LIFT:
            get_test_state = OK_LIFT;
            break;
          default:
            break;
        }
        break;
      case 0x02:

        break;

      default:
        break;
    }
#if 0
    switch (ble_usart_msg.cmd) {
      case BLE_CMD_CONNECT_STATE:
      case BLE_CMD_PAIR_STATE:
        if (ble_usart_msg.cmd_vale[0] == 0x01)
          ble_connect = true;
        else
          ble_connect = false;
        layoutRefreshSet(true);
        break;
      case BLE_CMD_PASSKEY:
        if (ble_usart_msg.cmd_len == 0x06) {
          memcpy(passkey, ble_usart_msg.cmd_vale, 6);
          layoutBlePasskey(passkey);
        }
        break;
      case BLE_CMD_BT_NAME:
        if (ble_usart_msg.cmd_len == BLE_NAME_LEN) {
          memcpy(ble_name, ble_usart_msg.cmd_vale, BLE_NAME_LEN);
          get_ble_name = true;
          layoutRefreshSet(true);
        }
        break;
      case BLE_CMD_BATTERY:
        get_ble_battery = true;
        if (ble_usart_msg.cmd_vale[0] <= 5)
          battery_cap = ble_usart_msg.cmd_vale[0];
        break;
      case BLE_CMD_VER:
        if (ble_usart_msg.cmd_len == 5) {
          memcpy(ble_ver, ble_usart_msg.cmd_vale, 5);
          get_ble_ver = true;
        }
        break;
      case BLE_CMD_ONOFF_BLE:
        get_ble_switch = true;
        if (ble_usart_msg.cmd_vale[0] == 0) {
          ble_switch = false;
        } else {
          ble_switch = true;
        }
        break;
      case BLE_CMD_DFU_STA:
        if (ble_usart_msg.cmd_vale[0]) {
          layoutDfuStatus(ble_usart_msg.cmd_vale[0]);
        }
        break;
      default:
        break;
    }
#endif
  }
}

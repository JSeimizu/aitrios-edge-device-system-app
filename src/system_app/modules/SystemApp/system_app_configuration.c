/*
* SPDX-FileCopyrightText: 2024-2025 Sony Semiconductor Solutions Corporation
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <stdio.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "evp/sdk_sys.h"

#include "network_manager.h"
#include "clock_manager.h"
#include "clock_manager_setting.h"
#include "system_manager.h"
#include "firmware_manager.h"
#include "json/include/json.h"

#include "system_app_log.h"
#include "system_app_common.h"
#include "system_app_deploy.h"
#include "system_app_state.h"
#include "system_app_configuration.h"
#include "system_app_led.h"
#include "system_app_timer.h"
#include "system_app_util.h"

//
// Macros.
//

//
// File private structure and enum.
//

typedef enum { IPvInvalid = -1, IPv4 = 1, IPv6 = 2 } IpVer;

//
// File static variables.
//

STATIC struct SYS_client* s_sys_client = NULL;

//
// File static private functions.
//

/*----------------------------------------------------------------------*/
static int ConvertFilterNameToValue(const char* filer_name)
{
    int filter = -1;

    if (strncmp(filer_name, "all", CFGST_LOG_FILTER_LEN) == 0) {
        filter = AllLog;
    }
    else if (strncmp(filer_name, "main", CFGST_LOG_FILTER_LEN) == 0) {
        filter = MainFwLog;
    }
    else if (strncmp(filer_name, "sensor", CFGST_LOG_FILTER_LEN) == 0) {
        filter = SensorLog;
    }
    else if (strncmp(filer_name, "companion_fw", CFGST_LOG_FILTER_LEN) == 0) {
        filter = CompanionFwLog;
    }
    else if (strncmp(filer_name, "companion_app", CFGST_LOG_FILTER_LEN) == 0) {
        filter = CompanionAppLog;
    }
    else {
        // filter = -1;
    }

    return filter;
}

/*----------------------------------------------------------------------*/
static IpVer CheckIpAddressType(const char* ip_string)
{
    int inet_ret = 0;

#if 0
  // Accept empty string.

  if (ip_string[0] == '\0') {
    return 0; // Empty.
  }
#endif

    // Check ip_string describes IPv4 or not.

    struct in_addr ipv4;

    inet_ret = inet_pton(AF_INET, ip_string, &ipv4);

    if (inet_ret == 1) {
        return IPv4; // IPv4.
    }
    // Check ip_string describes IPv6 or not.

    struct in6_addr ipv6;

    inet_ret = inet_pton(AF_INET6, ip_string, &ipv6);

    if (inet_ret == 1) {
        return IPv6; // IPv6.
    }

    return IPvInvalid;
}

/*----------------------------------------------------------------------*/
static bool CheckUpdateNumber(uint32_t topic, uint32_t type, int number)
{
    bool update = false;
    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;

    if (topic == ST_TOPIC_SYSTEM_SETTINGS) {
        if (type == TemperatureUpdateInterval) {
            int interval = 0;
            SysAppStateGetTemperatureUpdateInterval(&interval);
            update = (number != interval) ? true : false;
        }
        else {
        }
    }
    else if (topic == ST_TOPIC_NETWORK_SETTINGS) {
        if (type == IpMethod) {
            EsfNetworkManagerParameterMask esfnm_mask = {0};
            EsfNetworkManagerParameter esfnm_param = {0};
            esfnm_mask.normal_mode.ip_method = 1;

            esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

            if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                update = (number != esfnm_param.normal_mode.ip_method) ? true : false;
            }
        }
        else if (type == ProxyPort) {
            EsfNetworkManagerParameterMask esfnm_mask = {0};
            EsfNetworkManagerParameter esfnm_param = {0};
            esfnm_mask.proxy.port = 1;

            esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

            if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                update = (number != esfnm_param.proxy.port) ? true : false;
            }
        }
        else {
        }
    }
    else if (topic == ST_TOPIC_WIRELESS_SETTING) {
        if (type == StaEncryption) {
            EsfNetworkManagerParameterMask esfnm_mask = {0};
            EsfNetworkManagerParameter esfnm_param = {0};
            esfnm_mask.normal_mode.wifi_sta.encryption = 1;

            esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

            if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                update = (number != esfnm_param.normal_mode.wifi_sta.encryption) ? true : false;
            }
        }
        else {
        }
    }
    else if (topic == ST_TOPIC_PERIODIC_SETTING) {
        if (type == OperationMode) {
            update = true; /*T.B.D*/
        }
        else if (type == RecoveryMethod) {
            update = true; /*T.B.D*/
        }
        else {
        }
    }
    else if (topic == ST_TOPIC_UPLOAD_SENSOR_CALIBRATION_PARAM) {
    }
    else if (topic == ST_TOPIC_DEPLOY_FIRMWARE) {
    }
    else if (topic == ST_TOPIC_DEPLOY_AI_MODEL) {
    }
    else if (topic == ST_TOPIC_DEPLOY_SENSOR_CALIBRATION_PARAM) {
    }
    else {
    }

    return update;
}

/*----------------------------------------------------------------------*/
static bool CheckUpdateNumberWithIdx(uint32_t topic, uint32_t type, int idx, int number)
{
    bool update = false;
    RetCode log_ret = kRetOk;

    if (topic == ST_TOPIC_SYSTEM_SETTINGS) {
        if (type == LogLevel) {
            CfgStLogLevel level;

            log_ret = SysAppLogGetParameterNumber(idx, LogLevel, (int*)&level);

            if (log_ret == kRetOk) {
                update = (number != (int)level) ? true : false;
                SYSAPP_DBG("CheckUpdateNumberWithIdx LogLevel %d : %d", number, level);
            }
            else {
                SYSAPP_ERR("SysAppLogGetParameterNumber(dlog_level) ret %d", log_ret);
            }
        }
        else if (type == LogDestination) {
            CfgStLogDestination destination;

            log_ret = SysAppLogGetParameterNumber(idx, LogDestination, (int*)&destination);

            if (log_ret == kRetOk) {
                update = (number != (int)destination) ? true : false;
                SYSAPP_DBG("CheckUpdateNumberWithIdx LogDestination %d : %d", number, destination);
            }
            else {
                SYSAPP_ERR("SysAppLogGetParameterNumber(dlog_dest) ret %d", log_ret);
            }
        }
        else {
            /* for other type items */
        }
    }
    else if (topic == ST_TOPIC_PERIODIC_SETTING) {
        if (type == CaptureInterval) {
            update = true; /*T.B.D*/
        }
        else if (type == ConfigInterval) {
            update = true; /*T.B.D*/
        }
        else {
        }
    }
    else {
        /* for other topics */
    }

    return update;
}

/*----------------------------------------------------------------------*/
static bool CheckUpdateBoolean(uint32_t topic, uint32_t type, bool boolean)
{
    bool update = false;

    if (topic == ST_TOPIC_SYSTEM_SETTINGS) {
        if (type == LedEnabled) {
            bool led_val = true;

            RetCode ret = SysAppLedGetEnable(&led_val);

            if (ret == kRetOk) {
                update = ((boolean != led_val) ? true : false);
            }
            else {
                SYSAPP_ERR("SysAppLedGetEnable() ret %d", ret);
            }
        }
        else {
        }
    }
    else if (topic == ST_TOPIC_NETWORK_SETTINGS) {
    }
    else if (topic == ST_TOPIC_WIRELESS_SETTING) {
    }
    else if (topic == ST_TOPIC_PERIODIC_SETTING) {
    }
    else if (topic == ST_TOPIC_ENDPOINT_SETTINGS) {
    }
    else if (topic == ST_TOPIC_UPLOAD_SENSOR_CALIBRATION_PARAM) {
    }
    else if (topic == ST_TOPIC_DEPLOY_FIRMWARE) {
    }
    else if (topic == ST_TOPIC_DEPLOY_AI_MODEL) {
    }
    else if (topic == ST_TOPIC_DEPLOY_SENSOR_CALIBRATION_PARAM) {
    }
    else {
    }

    return update;
}

/*----------------------------------------------------------------------*/
static bool CheckUpdateString(uint32_t topic, uint32_t type, const char* string)
{
    bool update = false;
    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;
    EsfSystemManagerResult esfsm_ret = kEsfSystemManagerResultOk;

    if (topic == ST_TOPIC_SYSTEM_SETTINGS) {
        if (type == Id) {
            char* req_id = SysAppStateGetReqId(topic);
            if (req_id != NULL) {
                update = (strncmp(string, req_id, CFG_RES_ID_LEN + 1) != 0);
            }
        }
    }
    else if (topic == ST_TOPIC_NETWORK_SETTINGS) {
        if (type == Id) {
            char* req_id = SysAppStateGetReqId(topic);
            if (req_id != NULL) {
                update = (strncmp(string, req_id, CFG_RES_ID_LEN + 1) != 0);
            }
        }
        else if (type == ProxyUrl) {
            EsfNetworkManagerParameterMask esfnm_mask = {0};
            EsfNetworkManagerParameter esfnm_param = {0};
            esfnm_mask.proxy.url = 1;

            esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

            if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                update = (strncmp(string, esfnm_param.proxy.url, CFGST_NETWORK_PROXY_URL_LEN) != 0);
            }
        }
        else if (type == ProxyUserName) {
            EsfNetworkManagerParameterMask esfnm_mask = {0};
            EsfNetworkManagerParameter esfnm_param = {0};
            esfnm_mask.proxy.username = 1;

            esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

            if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                update = (strncmp(string, esfnm_param.proxy.username,
                                  CFGST_NETWORK_PROXY_USER_NAME_LEN) != 0);
            }
        }
        else if (type == ProxyPassword) {
            EsfNetworkManagerParameterMask esfnm_mask = {0};
            EsfNetworkManagerParameter esfnm_param = {0};
            esfnm_mask.proxy.password = 1;

            esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

            if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                update = (strncmp(string, esfnm_param.proxy.password,
                                  CFGST_NETWORK_PROXY_PASSWORD_LEN) != 0);
            }
        }
        else if (type == NtpUrl) {
            EsfClockManagerParams cm_param = {0};
            EsfClockManagerReturnValue esfcm_ret = EsfClockManagerGetParams(&cm_param);

            if (esfcm_ret == kClockManagerSuccess) {
                update =
                    (strncmp(string, cm_param.connect.hostname, CFGST_NETOWRK_NTP_URL_LEN) != 0);
            }
        }
        else {
        }
    }
    else if (topic == ST_TOPIC_WIRELESS_SETTING) {
        if (type == Id) {
            char* req_id = SysAppStateGetReqId(topic);
            if (req_id != NULL) {
                update = (strncmp(string, req_id, CFG_RES_ID_LEN + 1) != 0);
            }
        }
        else if (type == StaSsid) {
            EsfNetworkManagerParameterMask esfnm_mask = {0};
            EsfNetworkManagerParameter esfnm_param = {0};
            esfnm_mask.normal_mode.wifi_sta.ssid = 1;

            esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

            if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                update = (strncmp(string, esfnm_param.normal_mode.wifi_sta.ssid,
                                  CFGST_WIRELESS_STA_SSID_LEN) != 0);
            }
        }
        else if (type == StaPassword) {
            EsfNetworkManagerParameterMask esfnm_mask = {0};
            EsfNetworkManagerParameter esfnm_param = {0};
            esfnm_mask.normal_mode.wifi_sta.password = 1;

            esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

            if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                update = (strncmp(string, esfnm_param.normal_mode.wifi_sta.password,
                                  CFGST_WIRELESS_STA_PASSWORD_LEN) != 0);
            }
        }
        else {
        }
    }
    else if (topic == ST_TOPIC_PERIODIC_SETTING) {
        if (type == Id) {
            char* req_id = SysAppStateGetReqId(topic);
            if (req_id != NULL) {
                update = (strncmp(string, req_id, CFG_RES_ID_LEN + 1) != 0);
            }
        }
        else if (type == IpAddrSetting) {
            update = true; /*T.B.D*/
        }
        else {
        }
    }
    else if (topic == ST_TOPIC_ENDPOINT_SETTINGS) {
        if (type == Id) {
            char* req_id = SysAppStateGetReqId(topic);
            if (req_id != NULL) {
                update = (strncmp(string, req_id, CFG_RES_ID_LEN + 1) != 0);
            }
        }
        else if (type == EndpointUrl) {
            char* endp_host = NULL;
            size_t endp_host_buf_size = ESF_SYSTEM_MANAGER_EVP_HUB_URL_MAX_SIZE;
            endp_host = (char*)malloc(endp_host_buf_size);
            if (endp_host != NULL) {
                esfsm_ret = EsfSystemManagerGetEvpHubUrl(endp_host, &endp_host_buf_size);

                if (esfsm_ret == kEsfSystemManagerResultOk) {
                    update =
                        (strncmp(string, endp_host, ESF_SYSTEM_MANAGER_EVP_HUB_URL_MAX_SIZE) != 0);
                }
                else {
                    SYSAPP_WARN("EsfSystemManagerGetEvpHubUrl() failed %d", esfsm_ret);
                }

                free(endp_host);
            }
        }
        else if (type == EndpointPort) {
            char* endp_port = NULL;
            size_t endp_port_buf_size = ESF_SYSTEM_MANAGER_EVP_HUB_PORT_MAX_SIZE;
            endp_port = (char*)malloc(endp_port_buf_size);
            if (endp_port != NULL) {
                esfsm_ret = EsfSystemManagerGetEvpHubPort(endp_port, &endp_port_buf_size);

                if (esfsm_ret == kEsfSystemManagerResultOk) {
                    update =
                        (strncmp(string, endp_port, ESF_SYSTEM_MANAGER_EVP_HUB_PORT_MAX_SIZE) != 0);
                }
                else {
                    SYSAPP_WARN("EsfSystemManagerGetEvpHubPort() failed %d", esfsm_ret);
                }

                free(endp_port);
            }
        }
        else if (type == ProtocolVersion) {
            char* protocol_version = SysAppStateGetProtocolVersion();
            update = (strncmp(string, protocol_version, CFGST_ENDPOINT_PROTOCOL_VERSION_LEN) != 0);
        }
        else {
        }
    }
    else if (topic == ST_TOPIC_UPLOAD_SENSOR_CALIBRATION_PARAM) {
    }
    else if (topic == ST_TOPIC_DEPLOY_FIRMWARE) {
    }
    else if (topic == ST_TOPIC_DEPLOY_AI_MODEL) {
    }
    else if (topic == ST_TOPIC_DEPLOY_SENSOR_CALIBRATION_PARAM) {
    }
    else {
    }

    return update;
}

/*----------------------------------------------------------------------*/
static bool CheckUpdateIpAddress(uint32_t type, const char* string, int ip_check)
{
    bool update = false;
    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;
    EsfNetworkManagerParameterMask esfnm_mask = {0};
    EsfNetworkManagerParameter esfnm_param = {0};

    if ((type == IpAddress) || (type == IpAddressV6)) {
        esfnm_mask.normal_mode.dev_ip.ip = 1;
        esfnm_mask.normal_mode.dev_ip_v6.ip = 1;

        esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);
        if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
            if (ip_check == IPv4) { // IPv4.
                update = (strncmp(string, esfnm_param.normal_mode.dev_ip.ip,
                                  CFGST_NETOWRK_IP_ADDRESS_LEN) != 0);
            }
            else if (ip_check == IPv6) { // IPv6.
                update = (strncmp(string, esfnm_param.normal_mode.dev_ip_v6.ip,
                                  CFGST_NETOWRK_IP_ADDRESS_LEN) != 0);
            }
        }
    }
    else if ((type == SubnetMask) || (type == SubnetMaskV6)) {
        esfnm_mask.normal_mode.dev_ip.subnet_mask = 1;
        esfnm_mask.normal_mode.dev_ip_v6.subnet_mask = 1;

        esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

        if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
            if (ip_check == IPv4) { // IPv4.
                update = (strncmp(string, esfnm_param.normal_mode.dev_ip.subnet_mask,
                                  CFGST_NETOWRK_SUBNET_MASK_LEN) != 0);
            }
            else if (ip_check == IPv6) { // IPv6.
                update = (strncmp(string, esfnm_param.normal_mode.dev_ip_v6.subnet_mask,
                                  CFGST_NETOWRK_SUBNET_MASK_LEN) != 0);
            }
        }
    }
    else if ((type == GatewayAddress) || (type == GatewayAddressV6)) {
        esfnm_mask.normal_mode.dev_ip.gateway = 1;
        esfnm_mask.normal_mode.dev_ip_v6.gateway = 1;

        esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

        if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
            if (ip_check == IPv4) { // IPv4.
                update = (strncmp(string, esfnm_param.normal_mode.dev_ip.gateway,
                                  CFGST_NETOWRK_GATEWAY_ADDRESS_LEN) != 0);
            }
            else if (ip_check == IPv6) { // IPv6.
                update = (strncmp(string, esfnm_param.normal_mode.dev_ip_v6.gateway,
                                  CFGST_NETOWRK_GATEWAY_ADDRESS_LEN) != 0);
            }
        }
    }
    else if ((type == DnsAddress) || (type == DnsAddressV6)) {
        esfnm_mask.normal_mode.dev_ip.dns = 1;
        esfnm_mask.normal_mode.dev_ip_v6.dns = 1;

        esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

        if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
            if (ip_check == IPv4) { // IPv4.
                update = (strncmp(string, esfnm_param.normal_mode.dev_ip.dns,
                                  CFGST_NETOWRK_DNS_ADDRESS_LEN) != 0);
            }
            else if (ip_check == IPv6) { // IPv6.
                update = (strncmp(string, esfnm_param.normal_mode.dev_ip_v6.dns,
                                  CFGST_NETOWRK_DNS_ADDRESS_LEN) != 0);
            }
        }
    }
    else {
        // No Type.
    }

    return update;
}

/*----------------------------------------------------------------------*/
static RetCode ExistStaticIPv4InFlash(void)
{
    /* Check if an IPv4 address is located on Flash */

    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;
    EsfNetworkManagerParameterMask esfnm_mask = {.normal_mode.dev_ip.ip = 1,
                                                 .normal_mode.dev_ip.subnet_mask = 1,
                                                 .normal_mode.dev_ip.gateway = 1,
                                                 .normal_mode.dev_ip.dns = 1};
    EsfNetworkManagerParameter* esfnm_param = calloc(1, sizeof(EsfNetworkManagerParameter));

    if (esfnm_param == NULL) {
        SYSAPP_ERR("calloc");
        return kRetFailed;
    }

    RetCode ret = kRetOk;

    esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, esfnm_param);

    if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
        if (CheckIpAddressType(esfnm_param->normal_mode.dev_ip.ip) != IPv4 ||
            CheckIpAddressType(esfnm_param->normal_mode.dev_ip.subnet_mask) != IPv4 ||
            CheckIpAddressType(esfnm_param->normal_mode.dev_ip.gateway) != IPv4 ||
            CheckIpAddressType(esfnm_param->normal_mode.dev_ip.dns) != IPv4) {
            SYSAPP_INFO("IPv4 address is not set.");
            ret = kRetFailed;
        }
        else {
            SYSAPP_INFO("IPv4 address is set.");
        }
    }
    else {
        SYSAPP_ERR("EsfNetworkManagerLoadParameter ret %d", esfnm_ret);
        ret = kRetFailed;
    }

    free(esfnm_param);

    return ret;
}

/*----------------------------------------------------------------------*/
static bool CheckUpdateStringWithIdx(uint32_t topic, uint32_t type, int idx, const char* string)
{
    bool update = false;
    RetCode log_ret = kRetOk;

    if (topic == ST_TOPIC_SYSTEM_SETTINGS) {
        if (type == LogStorageName) {
            char buff_name[CFGST_LOG_STORAGE_NAME_LEN + 1];

            log_ret = SysAppLogGetParameterString(idx, LogStorageName, buff_name,
                                                  sizeof(buff_name));

            if (log_ret == kRetOk) {
                update = (strncmp(string, buff_name, CFGST_LOG_STORAGE_NAME_LEN + 1) != 0);
                SYSAPP_DBG("CheckUpdateStringWithIdx LogStorageName %s : %s", string, buff_name);
            }
            else {
                SYSAPP_ERR("SysAppLogGetParameterString(storage_name) ret %d", log_ret);
            }
        }
        else if (type == LogPath) {
            char buff_path[CFGST_LOG_PATH_LEN + 1];

            log_ret = SysAppLogGetParameterString(idx, LogPath, buff_path, sizeof(buff_path));

            if (log_ret == kRetOk) {
                update = (strncmp(string, buff_path, CFGST_LOG_PATH_LEN + 1) != 0);
                SYSAPP_DBG("CheckUpdateStringWithIdx LogPath %s : %s", string, buff_path);
            }
            else {
                SYSAPP_ERR("SysAppLogGetParameterString(path) ret %d", log_ret);
            }
        }
        else {
            /* for other type items */
        }
    }
    else if (topic == ST_TOPIC_PERIODIC_SETTING) {
        if (type == BaseTime) {
            update = true; /*T.B.D*/
        }
        else {
        }
    }
    else {
        /* for other topics */
    }

    return update;
}

#ifndef CONFIG_EXTERNAL_SYSTEMAPP_ENABLE_SYSTEM_FUNCTION
/*----------------------------------------------------------------------*/
STATIC RetCode ProcessUnimplementedConfiguration(const char* topic, const char* param)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    EsfJsonErrorCode esfj_ret = kEsfJsonSuccess;

    // Open handle and set config parameters.

    esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen() failed %d", esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize() failed %d", esfj_ret);
        ret = kRetFailed;
        goto close_json_handle;
    }

    // Get req_id property.

    const char* req_id = NULL;
    ret = SysAppCmnGetReqId(esfj_handle, val, &req_id);

    if ((ret != kRetOk) || ((strnlen(req_id, (CFG_RES_ID_LEN + 1)) > CFG_RES_ID_LEN))) {
        req_id = "0";
    }

    // Request to send "unimplemented" state.

    ret = SysAppStateSendUnimplementedState(topic, req_id);

    if (ret != kRetOk) {
        SYSAPP_WARN("Send %s(unimplemented) failed %d", topic, ret);
    }

close_json_handle:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose() failed %d", esfj_ret);
    }

    return ret;
}
#endif // !CONFIG_EXTERNAL_SYSTEMAPP_ENABLE_SYSTEM_FUNCTION

/*----------------------------------------------------------------------*/
STATIC void ConfigurationCallback(struct SYS_client* client, const char* topic, const char* config,
                                  enum SYS_type_configuration type, enum SYS_callback_reason reason,
                                  void* userData)
{
    (void)userData;
    if ((topic == NULL) || (config == NULL)) {
        SYSAPP_ERR("ConfigurationCallback(%p, %p, x)", topic, config);
        return;
    }

    SYSAPP_INFO("Configuration callback (topic %s)", topic);
    SYSAPP_DBG("Payload %s", config);

#ifndef CONFIG_EXTERNAL_SYSTEMAPP_ENABLE_SYSTEM_FUNCTION
    if (SysAppStateIsUnimplementedTopic(topic)) {
        // Unimplemented topic on this device.
        ProcessUnimplementedConfiguration(topic, config);
        return;
    }
#endif // !CONFIG_EXTERNAL_SYSTEMAPP_ENABLE_SYSTEM_FUNCTION

    // Execute each configuration.

    if (strcmp(topic, "system_settings") == 0) {
        SysAppCfgSystemSettings((const char*)config);
    }
    else if (strcmp(topic, "network_settings") == 0) {
        SysAppCfgNetworkSettings((const char*)config);
    }
    else if (strcmp(topic, "periodic_setting") == 0) {
        SysAppCfgPeriodicSetting((const char*)config);
    }
    else if (strcmp(topic, "wireless_setting") == 0) {
        SysAppCfgWirelessSetting((const char*)config);
    }
    else if (strcmp(topic, "PRIVATE_endpoint_settings") == 0) {
        SysAppCfgEndpointSettings((const char*)config);
    }
    else if (strcmp(topic, "PRIVATE_deploy_firmware") == 0) {
        SysAppDeploy(topic, (const char*)config, strlen(config));
    }
    else if (strcmp(topic, "PRIVATE_deploy_ai_model") == 0) {
        SysAppDeploy(topic, (const char*)config, strlen(config));
    }
    else if (strcmp(topic, "PRIVATE_deploy_sensor_calibration_param") == 0) {
        SysAppDeploy(topic, (const char*)config, strlen(config));
    }
    else {
        // Do nothing.
    }
}

/*----------------------------------------------------------------------*/
static RetCode RegisterConfigurationCallback(void)
{
    enum SYS_result sys_ret;
    RetCode ret = kRetFailed;
    // Register system_settings
    sys_ret = SYS_set_configuration_cb(s_sys_client, "system_settings", ConfigurationCallback,
                                       SYS_CONFIG_ANY, NULL);
    if (sys_ret != SYS_RESULT_OK) {
        SYSAPP_ERR("SYS_set_configuration_cb(%p, %s, %p, %d, NULL) ret %d", s_sys_client,
                   "system_settings", ConfigurationCallback, SYS_CONFIG_ANY, sys_ret);
        goto exit;
    }
    // Register network_settings
    sys_ret = SYS_set_configuration_cb(s_sys_client, "network_settings", ConfigurationCallback,
                                       SYS_CONFIG_ANY, NULL);
    if (sys_ret != SYS_RESULT_OK) {
        SYSAPP_ERR("SYS_set_configuration_cb(%p, %s, %p, %d, NULL) ret %d", s_sys_client,
                   "network_settings", ConfigurationCallback, SYS_CONFIG_ANY, sys_ret);
        goto exit;
    }
    // Register periodic_setting
    sys_ret = SYS_set_configuration_cb(s_sys_client, "periodic_setting", ConfigurationCallback,
                                       SYS_CONFIG_ANY, NULL);
    if (sys_ret != SYS_RESULT_OK) {
        SYSAPP_ERR("SYS_set_configuration_cb(%p, %s, %p, %d, NULL) ret %d", s_sys_client,
                   "periodic_setting", ConfigurationCallback, SYS_CONFIG_ANY, sys_ret);
        goto exit;
    }
    // Register wireless_setting
    sys_ret = SYS_set_configuration_cb(s_sys_client, "wireless_setting", ConfigurationCallback,
                                       SYS_CONFIG_ANY, NULL);
    if (sys_ret != SYS_RESULT_OK) {
        SYSAPP_ERR("SYS_set_configuration_cb(%p, %s, %p, %d, NULL) ret %d", s_sys_client,
                   "wireless_setting", ConfigurationCallback, SYS_CONFIG_ANY, sys_ret);
        goto exit;
    }
    // Register PRIVATE_endpoint_settings
    sys_ret = SYS_set_configuration_cb(s_sys_client, "PRIVATE_endpoint_settings",
                                       ConfigurationCallback, SYS_CONFIG_ANY, NULL);
    if (sys_ret != SYS_RESULT_OK) {
        SYSAPP_ERR("SYS_set_configuration_cb(%p, %s, %p, %d, NULL) ret %d", s_sys_client,
                   "PRIVATE_endpoint_settings", ConfigurationCallback, SYS_CONFIG_ANY, sys_ret);
        goto exit;
    }
    // Register PRIVATE_deploy_firmware
    sys_ret = SYS_set_configuration_cb(s_sys_client, "PRIVATE_deploy_firmware",
                                       ConfigurationCallback, SYS_CONFIG_ANY, NULL);
    if (sys_ret != SYS_RESULT_OK) {
        SYSAPP_ERR("SYS_set_configuration_cb(%p, %s, %p, %d, NULL) ret %d", s_sys_client,
                   "PRIVATE_deploy_firmware", ConfigurationCallback, SYS_CONFIG_ANY, sys_ret);
        goto exit;
    }
    // Register PRIVATE_deploy_ai_model
    sys_ret = SYS_set_configuration_cb(s_sys_client, "PRIVATE_deploy_ai_model",
                                       ConfigurationCallback, SYS_CONFIG_ANY, NULL);
    if (sys_ret != SYS_RESULT_OK) {
        SYSAPP_ERR("SYS_set_configuration_cb(%p, %s, %p, %d, NULL) ret %d", s_sys_client,
                   "PRIVATE_deploy_ai_model", ConfigurationCallback, SYS_CONFIG_ANY, sys_ret);
        goto exit;
    }
    // All configuration callback register is ok
    ret = kRetOk;
exit:
    return ret;
}

/*----------------------------------------------------------------------*/
static RetCode ClearEnrollmentData(void)
{
    // Remove project id and tokn

    if (EsfSystemManagerSetProjectId("", 1) != kEsfSystemManagerResultOk) {
        SYSAPP_ERR("EsfSystemManagerSetProjectId");
    }

    if (EsfSystemManagerSetRegisterToken("", 1) != kEsfSystemManagerResultOk) {
        SYSAPP_ERR("EsfSystemManagerSetRegisterToken");
    }

    return kRetOk;
}

/*----------------------------------------------------------------------*/
static int LoadIpMethodFromEsf(void)
{
    EsfNetworkManagerParameterMask esfnm_mask = {0};
    EsfNetworkManagerParameter esfnm_param = {0};
    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;
    esfnm_mask.normal_mode.ip_method = 1;

    esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

    if (esfnm_ret != kEsfNetworkManagerResultSuccess) {
        esfnm_param.normal_mode.ip_method = 0;
    }
    return esfnm_param.normal_mode.ip_method;
}

/*----------------------------------------------------------------------*/
static char* LoadNetworkAddressFromEsf(char* addr_buf, int addr_buf_len, uint32_t type)
{
    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;
    EsfNetworkManagerParameter esfnm_param = {0};
    EsfNetworkManagerParameterMask esfnm_mask = {0};

    memset(&esfnm_mask, 0, sizeof(esfnm_mask));
    memset(&esfnm_param, 0, sizeof(esfnm_param));

    esfnm_mask.normal_mode.dev_ip_v6.ip = 1;
    esfnm_mask.normal_mode.dev_ip_v6.subnet_mask = 1;
    esfnm_mask.normal_mode.dev_ip_v6.gateway = 1;
    esfnm_mask.normal_mode.dev_ip_v6.dns = 1;
    esfnm_mask.normal_mode.dev_ip.ip = 1;
    esfnm_mask.normal_mode.dev_ip.subnet_mask = 1;
    esfnm_mask.normal_mode.dev_ip.gateway = 1;
    esfnm_mask.normal_mode.dev_ip.dns = 1;

    // Load ESF network parameters.

    esfnm_ret = EsfNetworkManagerLoadParameter(&esfnm_mask, &esfnm_param);

    if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
        if (type == IpAddressV6) {
            strncpy(addr_buf, esfnm_param.normal_mode.dev_ip_v6.ip, addr_buf_len);
        }
        else if (type == SubnetMaskV6) {
            strncpy(addr_buf, esfnm_param.normal_mode.dev_ip_v6.subnet_mask, addr_buf_len);
        }
        else if (type == GatewayAddressV6) {
            strncpy(addr_buf, esfnm_param.normal_mode.dev_ip_v6.subnet_mask, addr_buf_len);
        }
        else if (type == DnsAddressV6) {
            strncpy(addr_buf, esfnm_param.normal_mode.dev_ip_v6.dns, addr_buf_len);
        }
        else if (type == IpAddress) {
            strncpy(addr_buf, esfnm_param.normal_mode.dev_ip.ip, addr_buf_len);
        }
        else if (type == SubnetMask) {
            strncpy(addr_buf, esfnm_param.normal_mode.dev_ip.subnet_mask, addr_buf_len);
        }
        else if (type == GatewayAddress) {
            strncpy(addr_buf, esfnm_param.normal_mode.dev_ip.gateway, addr_buf_len);
        }
        else if (type == DnsAddress) {
            strncpy(addr_buf, esfnm_param.normal_mode.dev_ip.dns, addr_buf_len);
        }
    }
    else {
        addr_buf[0] = '\0';
    }

    return addr_buf;
}

/*----------------------------------------------------------------------*/
static bool IsValidDomain(const char* domain, int max_len)
{
    bool isalpha_flag = false;

    /* Length check : Depends on domain name rules. */

    int len = strnlen(domain, max_len);
    if ((len < CFGST_ENDPOINT_DOMAIN_LEN_MIN) || (len > 253)) {
        SYSAPP_ERR("Invalid Domain length %d", len);
        return false; // Domain length is invalid
    }

    /* Check labels position. */

    int dot_count = 0;
    int label_start = 0;
    int label_last = 0;

    for (int i = 0; i <= len; i++) {
        if ((domain[i] == '.') || (domain[i] == '\0')) {
            label_last = i;

            if (domain[i] == '.') {
                dot_count++;
            }

            /* Check label length. */

            int label_length = label_last - label_start;
            if ((label_length < CFGST_ENDPOINT_LABEL_LEN_MIN) ||
                (label_length > CFGST_ENDPOINT_LABEL_LEN_MAX)) {
                SYSAPP_ERR("Invalid URL label length %d", label_length);
                return false; // Invalid label length 1..63
            }

            /* Check label characters. 1-st char. */

            char char_1st = domain[label_start];
            if (!isalnum(char_1st)) {
                SYSAPP_ERR("Invalid URL label 1st char. '%c' Must be AlNum", char_1st);
                return false; // Invalid labal 1-st charactor.
            }

            /* Check label characters. Last char. */

            if (label_length >= 2) {
                char char_last = domain[label_last - 1];
                if (!isalnum(char_last)) {
                    SYSAPP_ERR("Invalid URL label last char. '%c' Must be AlNum", char_last);
                    return false; // Invalid label last character.
                }
            }

            /* Check label characters. Mid char. */

            if (label_length >= 3) {
                for (int j = 1; j < label_length - 1; j++) {
                    char char_mid = domain[label_start + j];
                    if ((!isalnum(char_mid)) && (char_mid != '-')) {
                        SYSAPP_ERR("Invalid URL label mid char. '%c' Must be AlNum or '-'",
                                   char_mid);
                        return false; // Invalid character
                    }
                }
            }

            /* Next label token */
            label_start = i + 1;
        }
        else if ((isalpha(domain[i])) || (domain[i] == '-')) {
            /* Treat "number only pattern" as error , for example, "777.888.999.000" and more. */

            isalpha_flag = true;
        }

    } /* for */

    if (dot_count == 0) {
        SYSAPP_ERR("URL format, not found '.' : %s", domain);
        return false; // No dot found
    }

    /* Check the last part. TLD: Top level domain. (Usually represents a country.) */

    const char* tld = strrchr(domain, '.') + 1;
    int tld_len = strnlen(tld, max_len);

    if (tld_len < 2) {
        SYSAPP_ERR("URL format, invalid TLD length : %s", tld);
        return false; // TLD length is invalid
    }

    if (isalpha_flag == false) {
        SYSAPP_ERR("Invalid domain format, but it may be IP address. : %s", domain);
        return false; // invalid IPv4
    }

    return true; // Domain is valid
}

/*----------------------------------------------------------------------*/
STATIC bool IsValidUrlOrIpAddress(const char* string, int max_len)
{
    IpVer vIpAddress = CheckIpAddressType(string);
    if (vIpAddress == IPv4) { /* Exclude IPv6 */
        return true;
    }

    if (IsValidDomain(string, max_len)) {
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------*/
STATIC bool IsValidUrlOrNullString(const char* domain, int max_len)
{
    /* "" is valid URL. */
    if (strncmp(domain, "", max_len) == 0) {
        return true;
    }

    return IsValidUrlOrIpAddress(domain, max_len);
}

//
// Public functions.
//

/*----------------------------------------------------------------------*/
RetCode SysAppCfgInitialize(struct SYS_client* sys_client)
{
    SYSAPP_INFO("Initialize Configuration block.");
    RetCode ret = kRetOk;

    // Check and save sys_client.

    if (sys_client == NULL) {
        return kRetFailed;
    }

    s_sys_client = sys_client;

    // Register configuration callback.

    if (RegisterConfigurationCallback() != kRetOk) {
        ret = kRetFailed;
        goto set_config_callback_error;
    }

    return ret;

    //
    // Error handling.
    //

set_config_callback_error:

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgFinalize(void)
{
    SYSAPP_INFO("Finalize Configuration block.");

    RetCode ret = kRetOk;

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgLog(const char* param)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    const uint32_t topic = ST_TOPIC_SYSTEM_SETTINGS;

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p)", esfj_handle);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p, %s, %p)", esfj_handle, param, &val);
        ret = kRetFailed;
        goto log_exit;
    }

    // Get filter property.

    const char* filter_str = NULL;
    int filter = 0;
    bool valid_filter = false;
    int extret = SysAppCmnExtractStringValue(esfj_handle, val, "filter", &filter_str);

    if (extret >= 0) {
        if (extret >= 1) {
            filter = ConvertFilterNameToValue(filter_str);

            if ((filter >= AllLog) && (filter < LogFilterNum)) {
                valid_filter = true;
            }
        }
    }

    if (valid_filter) {
        // Save filter property.

        SysAppStateUpdateNumberWithIdx(topic, LogFilter, filter, filter);

        // Set got properties to target filter index.
        // If AllLog, set to all target.

        uint32_t start, stop;

        if (filter == AllLog) {
            start = AllLog;
            stop = LogFilterNum - 1;
        }
        else {
            start = stop = filter;
        }

        for (uint32_t idx = start; idx <= stop; idx++) {
            // Get level property.

            int level = 0;
            extret = SysAppCmnExtractNumberValue(esfj_handle, val, "level", &level);

            if (extret >= 0) {
                if ((extret >= 1) && ((level >= CriticalLv) && (level < LogLevelNum))) {
                    if (idx == AllLog) {
                        /* if idx == 0 (Alllog) then always setting. */
                        SysAppStateUpdateNumberWithIdx(topic, LogLevel, level, idx);
                    }
                    else {
                        /* if idx != 0 (Each Apps) then check and update parameter. */
                        if (CheckUpdateNumberWithIdx(topic, LogLevel, idx, level)) {
                            RetCode log_ret = SysAppLogSetParameterNumber(idx, LogLevel, level);

                            if (log_ret == kRetOk) {
                                SysAppStateUpdateNumberWithIdx(topic, LogLevel, level, idx);
                            }
                            else {
                                SYSAPP_WARN("SysAppLogSetParameterNumber(dlog_level) %d", log_ret);
                                SysAppStateSetInternalErrorWithIdx(topic, LogLevel, idx);
                            }
                        }
                    }
                }
                else {
                    SYSAPP_WARN("Invalid log level %d", level);
                    SysAppStateSetInvalidArgErrorWithIdx(topic, LogLevel, idx);
                }
            }

            // Get destination property.

            int destination = 0;
            extret = SysAppCmnExtractNumberValue(esfj_handle, val, "destination", &destination);

            if (extret >= 0) {
                if ((extret >= 1) &&
                    ((destination >= DestUart) && (destination < LogDestinationNum))) {
                    if (idx == AllLog) {
                        /* if idx == 0 (Alllog) then always setting. */
                        SysAppStateUpdateNumberWithIdx(topic, LogDestination, destination, idx);
                    }
                    else {
                        /* if idx != 0 (Each Apps) then check and update parameter. */
                        if (CheckUpdateNumberWithIdx(topic, LogDestination, idx, destination)) {
                            RetCode log_ret = SysAppLogSetParameterNumber(idx, LogDestination,
                                                                          destination);

                            if (log_ret == kRetOk) {
                                SysAppStateUpdateNumberWithIdx(topic, LogDestination, destination,
                                                               idx);
                            }
                            else {
                                SYSAPP_WARN("SysAppLogSetParameterNumber(dlog_destination) %d",
                                            log_ret);
                                SysAppStateSetInternalErrorWithIdx(topic, LogDestination, idx);
                            }
                        }
                    }
                }
                else {
                    SYSAPP_WARN("Invalid log destination %d", destination);
                    SysAppStateSetInvalidArgErrorWithIdx(topic, LogDestination, idx);
                }
            }

            // Get storage_name property.

            const char* storage_name = NULL;
            extret = SysAppCmnExtractStringValue(esfj_handle, val, "storage_name", &storage_name);

            if (extret >= 0) {
                if ((extret >= 1) && (strnlen(storage_name, CFGST_LOG_STORAGE_NAME_LEN + 1) <=
                                      CFGST_LOG_STORAGE_NAME_LEN)) {
                    if (idx == AllLog) {
                        /* if idx == 0 (Alllog) then always setting. */
                        SysAppStateUpdateStringWithIdx(topic, LogStorageName, storage_name, idx);
                    }
                    else {
                        /* if idx != 0 (Each Apps) then check and update parameter. */
                        if (CheckUpdateStringWithIdx(topic, LogStorageName, idx, storage_name)) {
                            RetCode log_ret = SysAppLogSetParameterString(
                                idx, LogStorageName, storage_name, CFGST_LOG_STORAGE_NAME_LEN + 1);

                            if (log_ret == kRetOk) {
                                SysAppStateUpdateStringWithIdx(topic, LogStorageName, storage_name,
                                                               idx);
                            }
                            else {
                                SYSAPP_WARN("SysAppLogSetParameterString(storage_name) %d",
                                            log_ret);
                                if (log_ret == kRetParamError) {
                                    SysAppStateSetInvalidArgErrorWithIdx(topic, LogStorageName,
                                                                         idx);
                                }
                                else {
                                    SysAppStateSetInternalErrorWithIdx(topic, LogStorageName, idx);
                                }
                            }
                        }
                    }
                }
                else {
                    SYSAPP_WARN("Invalid storage_name");
                    SysAppStateSetInvalidArgErrorWithIdx(topic, LogStorageName, idx);
                }
            }

            // Get path property.

            const char* path = NULL;
            extret = SysAppCmnExtractStringValue(esfj_handle, val, "path", &path);

            if (extret >= 0) {
                if ((extret >= 1) &&
                    (strnlen(path, CFGST_LOG_PATH_LEN + 1) <= CFGST_LOG_PATH_LEN)) {
                    if (idx == AllLog) {
                        /* if idx == 0 (Alllog) then always setting. */
                        SysAppStateUpdateStringWithIdx(topic, LogPath, path, idx);
                    }
                    else {
                        /* if idx != 0 (Each Apps) then check and update parameter. */
                        if (CheckUpdateStringWithIdx(topic, LogPath, idx, path)) {
                            RetCode log_ret = SysAppLogSetParameterString(idx, LogPath, path,
                                                                          CFGST_LOG_PATH_LEN + 1);

                            if (log_ret == kRetOk) {
                                SysAppStateUpdateStringWithIdx(topic, LogPath, path, idx);
                            }
                            else {
                                SYSAPP_WARN("SysAppLogSetParameterString(path) %d", log_ret);
                                if (log_ret == kRetParamError) {
                                    SysAppStateSetInvalidArgErrorWithIdx(topic, LogPath, idx);
                                }
                                else {
                                    SysAppStateSetInternalErrorWithIdx(topic, LogPath, idx);
                                }
                            }
                        }
                    }
                }
                else {
                    SYSAPP_WARN("Invalid path");
                    SysAppStateSetInvalidArgErrorWithIdx(topic, LogPath, idx);
                }
            }
        }
    }
    else {
        SYSAPP_WARN("Invalid filter %d", filter);
        SysAppStateSetInvalidArgErrorWithIdx(topic, LogFilter, AllLog);
    }

log_exit:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p)", esfj_handle);
    }

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgSystemSettings(const char* param)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    const uint32_t topic = ST_TOPIC_SYSTEM_SETTINGS;

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p) ret %d", &esfj_handle, esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p) ret %d", esfj_handle, esfj_ret);
        ret = kRetFailed;
        goto system_settings_exit;
    }

    // Get req_id property.

    const char* req_id = NULL;
    ret = SysAppCmnGetReqId(esfj_handle, val, &req_id);

    if (ret == kRetOk) {
        if (strnlen(req_id, (CFG_RES_ID_LEN + 1)) <= CFG_RES_ID_LEN) {
            if (CheckUpdateString(topic, Id, req_id)) {
                SysAppStateUpdateString(topic, Id, req_id);
            }
        }
        else {
            SysAppStateUpdateString(topic, Id, "0");
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }
    else {
        SysAppStateUpdateString(topic, Id, "0");

        if (ret == kRetFailed) {
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }

    // Get led_enabled property.

    bool led_enabled = false;
    int extret = SysAppCmnExtractBooleanValue(esfj_handle, val, "led_enabled", &led_enabled);

    if (extret >= 0) {
        if (extret >= 1) {
            if (CheckUpdateBoolean(topic, LedEnabled, led_enabled)) {
                RetCode ledret = SysAppLedSetEnable(led_enabled);

                if (ledret == kRetOk) {
                    SysAppStateUpdateBoolean(topic, LedEnabled, led_enabled);
                }
                else {
                    SYSAPP_WARN("SysAppLedSetEnable() failed %d", ledret);
                    SysAppStateSetInternalError(topic, LedEnabled);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid led_enabled %d", led_enabled);
            SysAppStateSetInvalidArgError(topic, LedEnabled);
        }
    }

    // Get log_settings property.

    EsfJsonValue cval = ESF_JSON_VALUE_INVALID;
    esfj_ret = EsfJsonObjectGet(esfj_handle, val, "log_settings", &cval);

    if (esfj_ret == kEsfJsonSuccess) {
        EsfJsonValueType val_type;
        esfj_ret = EsfJsonValueTypeGet(esfj_handle, cval, &val_type);

        if ((val_type == kEsfJsonValueTypeArray) && (esfj_ret == kEsfJsonSuccess)) {
            int32_t num = EsfJsonArrayCount(esfj_handle, cval);

            for (int32_t idx = 0; idx < num; idx++) {
                EsfJsonValue ccval;

                esfj_ret = EsfJsonArrayGet(esfj_handle, cval, idx, &ccval);

                if (esfj_ret == kEsfJsonSuccess) {
                    const char* log_obj_str = NULL;
                    esfj_ret = EsfJsonSerialize(esfj_handle, ccval, &log_obj_str);

                    if ((log_obj_str != NULL) && (esfj_ret == kEsfJsonSuccess)) {
                        SysAppCfgLog(log_obj_str);

                        EsfJsonSerializeFree(esfj_handle);
                    }
                }
            }
        }
    }

    // Get temperature_update_interval property.

    int temperature_update_interval = 0;
    extret = SysAppCmnExtractNumberValue(esfj_handle, val, "temperature_update_interval",
                                         &temperature_update_interval);

    if (extret >= 0) {
        if ((extret >= 1) &&
            ((temperature_update_interval >= 10) && (temperature_update_interval <= 3600))) {
            if (CheckUpdateNumber(topic, TemperatureUpdateInterval, temperature_update_interval)) {
                ret = SysAppTimerUpdateTimer(SensorTempIntervalTimer, temperature_update_interval);

                if (ret == kRetOk) {
                    SysAppStateUpdateNumber(topic, TemperatureUpdateInterval,
                                            temperature_update_interval);
                }
                else {
                    SYSAPP_WARN("SysAppTimerUpdateTimer() failed %d", ret);
                    SysAppStateSetInternalError(topic, TemperatureUpdateInterval);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid temperature_update_interval %d", temperature_update_interval);
            SysAppStateSetInvalidArgError(topic, TemperatureUpdateInterval);
        }
    }

    // Request to send system_settings.

    ret = SysAppStateSendState(ST_TOPIC_SYSTEM_SETTINGS);

    if (ret != kRetOk) {
        SYSAPP_WARN("Send system_settings failed %d", ret);
    }

system_settings_exit:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p) ret %d", esfj_handle, esfj_ret);
    }

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgStaticSettings(const char* param, IpVer ip_ver)
{
    //
    // This function have to be called when StaticIP setting.
    //

    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;
    EsfNetworkManagerParameterMask esfnm_mask = {0};
    EsfNetworkManagerParameter esfnm_param = {0};
    const uint32_t topic = ST_TOPIC_NETWORK_SETTINGS;

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p) ret %d", esfj_handle, esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p) ret %d", esfj_handle, esfj_ret);
        goto network_static_exit;
    }

    IpVer ip_check = -1;
    NetworkSettingsProperty property = IpAddressV6;
    int ext_ret = 0;

    // Get ip_address property.

    property = (ip_ver == IPv4) ? IpAddress : IpAddressV6;
    char ip_address_esfload[CFGST_NETOWRK_IP_ADDRESS_LEN + 1] = "";
    const char* ip_address = ip_address_esfload;

    ext_ret = SysAppCmnExtractStringValue(esfj_handle, val, "ip_address", &ip_address);

    if (ext_ret != 0) {
        if (ext_ret < 0) {
            // If it is -1, read the current value. This process is necessary because
            // the current value may change to an invalid value when it is
            // Factory Reset, among other times.
            ip_address = LoadNetworkAddressFromEsf(ip_address_esfload, sizeof(ip_address_esfload),
                                                   property);
        }
        // Check and save ip_address property.
        ip_check = CheckIpAddressType(ip_address);
        if ((strnlen(ip_address, (CFGST_NETOWRK_IP_ADDRESS_LEN + 1)) <=
             CFGST_NETOWRK_IP_ADDRESS_LEN) &&
            (ip_check == ip_ver)) {
            if (CheckUpdateIpAddress(property, ip_address, ip_check)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));

                if (ip_check == IPv4) { // IPv4.
                    esfnm_mask.normal_mode.dev_ip.ip = 1;
                    snprintf(esfnm_param.normal_mode.dev_ip.ip,
                             sizeof(esfnm_param.normal_mode.dev_ip.ip), "%s", ip_address);
                }
                else if (ip_check == IPv6) { // IPv6.
                    esfnm_mask.normal_mode.dev_ip_v6.ip = 1;
                    snprintf(esfnm_param.normal_mode.dev_ip_v6.ip,
                             sizeof(esfnm_param.normal_mode.dev_ip_v6.ip), "%s", ip_address);
                }

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateString(topic, property, ip_address);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(.ip %s) ret %d", ip_address,
                                esfnm_ret);
                    SysAppStateSetInternalError(topic, property);
                    ret = kRetFailed;
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid ip_address %s", ip_address);
            SysAppStateSetInvalidArgError(topic, property);
            ret = kRetFailed;
        }
    }
    else {
        SYSAPP_WARN("Invalid ip_address %s", ip_address);
        SysAppStateSetInvalidArgError(topic, property);
        ret = kRetFailed;
    }

    // Get subnet_mask property.

    property = (ip_ver == IPv4) ? SubnetMask : SubnetMaskV6;
    char subnet_mask_esfload[CFGST_NETOWRK_SUBNET_MASK_LEN + 1] = "";
    const char* subnet_mask = subnet_mask_esfload;

    ext_ret = SysAppCmnExtractStringValue(esfj_handle, val, "subnet_mask", &subnet_mask);

    if (ext_ret != 0) {
        if (ext_ret < 0) {
            // If it is -1, read the current value. This process is necessary because
            // the current value may change to an invalid value when it is
            // Factory Reset, among other times.
            subnet_mask = LoadNetworkAddressFromEsf(subnet_mask_esfload,
                                                    sizeof(subnet_mask_esfload), property);
        }
        // Check and save subnet_mask property.
        ip_check = CheckIpAddressType(subnet_mask);
        if ((strnlen(subnet_mask, (CFGST_NETOWRK_SUBNET_MASK_LEN + 1)) <=
             CFGST_NETOWRK_SUBNET_MASK_LEN) &&
            (ip_check == ip_ver)) {
            if (CheckUpdateIpAddress(property, subnet_mask, ip_check)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));

                if (ip_check == IPv4) { // IPv4.
                    esfnm_mask.normal_mode.dev_ip.subnet_mask = 1;
                    snprintf(esfnm_param.normal_mode.dev_ip.subnet_mask,
                             sizeof(esfnm_param.normal_mode.dev_ip.subnet_mask), "%s", subnet_mask);
                }
                else if (ip_check == IPv6) { // IPv6.
                    esfnm_mask.normal_mode.dev_ip_v6.subnet_mask = 1;
                    snprintf(esfnm_param.normal_mode.dev_ip_v6.subnet_mask,
                             sizeof(esfnm_param.normal_mode.dev_ip_v6.subnet_mask), "%s",
                             subnet_mask);
                }

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateString(topic, property, subnet_mask);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(.subnet_mask %s) ret %d",
                                subnet_mask, esfnm_ret);
                    SysAppStateSetInternalError(topic, property);
                    ret = kRetFailed;
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid subnet %s", subnet_mask);
            SysAppStateSetInvalidArgError(topic, property);
            ret = kRetFailed;
        }
    }
    else {
        SYSAPP_WARN("Invalid subnet %s", subnet_mask);
        SysAppStateSetInvalidArgError(topic, property);
        ret = kRetFailed;
    }

    // Get gateway_address property.

    property = (ip_ver == IPv4) ? GatewayAddress : GatewayAddressV6;
    char gateway_address_esfload[CFGST_NETOWRK_GATEWAY_ADDRESS_LEN + 1] = "";
    const char* gateway_address = gateway_address_esfload;

    ext_ret = SysAppCmnExtractStringValue(esfj_handle, val, "gateway_address", &gateway_address);

    if (ext_ret != 0) {
        if (ext_ret < 0) {
            // If it is -1, read the current value. This process is necessary because
            // the current value may change to an invalid value when it is
            // Factory Reset, among other times.
            gateway_address = LoadNetworkAddressFromEsf(gateway_address_esfload,
                                                        sizeof(gateway_address_esfload), property);
        }
        // Check and save gateway_address property.
        ip_check = CheckIpAddressType(gateway_address);
        if ((strnlen(gateway_address, (CFGST_NETOWRK_GATEWAY_ADDRESS_LEN + 1)) <=
             CFGST_NETOWRK_GATEWAY_ADDRESS_LEN) &&
            (ip_check == ip_ver)) {
            if (CheckUpdateIpAddress(property, gateway_address, ip_check)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));

                if (ip_check == IPv4) { // IPv4.
                    esfnm_mask.normal_mode.dev_ip.gateway = 1;
                    snprintf(esfnm_param.normal_mode.dev_ip.gateway,
                             sizeof(esfnm_param.normal_mode.dev_ip.gateway), "%s", gateway_address);
                }
                else if (ip_check == IPv6) { // IPv6.
                    esfnm_mask.normal_mode.dev_ip_v6.gateway = 1;
                    snprintf(esfnm_param.normal_mode.dev_ip_v6.gateway,
                             sizeof(esfnm_param.normal_mode.dev_ip_v6.gateway), "%s",
                             gateway_address);
                }

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateString(topic, property, gateway_address);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(.gateway %s) ret %d",
                                gateway_address, esfnm_ret);
                    SysAppStateSetInternalError(topic, property);
                    ret = kRetFailed;
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid gateway %s", gateway_address);
            SysAppStateSetInvalidArgError(topic, property);
            ret = kRetFailed;
        }
    }
    else {
        SYSAPP_WARN("Invalid gateway %s", gateway_address);
        SysAppStateSetInvalidArgError(topic, property);
        ret = kRetFailed;
    }

    // Get dns_address property.

    property = (ip_ver == IPv4) ? DnsAddress : DnsAddressV6;
    char dns_address_esfload[CFGST_NETOWRK_DNS_ADDRESS_LEN + 1] = "";
    const char* dns_address = dns_address_esfload;

    ext_ret = SysAppCmnExtractStringValue(esfj_handle, val, "dns_address", &dns_address);

    if (ext_ret != 0) {
        if (ext_ret < 0) {
            // If it is -1, read the current value. This process is necessary because
            // the current value may change to an invalid value when it is
            // Factory Reset, among other times.
            dns_address = LoadNetworkAddressFromEsf(dns_address_esfload,
                                                    sizeof(dns_address_esfload), property);
        }
        // Check and save dns_address property.
        ip_check = CheckIpAddressType(dns_address);
        if ((strnlen(dns_address, (CFGST_NETOWRK_DNS_ADDRESS_LEN + 1)) <=
             CFGST_NETOWRK_DNS_ADDRESS_LEN) &&
            (ip_check == ip_ver)) {
            if (CheckUpdateIpAddress(property, dns_address, ip_check)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));

                if (ip_check == IPv4) { // IPv4.
                    esfnm_mask.normal_mode.dev_ip.dns = 1;
                    snprintf(esfnm_param.normal_mode.dev_ip.dns,
                             sizeof(esfnm_param.normal_mode.dev_ip.dns), "%s", dns_address);
                }
                else if (ip_check == IPv6) { // IPv6.
                    esfnm_mask.normal_mode.dev_ip_v6.dns = 1;
                    snprintf(esfnm_param.normal_mode.dev_ip_v6.dns,
                             sizeof(esfnm_param.normal_mode.dev_ip_v6.dns), "%s", dns_address);
                }

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateString(topic, property, dns_address);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(.dns %s) ret %d", dns_address,
                                esfnm_ret);
                    SysAppStateSetInternalError(topic, property);
                    ret = kRetFailed;
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid dns_address %s", dns_address);
            SysAppStateSetInvalidArgError(topic, property);
            ret = kRetFailed;
        }
    }
    else {
        SYSAPP_WARN("Invalid dns_address %s", dns_address);
        SysAppStateSetInvalidArgError(topic, property);
        ret = kRetFailed;
    }

network_static_exit:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p) ret %d", esfj_handle, esfj_ret);
    }

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgStaticSettingsIPv6(const char* param)
{
    return SysAppCfgStaticSettings(param, IPv6);
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgStaticSettingsIPv4(const char* param)
{
    return SysAppCfgStaticSettings(param, IPv4);
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgProxySettings(const char* param)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;
    EsfNetworkManagerParameterMask esfnm_mask = {0};
    EsfNetworkManagerParameter esfnm_param = {0};
    const uint32_t topic = ST_TOPIC_NETWORK_SETTINGS;

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p) ret %d", esfj_handle, esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p) ret %d", esfj_handle, esfj_ret);
        goto network_proxy_exit;
    }

    // Get proxy_url property.

    const char* proxy_url = NULL;
    int extret = SysAppCmnExtractStringValue(esfj_handle, val, "proxy_url", &proxy_url);

    if (extret >= 0) {
        if ((extret >= 1) &&
            (strnlen(proxy_url, (CFGST_NETWORK_PROXY_URL_LEN + 1)) <=
             CFGST_NETWORK_PROXY_URL_LEN) &&
            (IsValidUrlOrNullString(proxy_url, CFGST_NETWORK_PROXY_URL_LEN))) {
            if (CheckUpdateString(topic, ProxyUrl, proxy_url)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));
                esfnm_mask.proxy.url = 1;
                snprintf(esfnm_param.proxy.url, sizeof(esfnm_param.proxy.url), "%s", proxy_url);

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateString(topic, ProxyUrl, proxy_url);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(proxy_url) failed %d", esfnm_ret);
                    SysAppStateSetInternalError(topic, ProxyUrl);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid proxy_url");
            SysAppStateSetInvalidArgError(topic, ProxyUrl);
        }
    }

    // Get proxy_port property.

    int proxy_port = 0;
    extret = SysAppCmnExtractNumberValue(esfj_handle, val, "proxy_port", &proxy_port);

    if (extret >= 0) {
        if ((extret >= 1) && ((proxy_port >= 0) && (proxy_port <= 65535))) {
            if (CheckUpdateNumber(topic, ProxyPort, proxy_port)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));
                esfnm_mask.proxy.port = 1;
                esfnm_param.proxy.port = proxy_port;

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateNumber(topic, ProxyPort, proxy_port);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(proxy_port) failed %d", esfnm_ret);
                    SysAppStateSetInternalError(topic, ProxyPort);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid proxy_port %d", proxy_port);
            SysAppStateSetInvalidArgError(topic, ProxyPort);
        }
    }

    // Get proxy_user_name property.

    const char* proxy_user_name = NULL;
    extret = SysAppCmnExtractStringValue(esfj_handle, val, "proxy_user_name", &proxy_user_name);

    if (extret >= 0) {
        if ((extret >= 1) && (strnlen(proxy_user_name, (CFGST_NETWORK_PROXY_USER_NAME_LEN + 1)) <=
                              CFGST_NETWORK_PROXY_USER_NAME_LEN)) {
            if (CheckUpdateString(topic, ProxyUserName, proxy_user_name)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));
                esfnm_mask.proxy.username = 1;
                snprintf(esfnm_param.proxy.username, sizeof(esfnm_param.proxy.username), "%s",
                         proxy_user_name);

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateString(topic, ProxyUserName, proxy_user_name);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(proxy_user_name) failed %d",
                                esfnm_ret);
                    SysAppStateSetInternalError(topic, ProxyUserName);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid proxy_user_name");
            SysAppStateSetInvalidArgError(topic, ProxyUserName);
        }
    }

    // Get proxy_password property.

    const char* proxy_password = NULL;
    extret = SysAppCmnExtractStringValue(esfj_handle, val, "proxy_password", &proxy_password);

    if (extret >= 0) {
        if ((extret >= 1) && (strnlen(proxy_password, (CFGST_NETWORK_PROXY_PASSWORD_LEN + 1)) <=
                              CFGST_NETWORK_PROXY_PASSWORD_LEN)) {
            if (CheckUpdateString(topic, ProxyPassword, proxy_password)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));
                esfnm_mask.proxy.password = 1;
                snprintf(esfnm_param.proxy.password, sizeof(esfnm_param.proxy.password), "%s",
                         proxy_password);

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateString(topic, ProxyPassword, proxy_password);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(proxy_password) failed %d",
                                esfnm_ret);
                    SysAppStateSetInternalError(topic, ProxyPassword);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid proxy_password");
            SysAppStateSetInvalidArgError(topic, ProxyPassword);
        }
    }

network_proxy_exit:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p) ret %d", esfj_handle, esfj_ret);
    }

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgNetworkSettings(const char* param)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    const uint32_t topic = ST_TOPIC_NETWORK_SETTINGS;
    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;
    EsfNetworkManagerParameterMask esfnm_mask = {0};
    EsfNetworkManagerParameter esfnm_param = {0};

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p) ret %d", esfj_handle, esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p) ret %d", esfj_handle, esfj_ret);
        ret = kRetFailed;
        goto network_setting_exit;
    }

    // Get req_id property.

    const char* req_id = NULL;
    ret = SysAppCmnGetReqId(esfj_handle, val, &req_id);

    if (ret == kRetOk) {
        if (strnlen(req_id, (CFG_RES_ID_LEN + 1)) <= CFG_RES_ID_LEN) {
            if (CheckUpdateString(topic, Id, req_id)) {
                SysAppStateUpdateString(topic, Id, req_id);
            }
        }
        else {
            SysAppStateUpdateString(topic, Id, "0");
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }
    else {
        SysAppStateUpdateString(topic, Id, "0");

        if (ret == kRetFailed) {
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }

    // Get ip_method property.

    int ip_method = 0;

    int extret = SysAppCmnExtractNumberValue(esfj_handle, val, "ip_method", &ip_method);

    if (extret >= 0) {
        if ((extret >= 1) && ((ip_method >= DhcpIp) && (ip_method < IpMethodNum))) {
            // ip_method is specified.
        }
        else {
            SYSAPP_WARN("Invalid ip_method %d", IpMethod);
            SysAppStateSetInvalidArgError(topic, IpMethod);
        }
    }
    else {
        // If ip_method is not specified, load from saved value.

        ip_method = LoadIpMethodFromEsf();
    }

    // Get ntp_url property.

    const char* ntp_url = NULL;
    extret = SysAppCmnExtractStringValue(esfj_handle, val, "ntp_url", &ntp_url);

    if (extret >= 0) {
        if ((extret >= 1) &&
            (strnlen(ntp_url, (CFGST_NETOWRK_NTP_URL_LEN + 1)) <= CFGST_NETOWRK_NTP_URL_LEN) &&
            (IsValidUrlOrNullString(ntp_url, CFGST_NETOWRK_NTP_URL_LEN))) {
            if (CheckUpdateString(topic, NtpUrl, ntp_url)) {
                EsfClockManagerParams cm_param = {0};
                EsfClockManagerParamsMask cm_mask = {.connect.hostname = 1};
                snprintf(cm_param.connect.hostname, sizeof(cm_param.connect.hostname), "%s",
                         ntp_url);

                EsfClockManagerReturnValue esfcm_ret = EsfClockManagerSetParamsForcibly(&cm_param,
                                                                                        &cm_mask);

                if (esfcm_ret == kClockManagerSuccess) {
                    // Reread after write ClockManager params.

                    esfcm_ret = EsfClockManagerGetParams(&cm_param);
                    ntp_url = &(cm_param.connect.hostname[0]);

                    if (esfcm_ret == kClockManagerSuccess) {
                        SysAppStateUpdateString(topic, NtpUrl, ntp_url);
                    }
                    else {
                        SYSAPP_WARN("EsfClockManagerGetParams() %d", esfcm_ret);
                        SysAppStateSetInternalError(topic, NtpUrl);
                    }
                }
                else {
                    SYSAPP_WARN("EsfClockManagerSetParamsForcibly() %d", esfcm_ret);
                    SysAppStateSetInternalError(topic, NtpUrl);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid ntp_url");
            SysAppStateSetInvalidArgError(topic, NtpUrl);
        }
    }

    // Get static_settings_ipv6.

    EsfJsonValue cval;
#if 0 // TODO:IPv6 could be save but not effective.
  RetCode static_settings_ipv6_ret = kRetOk;
#endif

    if (ip_method == StaticIp) {
        esfj_ret = EsfJsonObjectGet(esfj_handle, val, "static_settings_ipv6", &cval);

        if (esfj_ret == kEsfJsonSuccess) {
            EsfJsonValueType val_type;
            esfj_ret = EsfJsonValueTypeGet(esfj_handle, cval, &val_type);

            if ((val_type == kEsfJsonValueTypeObject) && (esfj_ret == kEsfJsonSuccess)) {
                const char* sta_obj_str = NULL;
                esfj_ret = EsfJsonSerialize(esfj_handle, cval, &sta_obj_str);

                if ((sta_obj_str != NULL) && (esfj_ret == kEsfJsonSuccess)) {
#if 0 // TODO:IPv6 could be save but not effective.
          static_settings_ipv6_ret = SysAppCfgStaticSettingsIPv6(sta_obj_str);
#else
                    SysAppCfgStaticSettingsIPv6(sta_obj_str);
#endif
                }

                EsfJsonSerializeFree(esfj_handle);
            }
        }
    }

    // Get static_settings_ipv4.

    /* IPv4 information in Flash is obtained,
   * and if all elements are present, kRetOk is set. */

    RetCode static_settings_ipv4_ret = ExistStaticIPv4InFlash();

    if (ip_method == StaticIp) {
        esfj_ret = EsfJsonObjectGet(esfj_handle, val, "static_settings_ipv4", &cval);

        if (esfj_ret == kEsfJsonSuccess) {
            EsfJsonValueType val_type;
            esfj_ret = EsfJsonValueTypeGet(esfj_handle, cval, &val_type);

            if ((val_type == kEsfJsonValueTypeObject) && (esfj_ret == kEsfJsonSuccess)) {
                const char* sta_obj_str = NULL;
                esfj_ret = EsfJsonSerialize(esfj_handle, cval, &sta_obj_str);

                if ((sta_obj_str != NULL) && (esfj_ret == kEsfJsonSuccess)) {
                    static_settings_ipv4_ret = SysAppCfgStaticSettingsIPv4(sta_obj_str);
                }

                EsfJsonSerializeFree(esfj_handle);
            }
        }
    }

    // Get proxy_settings.

    esfj_ret = EsfJsonObjectGet(esfj_handle, val, "proxy_settings", &cval);

    if (esfj_ret == kEsfJsonSuccess) {
        EsfJsonValueType val_type;
        esfj_ret = EsfJsonValueTypeGet(esfj_handle, cval, &val_type);

        if ((val_type == kEsfJsonValueTypeObject) && (esfj_ret == kEsfJsonSuccess)) {
            const char* sta_obj_str = NULL;
            esfj_ret = EsfJsonSerialize(esfj_handle, cval, &sta_obj_str);

            if ((sta_obj_str != NULL) && (esfj_ret == kEsfJsonSuccess)) {
                SysAppCfgProxySettings(sta_obj_str);
            }

            EsfJsonSerializeFree(esfj_handle);
        }
    }

    // Save ip_method property. (Can be write when static_setting is valid, or DHCP.)

    if ((static_settings_ipv4_ret == kRetOk)
#if 0 // TODO:IPv6 could be save but not effective.
   || (static_settings_ipv6_ret == kRetOk)
#endif
        || (ip_method == DhcpIp)) {
        if (CheckUpdateNumber(topic, IpMethod, ip_method)) {
            memset(&esfnm_mask, 0, sizeof(esfnm_mask));
            esfnm_mask.normal_mode.ip_method = 1;
            esfnm_param.normal_mode.ip_method = ip_method;

            esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

            if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                SysAppStateUpdateNumber(topic, IpMethod, ip_method);
            }
            else {
                SYSAPP_WARN("EsfNetworkManagerSaveParameter(ip_method) %d", esfnm_ret);
                SysAppStateSetInternalError(topic, IpMethod);
            }
        }
    }

    /* Error if ip_method specify in configuration differ
   * from value actually written to Esf. */

    if (ip_method != LoadIpMethodFromEsf()) {
        SysAppStateSetInvalidArgError(topic, IpMethod);
    }

    // Request to send wireless_setting.

    ret = SysAppStateSendState(ST_TOPIC_NETWORK_SETTINGS);

    if (ret != kRetOk) {
        SYSAPP_WARN("Send network_settings failed %d", ret);
    }

network_setting_exit:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p) ret %d", esfj_handle, esfj_ret);
    }

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgIntervalSetting(const char* param, int idx)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    const uint32_t topic = ST_TOPIC_PERIODIC_SETTING;

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p) ret %d", esfj_handle, esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p) ret %d", esfj_handle, esfj_ret);
        goto interval_setting_exit;
    }

    // Get base_time.

    const char* base_time = NULL;
    int extret = SysAppCmnExtractStringValue(esfj_handle, val, "base_time", &base_time);

    if (extret >= 0) {
        if ((extret >= 1) && (strnlen(base_time, CFGST_PERIODIC_INTERVAL_BASETIME_LEN + 1) ==
                              CFGST_PERIODIC_INTERVAL_BASETIME_LEN)) {
            if (CheckUpdateStringWithIdx(topic, BaseTime, idx, base_time)) {
                SysAppStateUpdateStringWithIdx(topic, BaseTime, base_time, idx);
            }
        }
        else {
            SYSAPP_WARN("Invalid base_time");
            SysAppStateSetInvalidArgErrorWithIdx(topic, BaseTime, idx);
        }
    }

    // Get capture_interval.

    int capture_interval = 0;
    extret = SysAppCmnExtractNumberValue(esfj_handle, val, "capture_interval", &capture_interval);

    if (extret >= 0) {
        if ((extret >= 1) &&
            ((capture_interval == 0) || (capture_interval >= 3 && capture_interval <= 1440))) {
            if (CheckUpdateNumberWithIdx(topic, CaptureInterval, idx, capture_interval)) {
                SysAppStateUpdateNumberWithIdx(topic, CaptureInterval, capture_interval, idx);
            }
        }
        else {
            SYSAPP_WARN("Invalid capture_interval %d", capture_interval);
            SysAppStateSetInvalidArgErrorWithIdx(topic, CaptureInterval, idx);
        }
    }

    // Get config_interval.

    int config_interval = 0;
    extret = SysAppCmnExtractNumberValue(esfj_handle, val, "config_interval", &config_interval);

    if (extret >= 0) {
        if ((extret >= 1) &&
            ((config_interval == 0) || (config_interval >= 5 && config_interval <= 1440))) {
            if (CheckUpdateNumberWithIdx(topic, ConfigInterval, idx, config_interval)) {
                SysAppStateUpdateNumberWithIdx(topic, ConfigInterval, config_interval, idx);
            }
        }
        else {
            SYSAPP_WARN("Invalid config_interval %d", config_interval);
            SysAppStateSetInvalidArgErrorWithIdx(topic, ConfigInterval, idx);
        }
    }

interval_setting_exit:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p) ret %d", esfj_handle, esfj_ret);
    }

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgPeriodicSetting(const char* param)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    const uint32_t topic = ST_TOPIC_PERIODIC_SETTING;

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p) ret %d", esfj_handle, esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p) ret %d", esfj_handle, esfj_ret);
        goto periodic_setting_exit;
    }

    // Get req_id property.

    const char* req_id = NULL;
    ret = SysAppCmnGetReqId(esfj_handle, val, &req_id);

    if (ret == kRetOk) {
        if (strnlen(req_id, (CFG_RES_ID_LEN + 1)) <= CFG_RES_ID_LEN) {
            if (CheckUpdateString(topic, Id, req_id)) {
                SysAppStateUpdateString(topic, Id, req_id);
            }
        }
        else {
            SysAppStateUpdateString(topic, Id, "0");
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }
    else {
        SysAppStateUpdateString(topic, Id, "0");

        if (ret == kRetFailed) {
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }

    // Get opration_mode property.

    int operation_mode = 0;
    int extret = SysAppCmnExtractNumberValue(esfj_handle, val, "operation_mode", &operation_mode);

    if (extret >= 0) {
        if ((extret >= 1) &&
            ((operation_mode >= ContinuoutMode) && (operation_mode < OperationModeNum))) {
            if (CheckUpdateNumber(topic, OperationMode, operation_mode)) {
                SysAppStateUpdateNumber(topic, OperationMode, operation_mode);
            }
        }
        else {
            SYSAPP_WARN("Invalid operation_mode %d", operation_mode);
            SysAppStateSetInvalidArgError(topic, OperationMode);
        }
    }

    // Get recovery_method property.

    int recovery_method = 0;
    extret = SysAppCmnExtractNumberValue(esfj_handle, val, "recovery_method", &recovery_method);

    if (extret >= 0) {
        if ((extret >= 1) &&
            ((recovery_method >= ManualReset) && (recovery_method < RecoveryMethodNum))) {
            if (CheckUpdateNumber(topic, RecoveryMethod, recovery_method)) {
                SysAppStateUpdateNumber(topic, RecoveryMethod, recovery_method);
            }
        }
        else {
            SYSAPP_WARN("Invalid recovery_method %d", recovery_method);
            SysAppStateSetInvalidArgError(topic, RecoveryMethod);
        }
    }

    // Get interval_settings.

    EsfJsonValue cval = ESF_JSON_VALUE_INVALID;
    esfj_ret = EsfJsonObjectGet(esfj_handle, val, "interval_settings", &cval);

    if (esfj_ret == kEsfJsonSuccess) {
        EsfJsonValueType val_type;
        esfj_ret = EsfJsonValueTypeGet(esfj_handle, cval, &val_type);

        if ((val_type == kEsfJsonValueTypeArray) && (esfj_ret == kEsfJsonSuccess)) {
            int32_t num = EsfJsonArrayCount(esfj_handle, cval);

            for (int32_t idx = 0; idx < num; idx++) {
                EsfJsonValue ccval;

                esfj_ret = EsfJsonArrayGet(esfj_handle, cval, idx, &ccval);

                if (esfj_ret == kEsfJsonSuccess) {
                    const char* interval_setting_obj_str = NULL;
                    esfj_ret = EsfJsonSerialize(esfj_handle, ccval, &interval_setting_obj_str);

                    if ((interval_setting_obj_str != NULL) && (esfj_ret == kEsfJsonSuccess)) {
                        SysAppCfgIntervalSetting(interval_setting_obj_str, idx);
                    }

                    EsfJsonSerializeFree(esfj_handle);
                }
            }
        }
    }

    // Get ip_addr_setting.

    const char* ip_addr_setting = NULL;
    extret = SysAppCmnExtractStringValue(esfj_handle, val, "ip_addr_setting", &ip_addr_setting);
    if (extret >= 0) {
        if ((extret >= 1) &&
            ((strcmp(ip_addr_setting, "save") == 0) || (strcmp(ip_addr_setting, "dhcp") == 0))) {
            if (CheckUpdateString(topic, IpAddrSetting, ip_addr_setting)) {
                SysAppStateUpdateString(topic, IpAddrSetting, ip_addr_setting);
            }
        }
        else {
            SYSAPP_WARN("Invalid ip_addr_setting");
            SysAppStateSetInvalidArgError(topic, IpAddrSetting);
        }
    }

periodic_setting_exit:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p) ret %d", esfj_handle, esfj_ret);
    }

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgStaModeSetting(const char* param)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    EsfNetworkManagerResult esfnm_ret = kEsfNetworkManagerResultSuccess;
    EsfNetworkManagerParameterMask esfnm_mask = {0};
    EsfNetworkManagerParameter esfnm_param = {0};
    const uint32_t topic = ST_TOPIC_WIRELESS_SETTING;

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p) ret %d", esfj_handle, esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p) ret %d", esfj_handle, esfj_ret);
        goto wireless_sta_mode_exit;
    }

    // Get ssid property.

    const char* ssid = NULL;
    int extret = SysAppCmnExtractStringValue(esfj_handle, val, "ssid", &ssid);

    if (extret >= 0) {
        if ((extret >= 1) &&
            (strnlen(ssid, (CFGST_WIRELESS_STA_SSID_LEN + 1)) <= CFGST_WIRELESS_STA_SSID_LEN)) {
            if (CheckUpdateString(topic, StaSsid, ssid)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));
                esfnm_mask.normal_mode.wifi_sta.ssid = 1;
                snprintf(esfnm_param.normal_mode.wifi_sta.ssid,
                         sizeof(esfnm_param.normal_mode.wifi_sta.ssid), "%s", ssid);

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateString(topic, StaSsid, ssid);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(wifi_sta.ssid) failed %d",
                                esfnm_ret);
                    SysAppStateSetInternalError(topic, StaSsid);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid ssid");
            SysAppStateSetInvalidArgError(topic, StaSsid);
        }
    }

    // Get password property.

    const char* password = NULL;
    extret = SysAppCmnExtractStringValue(esfj_handle, val, "password", &password);

    if (extret >= 0) {
        if ((extret >= 1) && (strnlen(password, (CFGST_WIRELESS_STA_PASSWORD_LEN + 1)) <=
                              CFGST_WIRELESS_STA_PASSWORD_LEN)) {
            int password_len = strnlen(password, (CFGST_WIRELESS_STA_PASSWORD_LEN + 1));

            // Reject passwords with length 1-7 characters
            if (password_len >= 1 && password_len < 8) {
                SYSAPP_WARN(
                    "Invalid password length: password must be 0 characters (open) or 8+ "
                    "characters (encrypted)");
                SysAppStateSetInvalidArgError(topic, StaPassword);
            }
            else if (CheckUpdateString(topic, StaPassword, password)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));
                esfnm_mask.normal_mode.wifi_sta.password = 1;
                snprintf(esfnm_param.normal_mode.wifi_sta.password,
                         sizeof(esfnm_param.normal_mode.wifi_sta.password), "%s", password);

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateString(topic, StaPassword, password);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(wifi_sta.password) failed %d",
                                esfnm_ret);
                    SysAppStateSetInternalError(topic, StaPassword);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid password");
            SysAppStateSetInvalidArgError(topic, StaPassword);
        }
    }

    // Get encryption property.

    int encryption = 0;
    extret = SysAppCmnExtractNumberValue(esfj_handle, val, "encryption", &encryption);

    if (extret >= 0) {
        if ((extret >= 1) && ((encryption >= EncWpa2Psk) && (encryption < WirelessEncryptionNum))) {
            if (CheckUpdateNumber(topic, StaEncryption, encryption)) {
                memset(&esfnm_mask, 0, sizeof(esfnm_mask));
                esfnm_mask.normal_mode.wifi_sta.encryption = 1;
                esfnm_param.normal_mode.wifi_sta.encryption = encryption;

                esfnm_ret = EsfNetworkManagerSaveParameter(&esfnm_mask, &esfnm_param);

                if (esfnm_ret == kEsfNetworkManagerResultSuccess) {
                    SysAppStateUpdateNumber(topic, StaEncryption, encryption);
                }
                else {
                    SYSAPP_WARN("EsfNetworkManagerSaveParameter(wifi_sta.encryption) failed %d",
                                esfnm_ret);
                    SysAppStateSetInternalError(topic, StaEncryption);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid encryption %d", encryption);
            SysAppStateSetInvalidArgError(topic, StaEncryption);
        }
    }

wireless_sta_mode_exit:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p) ret %d", esfj_handle, esfj_ret);
    }

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgWirelessSetting(const char* param)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    const uint32_t topic = ST_TOPIC_WIRELESS_SETTING;

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p) ret %d", esfj_handle, esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p) ret %d", esfj_handle, esfj_ret);
        ret = kRetFailed;
        goto wireless_setting_exit;
    }

    // Get req_id property.

    const char* req_id = NULL;
    ret = SysAppCmnGetReqId(esfj_handle, val, &req_id);

    if (ret == kRetOk) {
        if (strnlen(req_id, (CFG_RES_ID_LEN + 1)) <= CFG_RES_ID_LEN) {
            if (CheckUpdateString(topic, Id, req_id)) {
                SysAppStateUpdateString(topic, Id, req_id);
            }
        }
        else {
            SysAppStateUpdateString(topic, Id, "0");
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }
    else {
        SysAppStateUpdateString(topic, Id, "0");

        if (ret == kRetFailed) {
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }

    // Get sta_mode_setting property.

    EsfJsonValue cval;
    esfj_ret = EsfJsonObjectGet(esfj_handle, val, "sta_mode_setting", &cval);

    if (esfj_ret == kEsfJsonSuccess) {
        EsfJsonValueType val_type;
        esfj_ret = EsfJsonValueTypeGet(esfj_handle, cval, &val_type);

        if ((val_type == kEsfJsonValueTypeObject) && (esfj_ret == kEsfJsonSuccess)) {
            const char* sta_obj_str = NULL;
            esfj_ret = EsfJsonSerialize(esfj_handle, cval, &sta_obj_str);

            if ((sta_obj_str != NULL) && (esfj_ret == kEsfJsonSuccess)) {
                SysAppCfgStaModeSetting(sta_obj_str);
            }

            EsfJsonSerializeFree(esfj_handle);
        }
    }

    // Request to send wireless_setting.

    ret = SysAppStateSendState(ST_TOPIC_WIRELESS_SETTING);

    if (ret != kRetOk) {
        SYSAPP_WARN("Send wireless_settings failed %d", ret);
    }

wireless_setting_exit:

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p) ret %d", esfj_handle, esfj_ret);
    }

    return ret;
}

/*----------------------------------------------------------------------*/
RetCode SysAppCfgEndpointSettings(const char* param)
{
    RetCode ret = kRetOk;
    EsfJsonHandle esfj_handle = ESF_JSON_HANDLE_INITIALIZER;
    EsfJsonValue val = ESF_JSON_VALUE_INVALID;
    const uint32_t topic = ST_TOPIC_ENDPOINT_SETTINGS;
    EsfSystemManagerResult esfsm_ret = kEsfSystemManagerResultOk;
    char* endp_host_bk = NULL;
    size_t endp_host_buf_size = ESF_SYSTEM_MANAGER_EVP_HUB_URL_MAX_SIZE;
    char* endp_port_bk = NULL;
    size_t endp_port_buf_size = ESF_SYSTEM_MANAGER_EVP_HUB_PORT_MAX_SIZE;

    // Open handle and set config parameters.

    EsfJsonErrorCode esfj_ret = EsfJsonOpen(&esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonOpen(%p) ret %d", esfj_handle, esfj_ret);
        return kRetFailed;
    }

    esfj_ret = EsfJsonDeserialize(esfj_handle, param, &val);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonDeserialize(%p) ret %d", esfj_handle, esfj_ret);
        ret = kRetFailed;
        goto endpoint_settings_exit;
    }

    // Get req_id property.

    const char* req_id = NULL;
    ret = SysAppCmnGetReqId(esfj_handle, val, &req_id);

    if (ret == kRetOk) {
        if (strnlen(req_id, (CFG_RES_ID_LEN + 1)) <= CFG_RES_ID_LEN) {
            if (CheckUpdateString(topic, Id, req_id)) {
                SysAppStateUpdateString(topic, Id, req_id);
            }
        }
        else {
            SysAppStateUpdateString(topic, Id, "0");
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }
    else {
        SysAppStateUpdateString(topic, Id, "0");

        if (ret == kRetFailed) {
            SysAppStateSetInvalidArgError(topic, Id);
        }
    }

    // Get endpoint url and port for backup purposes.

    endp_host_bk = (char*)malloc(endp_host_buf_size);

    if (endp_host_bk == NULL) {
        SysAppStateSetInternalError(topic, EndpointUrl);
        goto request_to_send_endpoint_settings_exit;
    }

    esfsm_ret = EsfSystemManagerGetEvpHubUrl(endp_host_bk, &endp_host_buf_size);

    if (esfsm_ret != kEsfSystemManagerResultOk) {
        SYSAPP_WARN("EsfSystemManagerGetEvpHubUrl() failed %d", esfsm_ret);
        SysAppStateSetInternalError(topic, EndpointUrl);
        goto request_to_send_endpoint_settings_exit;
    }

    endp_port_bk = (char*)malloc(endp_port_buf_size);
    if (endp_port_bk == NULL) {
        SysAppStateSetInternalError(topic, EndpointPort);
        goto request_to_send_endpoint_settings_exit;
    }

    esfsm_ret = EsfSystemManagerGetEvpHubPort(endp_port_bk, &endp_port_buf_size);

    if (esfsm_ret != kEsfSystemManagerResultOk) {
        SYSAPP_WARN("EsfSystemManagerGetEvpHubPort() failed %d", esfsm_ret);
        SysAppStateSetInternalError(topic, EndpointPort);
        goto request_to_send_endpoint_settings_exit;
    }

    // Get endpoint_url property.

    uint32_t writeback_request = 0;
    const char* endpoint_url = NULL;
    int extret = SysAppCmnExtractStringValue(esfj_handle, val, "endpoint_url", &endpoint_url);

    if (extret >= 0) {
        int len = (extret >= 1) ? strnlen(endpoint_url, (CFGST_ENDPOINT_DOMAIN_LEN_MAX + 1)) : 0;
        if ((extret >= 1) && (len <= CFGST_ENDPOINT_DOMAIN_LEN_MAX) &&
            (IsValidUrlOrIpAddress(endpoint_url, CFGST_ENDPOINT_DOMAIN_LEN_MAX))) {
            if (CheckUpdateString(topic, EndpointUrl, endpoint_url)) {
                esfsm_ret = EsfSystemManagerSetEvpHubUrl(endpoint_url, len + 1 /* Add '\0' */);

                if (esfsm_ret == kEsfSystemManagerResultOk) {
                    SysAppStateUpdateString(topic, EndpointUrl, endpoint_url);
                }
                else {
                    SYSAPP_WARN("EsfSystemManagerSetEvpHubUrl() failed %d", esfsm_ret);
                    SysAppStateSetInternalError(topic, EndpointUrl);
                    writeback_request |= (1 << EndpointUrl);
                }
            }
        }
        else {
            SYSAPP_WARN("Invalid endpoint_url");
            SysAppStateSetInvalidArgError(topic, EndpointUrl);
        }
    }

    // Get endpoint_port property.

    int endpoint_port = 0;
    extret = SysAppCmnExtractNumberValue(esfj_handle, val, "endpoint_port", &endpoint_port);

    if (extret >= 0) {
        if ((extret >= 1) && ((endpoint_port >= 0) && (endpoint_port <= CFGST_ENDPOINT_PORT_MAX))) {
            char* port_buf = (char*)malloc(ESF_SYSTEM_MANAGER_EVP_HUB_PORT_MAX_SIZE);
            if (port_buf != NULL) {
                int len = snprintf(port_buf, ESF_SYSTEM_MANAGER_EVP_HUB_PORT_MAX_SIZE, "%d",
                                   endpoint_port);

                if (CheckUpdateString(topic, EndpointPort, port_buf)) {
                    esfsm_ret = EsfSystemManagerSetEvpHubPort(port_buf, len + 1 /* Add '\0' */);

                    if (esfsm_ret == kEsfSystemManagerResultOk) {
                        SysAppStateUpdateNumber(topic, EndpointPort, endpoint_port);
                    }
                    else {
                        SYSAPP_WARN("EsfSystemManagerSetEvpHubPort() failed %d", esfsm_ret);
                        SysAppStateSetInternalError(topic, EndpointPort);
                        writeback_request |= (1 << EndpointPort);
                    }
                }

                free(port_buf);
            }
            else {
                SysAppStateSetInternalError(topic, EndpointPort);
            }
        }
        else {
            SYSAPP_WARN("Invalid endpoint_port %d", endpoint_port);
            SysAppStateSetInvalidArgError(topic, EndpointPort);
        }
    }

    // Check both of url and port are successfully written?

    if (writeback_request) {
        SYSAPP_WARN("Write error! Reverting");

        esfsm_ret = EsfSystemManagerSetEvpHubUrl(endp_host_bk, endp_host_buf_size);

        if (esfsm_ret != kEsfSystemManagerResultOk) {
            SYSAPP_WARN("EsfSystemManagerSetEvpHubUrl() failed %d", esfsm_ret);
        }

        esfsm_ret = EsfSystemManagerSetEvpHubPort(endp_port_bk, endp_port_buf_size);

        if (esfsm_ret != kEsfSystemManagerResultOk) {
            SYSAPP_WARN("EsfSystemManagerSetEvpHubPort() failed %d", esfsm_ret);
        }
    }
    else {
        // Clear Enrollment Data(project id, tokn).

        ClearEnrollmentData();
    }

    // Get protocol_version property.
    // Note : This item is an error if it is specified other than "TB".

    const char* protocol_version = NULL;
    extret = SysAppCmnExtractStringValue(esfj_handle, val, "protocol_version", &protocol_version);

    if (extret >= 0) {
        if ((extret >= 1) &&
            (strnlen(protocol_version, (CFGST_ENDPOINT_PROTOCOL_VERSION_LEN + 1)) <=
             CFGST_ENDPOINT_PROTOCOL_VERSION_LEN)) {
            if (strncmp(protocol_version, "TB", CFGST_ENDPOINT_PROTOCOL_VERSION_LEN) == 0) {
                if (CheckUpdateString(topic, ProtocolVersion, protocol_version)) {
                    SysAppStateUpdateString(topic, ProtocolVersion, protocol_version);
                }
            }
            else {
                SYSAPP_WARN("Invalid protocol_version.");
                SysAppStateSetInvalidArgError(topic, ProtocolVersion);
            }
        }
        else {
            SYSAPP_WARN("Invalid protocol_version");
            SysAppStateSetInvalidArgError(topic, ProtocolVersion);
        }
    }

request_to_send_endpoint_settings_exit:

    // Request to send endpoint_setting.

    ret = SysAppStateSendState(ST_TOPIC_ENDPOINT_SETTINGS);

    if (ret != kRetOk) {
        SYSAPP_WARN("Send endpoint_settings failed %d", ret);
    }

endpoint_settings_exit:
    free(endp_host_bk);
    free(endp_port_bk);

    // Close handle.

    esfj_ret = EsfJsonClose(esfj_handle);

    if (esfj_ret != kEsfJsonSuccess) {
        SYSAPP_ERR("EsfJsonClose(%p) ret %d", esfj_handle, esfj_ret);
    }

    return ret;
}

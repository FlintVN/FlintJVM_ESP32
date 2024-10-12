
#include "flint.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "flint_string.h"
#include "flint_system_api.h"
#include "esp_system_native_wifi.h"

static const wifi_auth_mode_t authValueList[] = {
    WIFI_AUTH_OPEN,
    WIFI_AUTH_WEP,
    WIFI_AUTH_WPA_PSK,
    WIFI_AUTH_WPA2_PSK,
    WIFI_AUTH_WPA_WPA2_PSK,
    WIFI_AUTH_ENTERPRISE,
    WIFI_AUTH_WPA2_ENTERPRISE,
    WIFI_AUTH_WPA3_PSK,
    WIFI_AUTH_WPA2_WPA3_PSK,
    WIFI_AUTH_WAPI_PSK,
    WIFI_AUTH_OWE,
    WIFI_AUTH_WPA3_ENT_192,
    WIFI_AUTH_WPA3_EXT_PSK,
    WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE,
    WIFI_AUTH_DPP,
};

static bool checkParam(FlintExecution &execution, FlintString *ssid, FlintString *password, uint32_t authMode) {
    if(authMode >= sizeof(authValueList)) {
        FlintString &strObj = execution.flint.newString(STR_AND_SIZE("Authentication mode is invalid"));
        FlintThrowable &excpObj = execution.flint.newErrorException(strObj);
        execution.stackPushObject(&excpObj);
        return false;
    }
    if((ssid == 0) || ((password == 0) && (authValueList[authMode] != WIFI_AUTH_OPEN))) {
        const char *msg[] = {(ssid == 0) ? "ssid" : "password", " cannot be null object"};
        FlintString &strObj = execution.flint.newString(msg, LENGTH(msg));
        FlintThrowable &excpObj = execution.flint.newNullPointerException(strObj);
        execution.stackPushObject(&excpObj);
        return false;
    }

    uint32_t ssidLen = ssid->getLength();
    uint32_t passwordLen = password ? password->getLength() : 0;
    if((ssidLen > 32) || (passwordLen > 64)) {
        const char *msg[] = {(ssidLen > 32) ? "ssid" : "password", " value is invalid"};
        FlintString &strObj = execution.flint.newString(msg, LENGTH(msg));
        FlintThrowable &excpObj = execution.flint.newErrorException(strObj);
        execution.stackPushObject(&excpObj);
        return false;
    }
    return true;
}

static bool checkReturn(FlintExecution &execution, esp_err_t value) {
    if(value != ESP_OK) {
        FlintString &strObj = execution.flint.newString(STR_AND_SIZE("An error occurred while connecting to wifi"));
        FlintThrowable &excpObj = execution.flint.newErrorException(strObj);
        execution.stackPushObject(&excpObj);
        return false;
    }
    return true;
}

static bool nativeIsSupported(FlintExecution &execution) {
    execution.stackPushInt32(1);
    return true;
}

static bool nativeConnect(FlintExecution &execution) {
    uint32_t authMode = execution.stackPopInt32();
    FlintString *password = (FlintString *)execution.stackPopObject();
    FlintString *ssid = (FlintString *)execution.stackPopObject();

    if(!checkParam(execution, ssid, password, authMode))
        return false;

    uint32_t ssidLen = ssid->getLength();
    uint32_t passwordLen = password ? password->getLength() : 0;
    const char *ssidText = ssid->getText();
    const char *passwordText = password ? password->getText() : 0;
    wifi_config_t wifiConfig = {};
    for(uint8_t i = 0; i < ssidLen; i++)
        wifiConfig.sta.ssid[i] = ssidText[i];
    for(uint8_t i = 0; i < passwordLen; i++)
        wifiConfig.sta.password[i] = passwordText[i];
    wifiConfig.sta.threshold.authmode = authValueList[authMode];
    wifiConfig.sta.pmf_cfg.capable = true;
    wifiConfig.sta.pmf_cfg.required = false;

    execution.flint.lock();
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);
    if(ret == ESP_OK)
        ret = esp_wifi_start();
    if(ret == ESP_OK)
        ret = esp_wifi_connect();
    execution.flint.unlock();

    return checkReturn(execution, ret);
}

static bool nativeIsConnected(FlintExecution &execution) {
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    execution.stackPushInt32((ret == ESP_OK) ? 1 : 0);
    return true;
}

static bool nativeGetMacAddress(FlintExecution &execution) {
    uint8_t mac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if(ret == ESP_OK) {
        FlintObject &byteArray = execution.flint.newObject(6, *(FlintConstUtf8 *)primTypeConstUtf8List[4], 1);
        byteArray.data[0] = mac[0];
        byteArray.data[1] = mac[1];
        byteArray.data[2] = mac[2];
        byteArray.data[3] = mac[3];
        byteArray.data[4] = mac[4];
        byteArray.data[5] = mac[5];
        execution.stackPushObject(&byteArray);
    }
    else
        execution.stackPushObject(0);
    return true;
}

static bool nativeDisconnect(FlintExecution &execution) {
    esp_wifi_disconnect();
    return true;
}

static bool nativeSoftAP(FlintExecution &execution) {
    uint32_t maxConnection = execution.stackPopInt32();
    uint32_t channel = execution.stackPopInt32();
    uint32_t authMode = execution.stackPopInt32();
    FlintString *password = (FlintString *)execution.stackPopObject();
    FlintString *ssid = (FlintString *)execution.stackPopObject();

    if(!checkParam(execution, ssid, password, authMode))
        return false;

    uint32_t ssidLen = ssid->getLength();
    uint32_t passwordLen = password ? password->getLength() : 0;
    const char *ssidText = ssid->getText();
    const char *passwordText = password ? password->getText() : 0;
    wifi_config_t wifiConfig = {};
    wifiConfig.ap.ssid_len = ssidLen;
    wifiConfig.ap.channel = channel;
    wifiConfig.ap.max_connection = maxConnection;
    wifiConfig.ap.authmode = authValueList[authMode];
    wifiConfig.ap.pmf_cfg.required = false;
    for(uint8_t i = 0; i < ssidLen; i++)
        wifiConfig.ap.ssid[i] = ssidText[i];
    for(uint8_t i = 0; i < passwordLen; i++)
        wifiConfig.ap.password[i] = passwordText[i];

    execution.flint.lock();
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if(ret == ESP_OK)
        ret = esp_wifi_set_config(WIFI_IF_AP, &wifiConfig);
    if(ret == ESP_OK)
        ret = esp_wifi_start();
    execution.flint.unlock();

    return checkReturn(execution, ret);
}

static bool nativeSoftAPdisconnect(FlintExecution &execution) {
    esp_wifi_set_mode(WIFI_MODE_STA);
    return true;
}

static const FlintNativeMethod methods[] = {
    NATIVE_METHOD("\x0B\x00\x2B\x6D""isSupported",      "\x03\x00\x91\x9C""()Z",                                        nativeIsSupported),

    NATIVE_METHOD("\x07\x00\x73\x4F""connect",          "\x28\x00\x84\x65""(Ljava/lang/String;Ljava/lang/String;I)V",   nativeConnect),
    NATIVE_METHOD("\x0B\x00\x07\x5C""isConnected",      "\x03\x00\x91\x9C""()Z",                                        nativeIsConnected),
    NATIVE_METHOD("\x0D\x00\x72\x55""getMacAddress",    "\x04\x00\x9C\xB2""()[B",                                       nativeGetMacAddress),
    NATIVE_METHOD("\x0A\x00\xDF\x40""disconnect",       "\x03\x00\x91\x99""()V",                                        nativeDisconnect),

    NATIVE_METHOD("\x06\x00\x4E\x10""softAP",           "\x2A\x00\xED\x0C""(Ljava/lang/String;Ljava/lang/String;III)V", nativeSoftAP),
    NATIVE_METHOD("\x10\x00\x71\xA1""softAPdisconnect", "\x03\x00\x91\x99""()V",                                        nativeSoftAPdisconnect),
};

const FlintNativeClass WIFI_CLASS = NATIVE_CLASS(*(const FlintConstUtf8 *)"\x0C\x00\xE6\x3F""network/WiFi", methods);

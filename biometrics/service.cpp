/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.biometrics.fingerprint@2.1-service"

#include <android/log.h>
#include <android-base/file.h>
#include <android-base/properties.h>
#include <binder/ProcessState.h>
#include <hidl/HidlSupport.h>
#include <hidl/HidlTransportSupport.h>
#include <android/hardware/biometrics/fingerprint/2.1/IBiometricsFingerprint.h>
#include <android/hardware/biometrics/fingerprint/2.1/types.h>
#include "BiometricsFingerprint.h"
#include <errno.h>
#include <unistd.h>

int fingerprint_type = 0;
bool fp_type_2_is_goodix = false;
bool fp_type_2_use_cancel_hack = true;

static constexpr char kGoodixFpDev[] = "/dev/goodix_fp";

using android::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprint;
using android::hardware::biometrics::fingerprint::V2_1::implementation::BiometricsFingerprint;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::sp;

int main() {
    fingerprint_type = std::stoi(android::base::GetProperty("ro.vendor.fingerprint.supported", "0"));
    switch (fingerprint_type) {
        case 2:
            ALOGD("Fingerprint type is set to: %d", fingerprint_type);
            if (!android::base::WriteStringToFile("disable", "/sys/devices/soc/soc:fpc1020/compatible_all", true)) {
                ALOGE("Failed to reset fpc1020 driver.");
            }
            if (android::base::GetProperty("persist.sys.fp.vendor","") == "goodix") {
                ALOGD("Enable workarounds for goodix.");
                fp_type_2_is_goodix = true;
                if (android::base::GetProperty("vendor.fingerprint.goodix.disable_notify_cancel_hack","") == "1") {
                    fp_type_2_use_cancel_hack = false;
                    ALOGD("Disable notify client on cancel hack for goodix.");
                }
            }
            break;
        case 1:
            ALOGD("Fingerprint type is set to: %d", fingerprint_type);
            break;
        case 0:
            ALOGE("Fingerprint is unsupported.");
            android::base::SetProperty("ro.vendor.fingerprint.failed", "1");
            return 1;
        default:
            ALOGE("Invalid fingerprint type: %d", fingerprint_type);
            return 1;
    }

    android::sp<IBiometricsFingerprint> bio = BiometricsFingerprint::getInstance();

    if (fp_type_2_is_goodix) {
        if (access(kGoodixFpDev, F_OK) != 0) {
            ALOGE("Cannot access %s (%s)", kGoodixFpDev, strerror(errno));
            return 1;
        }
        // the conventional HAL might start vndbinder services
        android::ProcessState::initWithDriver("/dev/vndbinder");
        android::ProcessState::self()->startThreadPool();
    }

    configureRpcThreadpool(1, true /*callerWillJoin*/);

    if (bio != nullptr) {
        if (::android::OK != bio->registerAsService()) {
            return 1;
        }
    } else {
        ALOGE("Can't create instance of BiometricsFingerprint, nullptr");
    }

    joinRpcThreadpool();

    return 0; // should never get here
}

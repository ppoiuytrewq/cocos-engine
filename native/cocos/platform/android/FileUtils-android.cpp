/****************************************************************************
 Copyright (c) 2010-2012 cocos2d-x.org
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2023 Xiamen Yaji Software Co., Ltd.

 http://www.cocos.com

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights to
 use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
****************************************************************************/

#include "platform/android/FileUtils-android.h"
#include <android/log.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include "android/asset_manager.h"
#include "android/asset_manager_jni.h"
#include "base/Log.h"
#include "base/ZipUtils.h"
#include "base/memory/Memory.h"
#include "platform/java/jni/JniHelper.h"
#include "platform/java/jni/JniImp.h"
#include "base/base64.h"
#include "base/ZipUtils.h"
#include "platform/CC20.h"

#define LOG_TAG   "FileUtils-android.cpp"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define ASSETS_FOLDER_NAME "@assets/"

#ifndef JCLS_HELPER
    #define JCLS_HELPER "com/cocos/lib/CocosHelper"
#endif

namespace cc {

AAssetManager *FileUtilsAndroid::assetmanager = nullptr;
ZipFile *FileUtilsAndroid::obbfile = nullptr;
ccstd::unordered_map<ccstd::string, FileUtilsAndroid::FileInfo*> *FileUtilsAndroid::assetsMap = nullptr;
ccstd::string *FileUtilsAndroid::assetBinPath = nullptr;

FileUtils *createFileUtils() {
    return ccnew FileUtilsAndroid();
}

void FileUtilsAndroid::setAssetManager(AAssetManager *a) {
    if (nullptr == a) {
        LOGD("setAssetManager : received unexpected nullptr parameter");
        return;
    }

    cc::FileUtilsAndroid::assetmanager = a;

    cc::FileUtilsAndroid::assetsMap = new ccstd::unordered_map<ccstd::string, FileUtilsAndroid::FileInfo*>();

    assetBinPath = new ccstd::string("assets.bin");
    const unsigned char* in = (unsigned char*)"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";
    unsigned char* out = nullptr;
    int len = base64Decode(in, strlen((const char*)in), &out);

#ifdef AES
    const unsigned char* key = (unsigned char*)"6BsvhAKCs8nAGrLx";
    AES_KEY aesKey;
    AES_set_decrypt_key(key, 128, &aesKey);
    const unsigned char* aesIn = (const unsigned char*)out;
    unsigned char* aesOut = (unsigned char*)malloc(sizeof(out));
    AES_decrypt(aesIn, aesOut, &aesKey);
#endif

    CC20::XOR(out, len, "wYfuPsWrYvZSLIfNxqjnxln6GrG64c4Y");

    unsigned char* gzipIn = out;
    unsigned char* gzipOut = nullptr;
    if (ZipUtils::isGZipBuffer(gzipIn, 2)) {
        len = ZipUtils::inflateMemory(gzipIn, len, &gzipOut);
        ccstd::string str = "";
        ccstd::string key = "";
        int oriSize = 0, gzip = 0;
        for (int k = 0, j = 0, totalLength = 0; k < len; k++) {
            char c = gzipOut[k];
            int t = 0;
            if (c == '\n' || c == ',')
                t = 1;
            else {
                str += c;
                if (k == len - 1)
                    t = 1;
            }
            if (t) {
                if (j == 0) {
                    key = "@assets/data/" + str;
                } else if (j == 1) {
                    gzip = atoi(str.c_str());
                } else if (j == 2) {
                    oriSize = atoi(str.c_str());
                } else if (j == 3) {
                    FileUtilsAndroid::FileInfo *info = new FileUtilsAndroid::FileInfo;
                    info->gzip = gzip;
                    info->oriSize = oriSize;
                    info->offset = totalLength;
                    info->len = atoi(str.c_str());
                    totalLength += info->len;
                    (*assetsMap)[key] = info;
                    j = -1;
                }
                str = "";
                j++;
            }
        }
    }
    free(out);
    free(gzipOut);
}

FileUtilsAndroid::FileUtilsAndroid() {
    init();
}

FileUtilsAndroid::~FileUtilsAndroid() {
    CC_SAFE_DELETE(obbfile);
}

bool FileUtilsAndroid::init() {
    _defaultResRootPath = ASSETS_FOLDER_NAME;

    ccstd::string assetsPath(getObbFilePathJNI());
    if (assetsPath.find("/obb/") != ccstd::string::npos) {
        obbfile = ccnew ZipFile(assetsPath);
    }

    return FileUtils::init();
}

bool FileUtilsAndroid::isFileExistInternal(const ccstd::string &strFilePath) const {
    if (strFilePath.empty()) {
        return false;
    }

    bool bFound = false;

    // Check whether file exists in apk.
    if (strFilePath[0] != '/') {
        const char *s = strFilePath.c_str();

        // Found "@assets/" at the beginning of the path and we don't want it
        if (strFilePath.find(ASSETS_FOLDER_NAME) == 0) s += strlen(ASSETS_FOLDER_NAME);
        if (obbfile && obbfile->fileExists(s)) {
            bFound = true;
        } else if (FileUtilsAndroid::assetmanager) {
            auto iter = assetsMap->find(strFilePath);
            bFound = iter != assetsMap->end();
//            AAsset *aa = AAssetManager_open(FileUtilsAndroid::assetmanager, s, AASSET_MODE_UNKNOWN);
//            if (aa) {
//                bFound = true;
//                AAsset_close(aa);
//            } else {
//                // CC_LOG_DEBUG("[AssetManager] ... in APK %s, found = false!", strFilePath.c_str());
//            }
        }
    } else {
        FILE *fp = fopen(strFilePath.c_str(), "r");
        if (fp) {
            bFound = true;
            fclose(fp);
        }
    }
    return bFound;
}

bool FileUtilsAndroid::isDirectoryExistInternal(const ccstd::string &testDirPath) const {
    if (testDirPath.empty()) {
        return false;
    }

    ccstd::string dirPath = testDirPath;
    if (dirPath[dirPath.length() - 1] == '/') {
        dirPath[dirPath.length() - 1] = '\0';
    }

    // find absolute path in flash memory
    if (dirPath[0] == '/') {
        CC_LOG_DEBUG("find in flash memory dirPath(%s)", dirPath.c_str());
        struct stat st;
        if (stat(dirPath.c_str(), &st) == 0) {
            return S_ISDIR(st.st_mode);
        }
    } else {
        // find it in apk's assets dir
        // Found "@assets/" at the beginning of the path and we don't want it
        CC_LOG_DEBUG("find in apk dirPath(%s)", dirPath.c_str());
        const char *s = dirPath.c_str();
        if (dirPath.find(_defaultResRootPath) == 0) {
            s += _defaultResRootPath.length();
        }
        if (FileUtilsAndroid::assetmanager) {
            AAssetDir *aa = AAssetManager_openDir(FileUtilsAndroid::assetmanager, s);
            if (aa && AAssetDir_getNextFileName(aa)) {
                AAssetDir_close(aa);
                return true;
            }
        }
    }

    return false;
}

bool FileUtilsAndroid::isAbsolutePath(const ccstd::string &strPath) const {
    // On Android, there are two situations for full path.
    // 1) Files in APK, e.g. assets/path/path/file.png
    // 2) Files not in APK, e.g. /data/data/org.cocos2dx.hellocpp/cache/path/path/file.png, or /sdcard/path/path/file.png.
    // So these two situations need to be checked on Android.
    return strPath[0] == '/' || strPath.find(ASSETS_FOLDER_NAME) == 0;
}

FileUtils::Status FileUtilsAndroid::getContents(const ccstd::string &filename, ResizableBuffer *buffer) {
    if (filename.empty()) {
        return FileUtils::Status::NOT_EXISTS;
    }

    ccstd::string fullPath = fullPathForFilename(filename);
    if (fullPath.empty()) {
        return FileUtils::Status::NOT_EXISTS;
    }

    if (fullPath[0] == '/') {
        return FileUtils::getContents(fullPath, buffer);
    }

    ccstd::string relativePath;
    size_t position = fullPath.find(ASSETS_FOLDER_NAME);
    if (0 == position) {
        // "@assets/" is at the beginning of the path and we don't want it
        relativePath += fullPath.substr(strlen(ASSETS_FOLDER_NAME));
    } else {
        relativePath = fullPath;
    }

    if (obbfile) {
        if (obbfile->getFileData(relativePath, buffer)) {
            return FileUtils::Status::OK;
        }
    }

    if (nullptr == assetmanager) {
        LOGD("... FileUtilsAndroid::assetmanager is nullptr");
        return FileUtils::Status::NOT_INITIALIZED;
    }

    auto iter = assetsMap->find("@assets/" + relativePath);
    if (iter == assetsMap->end())
        return FileUtils::Status::NOT_EXISTS;
    AAsset *asset = AAssetManager_open(assetmanager, assetBinPath->c_str(), AASSET_MODE_UNKNOWN);
    if (nullptr == asset) {
        LOGD("asset (%s) is nullptr", filename.c_str());
        return FileUtils::Status::OPEN_FAILED;
    }

    int a = iter->second->oriSize * (iter->second->gzip ? 2 : 1);
    int b = (iter->second->gzip ? iter->second->len : 0);
    buffer->resize(a + b);
    AAsset_seek(asset, iter->second->offset, 0);

    unsigned char *pp = (unsigned char *)(buffer->buffer()) + (iter->second->gzip ? a : 0);
    int readsize = AAsset_read(asset, pp, iter->second->len);
    AAsset_close(asset);

    if (readsize >= 5) {
        unsigned char *p = pp + (readsize - 5);
        // 0xee, 0xd3, 0xc9, 0xb1, 0xa6
        if (*p == 0xee && *(p + 1) == 0xd3 && *(p + 2) == 0xc9 && *(p + 3) == 0xb1 && *(p + 4) == 0xa6) {
            readsize -= 5;
            bool b1024 = (relativePath.rfind(".jpg") == relativePath.size() - 4);
            b1024 = b1024 || (relativePath.rfind(".png") == relativePath.size() - 4);
            b1024 = b1024 || (relativePath.rfind(".webp") == relativePath.size() - 5);
            CC20::XOR(pp, b1024 ? std::min(readsize, 1024) : readsize, "wYfuPsWrYvZSLIfNxqjnxln6GrG64c4Y");
        }
    }

    if (iter->second->gzip) {
        uint32_t outLength;
        if (ZipUtils::inflateMemoryWithHint1(pp, readsize, (unsigned char *)(buffer->buffer()), &outLength, a + b))
            LOGD("asset (%s) gzip inflate fail", filename.c_str());
        buffer->resize(iter->second->oriSize);
    }

    return FileUtils::Status::OK;
}

ccstd::string FileUtilsAndroid::getWritablePath() const {
    if (!_writablePath.empty()) {
        return _writablePath;
    }
    // Fix for Nexus 10 (Android 4.2 multi-user environment)
    // the path is retrieved through Java Context.getCacheDir() method
    ccstd::string dir;
    ccstd::string tmp = JniHelper::callStaticStringMethod(JCLS_HELPER, "getWritablePath");

    if (tmp.length() > 0) {
        dir.append(tmp).append("/");
        return dir;
    }
    return "";
}

} // namespace cc

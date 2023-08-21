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
                    key = str;
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
                    (*assetsMap)["@assets/data/" + key] = info;
                    (*assetsMap)["@assets/" + key] = info;
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

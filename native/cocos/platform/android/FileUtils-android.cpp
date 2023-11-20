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

#define cc20key "4aPxbN6wrJXZWX2xTEVfZn6VkI739f5n"

namespace cc {

bool isOffline = true;
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

    if (assetBinPath == nullptr) {
        {
            ccstd::string s = "___________________________Assets.bin___________________________";
            s.erase(0, s.find_first_not_of(' '));
            s.erase(s.find_last_not_of(' ') + 1);
            assetBinPath = new ccstd::string(s);
        }

        unsigned char *in;
        uint8_t c0 = 0, c1 = 0, c2 = 0, c3 = 0;
        AAsset *asset = AAssetManager_open(assetmanager, assetBinPath->c_str(), AASSET_MODE_UNKNOWN);
        if (asset == nullptr)
            return;
        AAsset_seek(asset, 64, 0);
        cc::FileUtilsAndroid::assetsMap = new ccstd::unordered_map<ccstd::string, FileUtilsAndroid::FileInfo*>();
        AAsset_read(asset, &c0, 1);
        AAsset_read(asset, &c1, 1);
        AAsset_read(asset, &c2, 1);
        AAsset_read(asset, &c3, 1);
        int headerLen = c0 + c1 * 256 + c2 * 256 * 256 + c3 * 256 * 256 * 256;
        in = (unsigned  char *)malloc(headerLen);
        AAsset_read(asset, in, headerLen);

        unsigned char *base64out = nullptr;
        int len = base64Decode(in, headerLen, &base64out);

#ifdef AES
        const unsigned char* key = (unsigned char*)"6BsvhAKCs8nAGrLx";
        AES_KEY aesKey;
        AES_set_decrypt_key(key, 128, &aesKey);
        const unsigned char* aesIn = (const unsigned char*)out;
        unsigned char* aesOut = (unsigned char*)malloc(sizeof(out));
        AES_decrypt(aesIn, aesOut, &aesKey);
#endif

        CC20::XOR(base64out, len, cc20key);

        unsigned char *gzipIn = base64out;
        unsigned char *gzipOut = nullptr;
        if (ZipUtils::isGZipBuffer(gzipIn, 2)) {

            len = ZipUtils::inflateMemory(gzipIn, len, &gzipOut);
            ccstd::string str = "";
            ccstd::string key = "";
            int oriSize = 0, gzip = 0;
            for (int k = 0, j = 0, totalLength = 64 + 4 + headerLen; k < len; k++) {
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
                        //(*assetsMap)["@assets/data/" + key] = info;
                        (*assetsMap)["@assets/" + key] = info;
                        j = -1;
                    }
                    str = "";
                    j++;
                }
            }
        }

        free(in);
        free(base64out);
        free(gzipOut);
        AAsset_close(asset);
    }
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

    //return FileUtils::init();
    _searchPathArray.push_back(_defaultResRootPath);
    return true;
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
            if (assetsMap == nullptr) {
               AAsset *aa = AAssetManager_open(FileUtilsAndroid::assetmanager, s, AASSET_MODE_UNKNOWN);
               if (aa) {
                   bFound = true;
                   AAsset_close(aa);
               } else {
                   // CC_LOG_DEBUG("[AssetManager] ... in APK %s, found = false!", strFilePath.c_str());
               }
            }
            else {
                auto iter = assetsMap->find(strFilePath);
                bFound = iter != assetsMap->end();
            }
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

    if (nullptr == assetsMap) {
        AAsset *asset = AAssetManager_open(assetmanager, relativePath.data(), AASSET_MODE_UNKNOWN);
        if (nullptr == asset) {
            LOGD("asset (%s) is nullptr", filename.c_str());
            return FileUtils::Status::OPEN_FAILED;
        }

        auto size = AAsset_getLength(asset);
        buffer->resize(size);

        int readsize = AAsset_read(asset, buffer->buffer(), size);
        AAsset_close(asset);

        if (readsize >= 5) {
            unsigned char *p = (unsigned char *)(buffer->buffer()) + (readsize - 5);
            // 0xee, 0xd3, 0xc9, 0xb1, 0xa6
            if (*p == 0xee && *(p + 1) == 0xd3 && *(p + 2) == 0xc9 && *(p + 3) == 0xb1 && *(p + 4) == 0xa6) {
                readsize -= 5;
                bool b1024 = (relativePath.rfind(".jpg") == relativePath.size() - 4);
                b1024 = b1024 || (relativePath.rfind(".png") == relativePath.size() - 4);
                b1024 = b1024 || (relativePath.rfind(".webp") == relativePath.size() - 5);
                b1024 = b1024 || (relativePath.rfind(".astc") == relativePath.size() - 5);
                CC20::XOR((unsigned char *)(buffer->buffer()), b1024 && isOffline ? std::min(readsize, 1024) : readsize, cc20key);
            }
        }

        if (ZipUtils::isGZipBuffer((unsigned char *)(buffer->buffer()), 2)) {
            unsigned char *out{nullptr};
            int outLen = ZipUtils::inflateMemory((unsigned char *)(buffer->buffer()),readsize, &out);
            if (outLen > 0) {
                buffer->resize(outLen);
                memcpy(buffer->buffer(), out, outLen);
                free(out);
                readsize = outLen;
            }
        }

        buffer->resize(readsize);
        return FileUtils::Status::OK;
    }
    else {
        auto iter = assetsMap->find("@assets/" + relativePath);
        if (iter == assetsMap->end())
            return FileUtils::Status::NOT_EXISTS;
        AAsset *asset = AAssetManager_open(assetmanager, assetBinPath->c_str(), AASSET_MODE_UNKNOWN);
        if (nullptr == asset) {
            LOGD("asset (%s) is nullptr", filename.c_str());
            return FileUtils::Status::OPEN_FAILED;
        }

        int a = iter->second->oriSize * (iter->second->gzip ? 2 : 1) + 5;
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
                b1024 = b1024 || (relativePath.rfind(".astc") == relativePath.size() - 5);
                CC20::XOR(pp, b1024 && isOffline ? std::min(readsize, 1024) : readsize, cc20key);
            }
        }

        if (iter->second->gzip) {
            uint32_t outLength;
            if (ZipUtils::inflateMemoryWithHint1(pp, readsize, (unsigned char *)(buffer->buffer()), &outLength, a + b))
                LOGD("asset (%s) gzip inflate fail", filename.c_str());
            buffer->resize(iter->second->oriSize);
        }
        else
            buffer->resize(readsize);

        return FileUtils::Status::OK;
    }
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

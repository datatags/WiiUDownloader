#ifdef _WIN32

#include <cstdio>
#include <curl/curl.h>
#include <direct.h>
#include <fstream>
#include <iostream>
#include <json/json.h>
#include <log.h>
#include <miniz/miniz.h>
#include <string>
#include <updater.h>
#include <utils.h>
#include <vector>
#include <version.h>

#define URL "https://github.com/Xpl0itU/WiiUDownloader/releases/latest/download/WiiUDownloader-Windows.zip"

static std::string buffer;

static size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp) {
    buffer.append(contents, size * nmemb);
    return size * nmemb;
}

static int compareVersion(std::string version1, std::string version2) {
    std::vector<int> v1, v2;
    int i = 0, j = 0;
    while (i < version1.size() || j < version2.size()) {
        int num1 = 0, num2 = 0;
        while (i < version1.size() && version1[i] != '.') {
            num1 = num1 * 10 + (version1[i] - '0');
            i++;
        }
        while (j < version2.size() && version2[j] != '.') {
            num2 = num2 * 10 + (version2[j] - '0');
            j++;
        }
        v1.push_back(num1);
        v2.push_back(num2);
        i++;
        j++;
    }
    int len = std::max(v1.size(), v2.size());
    for (int k = 0; k < len; k++) {
        if (k >= v1.size()) v1.push_back(0);
        if (k >= v2.size()) v2.push_back(0);
        if (v1[k] < v2[k]) return -1;
        else if (v1[k] > v2[k])
            return 1;
    }
    return 0;
}

static int downloadFile(const std::string &url, const std::string &filename) {
    CURL *curl;
    FILE *fp;
    CURLcode res;
    curl = curl_easy_init();
    if (curl) {
        fp = fopen(filename.c_str(), "wb");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "WiiUDownloader-Updater");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
    }
    return res;
}

static bool mkdir_p(const std::string &dir) {
    std::vector<std::string> parts;
    std::string current;

    // Split the directory path into parts
    for (char c : dir) {
        if (c == '/') {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);

    // Create the necessary intermediate directories
    current.clear();
    for (const std::string &part : parts) {
        current += "/" + part;
        if (mkdir(current.c_str()) != 0 && errno != EEXIST) {
            std::cerr << "Can't create directory: " << current << std::endl;
            return false;
        }
    }

    return true;
}

static int extractZip(const char *zipfile) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, zipfile, 0)) {
        log_error("Error opening zip file: %s\n", zipfile);
        return -1;
    }
    for (int i = 0; i < (int) mz_zip_reader_get_num_files(&zip); i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) {
            log_error("Error reading zip file: %s\n", zipfile);
            return -1;
        }
        if (!mz_zip_reader_is_file_a_directory(&zip, i)) {
            char *filename = (char *) malloc(strlen(file_stat.m_filename) + 1);
            if (!filename) {
                log_error("Error allocating filename\n");
                return -1;
            }
            sprintf(filename, "%s", file_stat.m_filename);
            char *last = strrchr(filename, '/');
            if (last) {
                *last = '\0';
                mkdir_p(filename);
                *last = '/';
            }
            if (!mz_zip_reader_extract_to_file(&zip, i, filename, 0)) {
                log_error("Error extracting zip file: %s\n", zipfile);
                free(filename);
                return -1;
            }
            free(filename);
        }
    }
    mz_zip_reader_end(&zip);
    return 0;
}

static int downloadNewestVersion() {
    int res = downloadFile(URL, "update.zip");
    if (res != CURLE_OK) {
        std::cerr << "Error downloading file: " << curl_easy_strerror(static_cast<CURLcode>(res)) << std::endl;
        return 1;
    }
    // rename exe file to exe_old
    if (std::rename("WiiUDownloader.exe", "WiiUDownloader.exe_old") != 0) {
        std::cerr << "Error renaming file" << std::endl;
        return 1;
    }
    // extract the zip to current directory
    res = extractZip("update.zip");
    if (res != 0) {
        std::cerr << "Error extracting zip" << std::endl;
        return 1;
    }
    // remove exe_old file
    if (std::remove("WiiUDownloader.exe_old") != 0) {
        std::cerr << "Error removing file" << std::endl;
        return 1;
    }
    return 0;
}

static std::string fetchLatestVersion() {
    std::string version;
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "WiiUDownloader-Updater");
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/repos/Xpl0itU/WiiUDownloader/releases/latest");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        } else {
            Json::Value root;
            Json::Reader reader;

            if (reader.parse(buffer, root)) {
                std::string releaseName = root["name"].asString();
                std::cout << "Latest release name: " << releaseName << std::endl;
                version = releaseName;
            } else {
                std::cerr << "Failed to parse JSON" << std::endl;
                version = "";
            }
        }

        curl_easy_cleanup(curl);
    }

    return version;
}

void checkAndDownloadLatestVersion() {
    if (fileExists("WiiUDownloader.exe_old"))
        remove("WiiUDownloader.exe_old");
    std::string latestVersion = fetchLatestVersion();
    if (!latestVersion.empty()) {
        latestVersion = latestVersion.substr(1, latestVersion.length() - 1);
        if (compareVersion(VERSION, latestVersion) == -1) {
            if (ask("A new update has been released, do you want to update?")) {
                if (downloadNewestVersion() != 0) {
                    showError("Error while updating!");
                } else {
                    showError("Updated successfully, WiiUDownloader will now close\nReopen it manually");
                    exit(0);
                }
            }
        }
    }
}

#endif // _WIN32
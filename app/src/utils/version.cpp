#include <borealis.hpp>
#ifdef __SWITCH__
#include <switch.h>
#include <filesystem>
#elif defined(__PS4__)
#include <orbis/Bgft.h>
#include <orbis/AppInstUtil.h>
#include <orbis/SystemService.h>
#include <sys/stat.h>
#elif defined(__PSV__)
#include <psp2/vshbridge.h>
#elif defined(ANDROID)
#include <SDL2/SDL.h>
#include <jni.h>
#elif defined(__APPLE__)
#include <SystemConfiguration/SystemConfiguration.h>
#elif defined(__linux__)
#include <borealis/platforms/desktop/steam_deck.hpp>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "utils/config.hpp"
#include "utils/dialog.hpp"
#include "utils/thread.hpp"
#include "api/http.hpp"

using namespace brls::literals;

#define STR_IMPL(x) #x
#define STR(x) STR_IMPL(x)

std::string AppVersion::getVersion() { return STR(APP_VERSION); }

std::string AppVersion::getPackageName() { return STR(BUILD_PACKAGE_NAME); }

std::string AppVersion::getCommit() { return STR(BUILD_TAG_SHORT); }

std::string AppVersion::getPlatform() {
#ifdef __SWITCH__
    return "NX";
#elif defined(__PSV__)
    return "PSVita";
#elif defined(__PS4__)
    return "PS4";
#elif defined(ANDROID)
    return "Android";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    if (getenv("SteamDeck")) return "SteamDeck";
    return "Linux";
#elif defined(_WIN32)
#if defined(_M_ARM64)
    return "Windows-arm64";
#else
    return "Windows";
#endif
#else
#error "Unsupport platform"
#endif
}

std::string AppVersion::getDeviceName() {
#ifdef __SWITCH__
    SetSysDeviceNickName nick;
    if (R_SUCCEEDED(setsysGetDeviceNickname(&nick))) {
        return nick.nickname;
    }
#elif defined(__PSV__)
    if (vshSblAimgrIsGenuineDolce()) {
        return "PSTV";
    } else if (vshSblAimgrIsGenuineVITA()) {
        char cid[0x20];
        if (_vshSblAimgrGetConsoleId(cid) >= 0) {
            if (cid[7] == 0x14 || cid[7] == 0x18) {
                return "PSVita Slim";
            }
        }
        return "PSVita";
    }
#elif defined(ANDROID)
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jclass clazz = env->FindClass("android/os/Build");
    if (clazz) {
        jfieldID fid = env->GetStaticFieldID(clazz, "MODEL", "Ljava/lang/String;");
        jstring jname = (jstring)env->GetStaticObjectField(clazz, fid);
        const char* name = env->GetStringUTFChars(jname, nullptr);
        std::string device_name = name;
        env->ReleaseStringUTFChars(jname, name);
        env->DeleteLocalRef(jname);
        env->DeleteLocalRef(clazz);
        return device_name;
    }
#elif defined(_WIN32)
    DWORD bufsize = MAX_PATH;
    std::wstring buf(bufsize, '\0');
    if (GetComputerNameW(buf.data(), &bufsize)) {
        std::string name(bufsize * 3, '\0');
        WideCharToMultiByte(CP_UTF8, 0, buf.data(), bufsize, name.data(), name.size(), nullptr, nullptr);
        return name.data();
    }
#elif defined(__APPLE__)
    CFStringRef nameRef = SCDynamicStoreCopyComputerName(nullptr, nullptr);
    if (nameRef) {
        std::vector<char> name(CFStringGetLength(nameRef) * 3);
        CFStringGetCString(nameRef, name.data(), name.size(), kCFStringEncodingUTF8);
        CFRelease(nameRef);
        return name.data();
    }
#elif defined(__linux__)
    static const char* dev_names[] = {
        "/sys/devices/virtual/dmi/id/product_name",
        "/sys/devices/virtual/dmi/id/board_name",
        "/sys/firmware/devicetree/base/model",
    };
    for (size_t i = 0; i < sizeof(dev_names); i++) {
        std::ifstream f(dev_names[i]);
        if (f.is_open()) {
            std::string name;
            std::getline(f, name);
            while (!std::isprint(name.back())) name.pop_back();
            if (name.size() > 0) return name;
        }
    }
#endif
    return fmt::format("{} for {}", getPackageName(), getPlatform());
}

#ifdef __PS4__
#include <orbis/libkernel.h>
#ifdef USE_JBC
#include <libjbc.h>
#endif

// BGFT (Background File Transfer) PKG installer for PS4
#define BGFT_HEAP_SIZE (1 * 1024 * 1024)

static OrbisBgftInitParams s_bgft_init_params;
static bool s_bgft_initialized = false;
static int s_bgft_module_handle = 0;

// File pointer for logging within bgft functions (set by caller)
static FILE* s_bgft_log = nullptr;

#ifdef USE_JBC
// Saved credentials for jailbreak
static struct jbc_cred s_orig_cred;
static struct jbc_cred s_root_cred;
static bool s_jailbroken = false;

static int jailbreak_me(void) {
    if (s_jailbroken) return 0;

    if (s_bgft_log) { fprintf(s_bgft_log, "jailbreak_me: Getting current creds...\n"); fflush(s_bgft_log); }
    if (jbc_get_cred(&s_orig_cred) != 0) {
        if (s_bgft_log) { fprintf(s_bgft_log, "jailbreak_me: jbc_get_cred failed\n"); fflush(s_bgft_log); }
        return -1;
    }

    s_root_cred = s_orig_cred;
    if (s_bgft_log) { fprintf(s_bgft_log, "jailbreak_me: Jailbreaking creds...\n"); fflush(s_bgft_log); }
    if (jbc_jailbreak_cred(&s_root_cred) != 0) {
        if (s_bgft_log) { fprintf(s_bgft_log, "jailbreak_me: jbc_jailbreak_cred failed\n"); fflush(s_bgft_log); }
        return -1;
    }

    if (s_bgft_log) { fprintf(s_bgft_log, "jailbreak_me: Setting root creds...\n"); fflush(s_bgft_log); }
    if (jbc_set_cred(&s_root_cred) != 0) {
        if (s_bgft_log) { fprintf(s_bgft_log, "jailbreak_me: jbc_set_cred failed\n"); fflush(s_bgft_log); }
        return -1;
    }

    s_jailbroken = true;
    if (s_bgft_log) { fprintf(s_bgft_log, "jailbreak_me: SUCCESS - we have root!\n"); fflush(s_bgft_log); }
    return 0;
}

static void unjailbreak_me(void) {
    if (!s_jailbroken) return;
    jbc_set_cred(&s_orig_cred);
    s_jailbroken = false;
    if (s_bgft_log) { fprintf(s_bgft_log, "unjailbreak_me: Restored original creds\n"); fflush(s_bgft_log); }
}
#endif

static int bgft_init(void) {
    if (s_bgft_initialized) return 0;

#ifdef USE_JBC
    // Get root access first so we can load system modules
    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Getting root access via JBC...\n"); fflush(s_bgft_log); }
    if (jailbreak_me() != 0) {
        if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Failed to get root access\n"); fflush(s_bgft_log); }
        return -1;
    }

    // Load AppInstUtil module (needed for sceAppInstUtilGetTitleIdFromPkg)
    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Loading AppInstUtil module...\n"); fflush(s_bgft_log); }
    int appinst_handle = sceKernelLoadStartModule("/system/common/lib/libSceAppInstUtil.sprx", 0, NULL, 0, NULL, NULL);
    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: AppInstUtil handle=%d\n", appinst_handle); fflush(s_bgft_log); }

    // Load BGFT module from system path (requires root)
    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Loading BGFT module from /system/common/lib/...\n"); fflush(s_bgft_log); }
    s_bgft_module_handle = sceKernelLoadStartModule("/system/common/lib/libSceBgft.sprx", 0, NULL, 0, NULL, NULL);
#else
    // Without JBC, try sandbox path
    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Loading module from sandbox...\n"); fflush(s_bgft_log); }
    const char* sandbox = sceKernelGetFsSandboxRandomWord();
    std::string module_path = fmt::format("/{}/common/lib/libSceBgft.sprx", sandbox ? sandbox : "");
    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Path: %s\n", module_path.c_str()); fflush(s_bgft_log); }
    s_bgft_module_handle = sceKernelLoadStartModule(module_path.c_str(), 0, NULL, 0, NULL, NULL);
#endif

    if (s_bgft_module_handle < 0) {
        if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Module load failed: 0x%08X\n", s_bgft_module_handle); fflush(s_bgft_log); }
#ifdef USE_JBC
        unjailbreak_me();
#endif
        return -1;
    }
    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Module loaded OK, handle=%d\n", s_bgft_module_handle); fflush(s_bgft_log); }

    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Allocating heap...\n"); fflush(s_bgft_log); }
    memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));
    s_bgft_init_params.heapSize = BGFT_HEAP_SIZE;
    s_bgft_init_params.heap = (uint8_t*)malloc(s_bgft_init_params.heapSize);
    if (!s_bgft_init_params.heap) {
        if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Heap alloc failed\n"); fflush(s_bgft_log); }
#ifdef USE_JBC
        unjailbreak_me();
#endif
        return -1;
    }
    memset(s_bgft_init_params.heap, 0, s_bgft_init_params.heapSize);
    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Heap OK at %p\n", (void*)s_bgft_init_params.heap); fflush(s_bgft_log); }

    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: Calling sceBgftServiceIntInit...\n"); fflush(s_bgft_log); }
    int ret = sceBgftServiceIntInit(&s_bgft_init_params);
    if (ret) {
        if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: sceBgftServiceIntInit failed: 0x%08X\n", ret); fflush(s_bgft_log); }
        free(s_bgft_init_params.heap);
        s_bgft_init_params.heap = nullptr;
#ifdef USE_JBC
        unjailbreak_me();
#endif
        return -1;
    }
    if (s_bgft_log) { fprintf(s_bgft_log, "bgft_init: sceBgftServiceIntInit OK\n"); fflush(s_bgft_log); }

    s_bgft_initialized = true;
    return 0;
}

static void bgft_fini(void) {
    if (!s_bgft_initialized) return;

    brls::Logger::info("PS4 Update: Terminating BGFT...");
    sceBgftServiceIntTerm();
    if (s_bgft_init_params.heap) {
        free(s_bgft_init_params.heap);
        s_bgft_init_params.heap = nullptr;
    }
    memset(&s_bgft_init_params, 0, sizeof(s_bgft_init_params));
    s_bgft_initialized = false;
#ifdef USE_JBC
    unjailbreak_me();
#endif
}

static int install_pkg(const char* filepath) {
    if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: called with: %s\n", filepath); fflush(s_bgft_log); }

    char titleId[18];
    memset(titleId, 0, sizeof(titleId));
    int is_app = -1;

    if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: Calling sceAppInstUtilGetTitleIdFromPkg...\n"); fflush(s_bgft_log); }
    int ret = sceAppInstUtilGetTitleIdFromPkg(filepath, titleId, &is_app);
    if (ret) {
        if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: sceAppInstUtilGetTitleIdFromPkg failed: 0x%08X\n", ret); fflush(s_bgft_log); }
        return -1;
    }

    if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: Got titleId=%s, is_app=%d\n", titleId, is_app); fflush(s_bgft_log); }

    OrbisBgftDownloadParamEx download_params;
    memset(&download_params, 0, sizeof(download_params));
    download_params.params.entitlementType = 5;
    download_params.params.id = "";
    download_params.params.contentUrl = filepath;
    download_params.params.contentName = titleId;
    download_params.params.iconPath = "";
    download_params.params.playgoScenarioId = "0";
    download_params.params.option = (OrbisBgftTaskOpt)(ORBIS_BGFT_TASK_OPT_DISABLE_CDN_QUERY_PARAM | ORBIS_BGFT_TASK_OPT_FORCE_UPDATE);
    download_params.slot = 0;

    int task_id = -1;
    if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: Calling sceBgftServiceIntDownloadRegisterTaskByStorageEx...\n"); fflush(s_bgft_log); }
    ret = sceBgftServiceIntDownloadRegisterTaskByStorageEx(&download_params, &task_id);
    if (ret) {
        if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: sceBgftServiceIntDownloadRegisterTaskByStorageEx failed: 0x%08X\n", ret); fflush(s_bgft_log); }
        return -1;
    }

    if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: Task registered, ID=0x%08X\n", task_id); fflush(s_bgft_log); }

    if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: Calling sceBgftServiceDownloadStartTask...\n"); fflush(s_bgft_log); }
    ret = sceBgftServiceDownloadStartTask(task_id);
    if (ret) {
        if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: sceBgftServiceDownloadStartTask failed: 0x%08X\n", ret); fflush(s_bgft_log); }
        return -1;
    }

    if (s_bgft_log) { fprintf(s_bgft_log, "install_pkg: Task started successfully!\n"); fflush(s_bgft_log); }
    return 0;
}
#endif

bool AppVersion::needUpdate(std::string latestVersion) { return false; }

void AppVersion::checkUpdate(int delay, bool showUpToDateDialog) {
    if (!AppVersion::updating->load()) {
        Dialog::cancelable("main/setting/others/updating"_i18n, [] { AppVersion::updating->store(true); });
        return;
    }
    ThreadPool::instance().submit([showUpToDateDialog](HTTP& s) {
        try {
            std::string url = fmt::format("https://api.github.com/repos/{}/releases/latest", git_repo);
            auto resp = HTTP::get(url, HTTP::Timeout{});
            nlohmann::json j = nlohmann::json::parse(resp);
            std::string latest_ver = j.at("tag_name").get<std::string>();
            if (latest_ver.compare(getVersion()) <= 0) {
                brls::Logger::info("App is up to date");
                if (showUpToDateDialog) brls::sync([]() { Dialog::show("main/setting/others/up2date"_i18n); });
                return;
            }

            brls::sync([latest_ver]() {
                std::string title = brls::getStr("main/setting/others/upgrade", latest_ver);
                auto dialog = new brls::Dialog(title);
                dialog->addButton("hints/cancel"_i18n, []() {
                    auto& conf = AppConfig::instance();
                    conf.setItem(AppConfig::APP_UPDATE, getVersion());
                });
#ifdef __SWITCH__
                dialog->addButton("hints/ok"_i18n, [latest_ver]() {
                    AppVersion::updating->store(false);
                    ThreadPool::instance().submit([latest_ver](HTTP& s) {
                        std::string conf_dir = AppConfig::instance().configDir();
                        std::string pkg_name = AppVersion::getPackageName();
                        std::string path = fmt::format("{}/{}_{}.nro", conf_dir, pkg_name, latest_ver);
                        std::string url = fmt::format(
                            "https://github.com/{}/releases/download/{}/Switchfin.nro", git_repo, latest_ver);
                        try {
                            HTTP::download(url, path, HTTP::Timeout{-1}, AppVersion::updating);
                            romfsExit();

                            std::string target = fmt::format("{}/{}.nro", conf_dir, pkg_name);
                            std::filesystem::remove(target);
                            std::filesystem::rename(path, target);
                            Dialog::quitApp(true);
                        } catch (const std::exception& ex) {
                            std::filesystem::remove(path);
                            AppVersion::updating->store(true);
                            std::string msg = fmt::format("{}: {}", path, ex.what());
                            brls::sync([msg]() { Dialog::show(msg); });
                        }
                    });
                });
#elif defined(__PS4__)
                dialog->addButton("hints/ok"_i18n, [latest_ver]() {
                    // Direct file logging - bypass brls::Logger entirely
                    std::string logPath = fmt::format("{}/update.log", AppConfig::instance().configDir());
                    FILE* dbgLog = fopen(logPath.c_str(), "w");
                    auto LOG = [&](const char* msg) {
                        if (dbgLog) { fprintf(dbgLog, "%s\n", msg); fflush(dbgLog); }
                    };
                    LOG("=== PS4 Update triggered ===");

                    AppVersion::updating->store(false);
                    ThreadPool::instance().submit([latest_ver, logPath](HTTP& s) {
                        FILE* dbgLog = fopen(logPath.c_str(), "a");
                        auto LOG = [&](const char* msg) {
                            if (dbgLog) { fprintf(dbgLog, "%s\n", msg); fflush(dbgLog); }
                        };

                        // Download PKG to /user/data/pkg/ - GoldHEN Package Installer finds it there
                        std::string path = "/user/data/pkg/Switchfin_update.pkg";
                        std::string url = fmt::format(
                            "https://github.com/{}/releases/download/{}/IV0001-SFIN00000_00-SFIN000000008000.pkg",
                            git_repo, latest_ver);
                        try {
                            LOG("Starting update process");
                            LOG(url.c_str());
                            LOG(path.c_str());
                            brls::sync([]() { Dialog::show("Downloading update..."); });

                            // Create pkg directory if needed
                            mkdir("/user/data/pkg", 0777);

                            // 1. Download the PKG
                            LOG("Starting PKG download");
                            HTTP::download(url, path, HTTP::Timeout{-1}, AppVersion::updating);

                            LOG("Download complete");
                            FILE* f = fopen(path.c_str(), "rb");
                            if (f) {
                                fseek(f, 0, SEEK_END);
                                long size = ftell(f);
                                fclose(f);
                                fprintf(dbgLog, "File size: %ld\n", size); fflush(dbgLog);
                            } else {
                                LOG("ERROR: Cannot open downloaded file");
                                throw std::runtime_error("Download failed - file not found");
                            }

                            // 2. Show instructions and exit
                            LOG("PKG ready at /user/data/pkg/ - showing instructions");
                            if (dbgLog) fclose(dbgLog);
                            brls::sync([]() {
                                Dialog::show("Update downloaded!\n\n1. Delete this app from home screen\n2. Go to GoldHEN Package Installer\n3. Set source to HDD and install", []() {
                                    Dialog::quitApp();
                                });
                            });
                        } catch (const std::exception& ex) {
                            fprintf(dbgLog, "EXCEPTION: %s\n", ex.what()); fflush(dbgLog);
                            if (dbgLog) fclose(dbgLog);
                            AppVersion::updating->store(true);
                            std::string msg = fmt::format("Update failed: {}", ex.what());
                            brls::sync([msg]() { Dialog::show(msg); });
                        }
                    });
                });
#else
                std::string url = fmt::format("https://github.com/{}/releases/tag/{}", git_repo, latest_ver);
                dialog->addButton("hints/ok"_i18n, [url] { brls::Application::getPlatform()->openBrowser(url); });
#endif
                dialog->open();
            });
        } catch (const std::exception& ex) {
            brls::Logger::error("checkUpdate failed: {}", ex.what());
        }
    });
}

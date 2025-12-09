#include "activity/main_activity.hpp"
#include "view/auto_tab_frame.hpp"
#include "tab/media_collection.hpp"
#include "tab/search_tab.hpp"
#include "tab/remote_tab.hpp"
#include "tab/setting_tab.hpp"
#include "utils/config.hpp"
#include "api/jellyfin.hpp"
#include "api/http.hpp"

MainActivity::MainActivity() {
    brls::Logger::debug("MainActivity: create");

    auto& conf = AppConfig::instance();
    conf.checkDanmuku();

    std::string query = HTTP::encode_form({
        {"api_key", conf.getToken()},
        {"deviceId", conf.getDeviceId()},
    });

    std::string url = fmt::format("{}/socket?{}", "ws" + conf.getUrl().substr(4), query);
    this->ws = std::make_unique<websocket>(url);
}

void MainActivity::onContentAvailable() {
    // Load libraries and add all tabs dynamically
    this->loadLibraries();
}

void MainActivity::loadLibraries() {
    // Use raw pointers for the async callback since Activity doesn't have ASYNC_RETAIN/RELEASE
    AutoTabFrame* frame = this->tabFrame;

    jellyfin::getJSON<jellyfin::Result<jellyfin::Collection>>(
        [frame](const jellyfin::Result<jellyfin::Collection>& r) {
            // Add library tabs (after Home which is already in XML)
            for (auto& item : r.Items) {
                // Determine the media type based on collection type
                std::string itemType;
                if (item.CollectionType == "tvshows")
                    itemType = jellyfin::mediaTypeSeries;
                else if (item.CollectionType == "movies")
                    itemType = jellyfin::mediaTypeMovie;
                else if (item.CollectionType == "music")
                    itemType = jellyfin::mediaTypeMusicAlbum;
                else if (item.CollectionType == "books")
                    itemType = jellyfin::mediaTypeBook;
                else if (item.CollectionType == "playlists")
                    itemType = jellyfin::mediaTypePlaylist;
                else if (item.CollectionType == "boxsets")
                    itemType = jellyfin::mediaTypeBoxSet;
                else if (item.CollectionType == "livetv")
                    continue;  // Skip live TV for now
                else
                    itemType = "";  // Generic collection

                // Create sidebar item for this library
                AutoSidebarItem* sidebarItem = new AutoSidebarItem();
                sidebarItem->setTabStyle(AutoTabBarStyle::ACCENT);
                sidebarItem->setFontSize(20);
                sidebarItem->setLabel(item.Name);

                // Capture by value for lambda
                std::string itemId = item.Id;

                frame->addTab(sidebarItem, [itemId, itemType]() {
                    return new MediaCollection(itemId, itemType);
                });
            }

            // Add Search tab
            AutoSidebarItem* searchItem = new AutoSidebarItem();
            searchItem->setTabStyle(AutoTabBarStyle::ACCENT);
            searchItem->setFontSize(20);
            searchItem->setLabel("Search");
            frame->addTab(searchItem, []() {
                return new SearchTab();
            });

            // Add Remote tab (only if remotes configured)
            if (!AppConfig::instance().getRemotes().empty()) {
                AutoSidebarItem* remoteItem = new AutoSidebarItem();
                remoteItem->setTabStyle(AutoTabBarStyle::ACCENT);
                remoteItem->setFontSize(20);
                remoteItem->setLabel("Remote");
                frame->addTab(remoteItem, []() {
                    return new RemoteTab();
                });
            }

            // Add Settings tab
            AutoSidebarItem* settingsItem = new AutoSidebarItem();
            settingsItem->setTabStyle(AutoTabBarStyle::ACCENT);
            settingsItem->setFontSize(20);
            settingsItem->setLabel("Settings");
            frame->addTab(settingsItem, []() {
                return new SettingTab();
            });
        },
        [frame](const std::string& ex) {
            brls::Logger::error("Failed to load libraries: {}", ex);

            // Even if library fetch fails, add the basic tabs
            AutoSidebarItem* searchItem = new AutoSidebarItem();
            searchItem->setTabStyle(AutoTabBarStyle::ACCENT);
            searchItem->setFontSize(20);
            searchItem->setLabel("Search");
            frame->addTab(searchItem, []() {
                return new SearchTab();
            });

            AutoSidebarItem* settingsItem = new AutoSidebarItem();
            settingsItem->setTabStyle(AutoTabBarStyle::ACCENT);
            settingsItem->setFontSize(20);
            settingsItem->setLabel("Settings");
            frame->addTab(settingsItem, []() {
                return new SettingTab();
            });
        },
        jellyfin::apiUserViews, AppConfig::instance().getUserId());
}

/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "Chat.h"
#include "ScriptObjects.h"
#include "AuctionHouseBot.h"
#include "Log.h"
#include "Config/Config.h"

#include <vector>

class AHBot_WorldScript : public WorldScript
{
private:
    bool HasPerformedStartup;
    bool ConfigurationLoaded;
    int32 UpdateTimer;

public:
    AHBot_WorldScript() : WorldScript("AHBot_WorldScript", { WORLDHOOK_ON_AFTER_CONFIG_LOAD, WORLDHOOK_ON_BEFORE_WORLD_INITIALIZED, WORLDHOOK_ON_STARTUP, WORLDHOOK_ON_UPDATE }), HasPerformedStartup(false), ConfigurationLoaded(false), UpdateTimer(0) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        // On initial startup, module scripts are not yet loaded when this hook fires,
        // so initial configuration is handled in OnBeforeWorldInitialized.
        // This hook is still used for config reloads after the server is running.
        LoadConfiguration(false);
    }

    void OnBeforeWorldInitialized() override
    {
        LoadConfiguration(true);
    }


    void OnStartup() override
    {
        if (!auctionbot->IsModuleEnabled())
            return;

        if (!ConfigurationLoaded)
            LoadConfiguration(true);

        sLog.outString("AuctionHouseBot: Running initial auction house update ...");
        auctionbot->Update();
        HasPerformedStartup = true;
        UpdateTimer = 60000; // Next automatic update in 1 minute
    }

    void OnUpdate(uint32 diff) override
    {
        if (!auctionbot->IsModuleEnabled())
            return;

        UpdateTimer -= static_cast<int32>(diff);
        if (UpdateTimer <= 0)
        {
            auctionbot->Update();
            UpdateTimer = 60000; // 1 minute
        }
    }

private:
    void LoadConfiguration(bool startup)
    {
        if (!auctionbot->IsModuleEnabled())
            return;

        auctionbot->InitializeConfiguration();
        ConfigurationLoaded = true;

        if (startup || HasPerformedStartup)
        {
            sLog.outString("AuctionHouseBot: (Re)populating item candidate lists ...");
            auctionbot->PopulateItemCandidatesAndProportions();

            if (GetConfigBool("AuctionHouseBot.AdvancedListingRules.UseDropRates.Enabled", false))
            {
                auctionbot->PopulateQuestRewardItemIDs();
                auctionbot->PopulateItemDropChances();
            }
        }
    }

    static bool GetConfigBool(char const* name, bool def)
    {
        std::string val = sConfig.GetStringDefaultInSection(name, "mod-ah-bot-plus", def ? "true" : "false");
        return val == "true" || val == "TRUE" || val == "yes" || val == "YES" || val == "1";
    }
};

class AHBot_CommandScript : public CommandScript
{
public:
    AHBot_CommandScript() : CommandScript("AHBot_CommandScript") { }

    std::vector<ChatCommand> GetCommands() const override
    {
        static std::vector<ChatCommand> AHBotCommandTable = {
            { "update", SEC_DEVELOPER, true, nullptr, "", nullptr, 0, "", 0, HandleAHBotUpdateCommand },
            { "reload", SEC_DEVELOPER, true, nullptr, "", nullptr, 0, "", 0, HandleAHBotReloadCommand },
            { "empty",  SEC_DEVELOPER, true, nullptr, "", nullptr, 0, "", 0, HandleAHBotEmptyCommand },
            { "help",   SEC_DEVELOPER, true, nullptr, "", nullptr, 0, "", 0, HandleAHBotHelpCommand }
        };

        static std::vector<ChatCommand> commandTable = {
            { "ahbot", SEC_DEVELOPER, true, nullptr, "", AHBotCommandTable.data(), 0, "", 0, nullptr },
        };

        return commandTable;
    }

    static bool HandleAHBotUpdateCommand(ChatHandler* handler, char* /*args*/)
    {
        sLog.outString("AuctionHouseBot: Updating Auction House...");
        handler->PSendSysMessage("AuctionHouseBot: Updating Auction House...");
        AuctionHouseBot::instance()->Update();
        sLog.outString("AuctionHouseBot: Auction House Updated.");
        handler->PSendSysMessage("AuctionHouseBot: Auction House Updated.");
        return true;
    }

    static bool HandleAHBotReloadCommand(ChatHandler* handler, char* /*args*/)
    {
        sLog.outString("AuctionHouseBot: Reloading Config...");
        handler->PSendSysMessage("AuctionHouseBot: Reloading Config...");

        AuctionHouseBot::instance()->InitializeConfiguration();
        AuctionHouseBot::instance()->PopulateItemCandidatesAndProportions();

        if (GetConfigBool("AuctionHouseBot.AdvancedListingRules.UseDropRates.Enabled", true))
        {
            auctionbot->PopulateQuestRewardItemIDs();
            auctionbot->PopulateItemDropChances();
        }

        sLog.outString("AuctionHouseBot: Config reloaded.");
        handler->PSendSysMessage("AuctionHouseBot: Config reloaded.");
        return true;
    }

    static bool HandleAHBotEmptyCommand(ChatHandler* handler, char* /*args*/)
    {
        sLog.outString("AuctionHouseBot: Emptying Auction House...");
        handler->PSendSysMessage("AuctionHouseBot: Emptying Auction House...");
        AuctionHouseBot::instance()->EmptyAuctionHouses();
        AuctionHouseBot::instance()->CleanupExpiredAuctionItems();
        AuctionHouseBot::instance()->CleanupBotMail();
        sLog.outString("AuctionHouseBot: Auction Houses Emptied.");
        handler->PSendSysMessage("AuctionHouseBot: Auction Houses Emptied.");
        return true;
    }

    static bool HandleAHBotHelpCommand(ChatHandler* handler, char* /*args*/)
    {
        handler->PSendSysMessage("AuctionHouseBot commands:");
        handler->PSendSysMessage("  .ahbot reload - Reloads configuration");
        handler->PSendSysMessage("  .ahbot empty  - Removes all AuctionHouseBot auctions");
        handler->PSendSysMessage("  .ahbot update - Runs an update cycle");
        return true;
    }

private:
    static bool GetConfigBool(char const* name, bool def)
    {
        std::string val = sConfig.GetStringDefaultInSection(name, "mod-ah-bot-plus", def ? "true" : "false");
        return val == "true" || val == "TRUE" || val == "yes" || val == "YES" || val == "1";
    }
};

void AddAHBotScripts()
{
    new AHBot_WorldScript();
    new AHBot_CommandScript();
}

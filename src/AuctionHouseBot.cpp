/*
 * Copyright (C) 2008-2010 Trinity <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2023+ Nathan Handley <https://github.com/NathanHandley>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ObjectMgr.h"
#include "AuctionHouseMgr.h"
#include "AuctionHouseBot.h"
#include "Config/Config.h"
#include "Player.h"
#include "WorldSession.h"
#include "Database/DatabaseEnv.h"
#include "ItemPrototype.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include "SpellEntry.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Mail.h"
#include "World.h"
#include <cmath>

#include <algorithm>
#include <functional>
#include <set>
#include <initializer_list>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

static char const* MODULE_CONFIG_SECTION = "mod-ah-bot-plus";

static bool GetConfigBool(char const* name, bool def)
{
    std::string val = sConfig.GetStringDefaultInSection(name, MODULE_CONFIG_SECTION, def ? "true" : "false");
    return val == "true" || val == "TRUE" || val == "yes" || val == "YES" || val == "1";
}

static int32 GetConfigInt(char const* name, int32 def)
{
    std::string val = sConfig.GetStringDefaultInSection(name, MODULE_CONFIG_SECTION, std::to_string(def).c_str());
    return atoi(val.c_str());
}

static uint32 GetConfigUInt(char const* name, uint32 def)
{
    return static_cast<uint32>(GetConfigInt(name, static_cast<int32>(def)));
}

static float GetConfigFloat(char const* name, float def)
{
    std::string val = sConfig.GetStringDefaultInSection(name, MODULE_CONFIG_SECTION, std::to_string(def).c_str());
    return static_cast<float>(atof(val.c_str()));
}

static std::string GetConfigString(char const* name, char const* def)
{
    return sConfig.GetStringDefaultInSection(name, MODULE_CONFIG_SECTION, def);
}

AuctionHouseBot::AuctionHouseBot() :
    debug_Out(false),
    debug_Out_Filters(false),
    SellingBotEnabled(false),
    BuyingBotEnabled(false),
    ReturnExpiredAuctionItemsToBot(false),
    CyclesBetweenBuyActionMin(1),
    CyclesBetweenBuyAction(1),
    CyclesBetweenBuyActionMax(1),
    CyclesBetweenSellActionMin(1),
    CyclesBetweenSellAction(1),
    CyclesBetweenSellActionMax(1),
    MaxBuyoutPriceInCopper(1000000000),
    CompleteItemValueOverrideEnabled(false),
    CompleteItemValueOverrideDoApplyBidVariations(false),
    CompleteItemValueOverrideDoApplyBuyoutVariations(false),
    BuyoutVariationReducePercent(0.15f),
    BuyoutVariationAddPercent(0.25f),
    BidVariationHighReducePercent(0),
    BidVariationLowReducePercent(0.25f),
    BuyoutBelowVendorVariationAddPercentEnabled(true),
    BuyoutBelowVendorVariationAddPercent(0.25f),
    BuyingBotBuyCandidatesPerBuyCycleMin(1),
    BuyingBotBuyCandidatesPerBuyCycleMax(1),
    ListingExpireTimeInSecondsMin(900),
    ListingExpireTimeInSecondsMax(86400),
    BuyingBotAcceptablePriceModifier(1),
    BuyingBotAlwaysBidMaxCalculatedPrice(false),
    BuyingBotWillBidAgainstPlayers(false),
    AHCharactersGUIDsForQuery(""),
    ItemsPerCycle(75),
    DisabledItemTextFilter(true),
    DisabledRecipeProducedItemFilterEnabled(false),
    ListedItemLevelRestrictedEnabled(false),
    ListedItemLevelRestrictedUseCraftedItemForCalculation(true),
    ListedItemLevelMin(0),
    ListedItemLevelMax(999),
    ListedItemUseOrEquipRestrictedEnabled(false),
    ListedItemUseOrEquipRestrictMinLevel(0),
    ListedItemUseOrEquipRestrictMaxLevel(999),
    RandomStackRatioConsumable(1),
    RandomStackRatioContainer(1),
    RandomStackRatioWeapon(1),
    RandomStackRatioGem(1),
    RandomStackRatioArmor(1),
    RandomStackRatioReagent(1),
    RandomStackRatioProjectile(1),
    RandomStackRatioTradeGood(1),
    RandomStackRatioGeneric(1),
    RandomStackRatioRecipe(1),
    RandomStackRatioQuiver(1),
    RandomStackRatioQuest(1),
    RandomStackRatioKey(1),
    RandomStackRatioMisc(1),
    RandomStackIncrementConsumable(1),
    RandomStackIncrementContainer(1),
    RandomStackIncrementWeapon(1),
    RandomStackIncrementGem(1),
    RandomStackIncrementArmor(1),
    RandomStackIncrementReagent(1),
    RandomStackIncrementProjectile(1),
    RandomStackIncrementTradeGood(1),
    RandomStackIncrementGeneric(1),
    RandomStackIncrementRecipe(1),
    RandomStackIncrementQuiver(1),
    RandomStackIncrementQuest(1),
    RandomStackIncrementKey(1),
    RandomStackIncrementMisc(1),
    MaximumStackSizeConsumable(0),
    MaximumStackSizeContainer(0),
    MaximumStackSizeWeapon(0),
    MaximumStackSizeGem(0),
    MaximumStackSizeArmor(0),
    MaximumStackSizeReagent(0),
    MaximumStackSizeProjectile(0),
    MaximumStackSizeTradeGood(0),
    MaximumStackSizeGeneric(0),
    MaximumStackSizeRecipe(0),
    MaximumStackSizeQuiver(0),
    MaximumStackSizeQuest(0),
    MaximumStackSizeKey(0),
    MaximumStackSizeMisc(0),
    PriceMultiplierCategoryConsumable(1),
    PriceMultiplierCategoryContainer(1),
    PriceMultiplierCategoryWeapon(1),
    PriceMultiplierCategoryGem(1),
    PriceMultiplierCategoryArmor(1),
    PriceMultiplierCategoryReagent(1),
    PriceMultiplierCategoryProjectile(1),
    PriceMultiplierCategoryTradeGood(1),
    PriceMultiplierCategoryGeneric(1),
    PriceMultiplierCategoryRecipe(1),
    PriceMultiplierCategoryQuiver(1),
    PriceMultiplierCategoryQuest(1),
    PriceMultiplierCategoryKey(1),
    PriceMultiplierCategoryMisc(1),
    PriceMultiplierItemLevelCategoryConsumable(0),
    PriceMultiplierItemLevelCategoryContainer(0),
    PriceMultiplierItemLevelCategoryWeapon(0),
    PriceMultiplierItemLevelCategoryGem(0),
    PriceMultiplierItemLevelCategoryArmor(0),
    PriceMultiplierItemLevelCategoryReagent(0),
    PriceMultiplierItemLevelCategoryProjectile(0),
    PriceMultiplierItemLevelCategoryTradeGood(0),
    PriceMultiplierItemLevelCategoryGeneric(0),
    PriceMultiplierItemLevelCategoryRecipe(0),
    PriceMultiplierItemLevelCategoryQuiver(0),
    PriceMultiplierItemLevelCategoryQuest(0),
    PriceMultiplierItemLevelCategoryKey(0),
    PriceMultiplierItemLevelCategoryMisc(0),
    PriceMultiplierQualityPoor(1),
    PriceMultiplierQualityNormal(1),
    PriceMultiplierQualityUncommon(1),
    PriceMultiplierQualityRare(1),
    PriceMultiplierQualityEpic(1),
    PriceMultiplierQualityLegendary(1),
    PriceMultiplierQualityArtifact(1),
    UseItemSellPriceIfHigherThanPriceMinimumCenterBase(true),
    PriceMinimumCenterBaseConsumable(1),
    PriceMinimumCenterBaseContainer(1),
    PriceMinimumCenterBaseWeapon(1),
    PriceMinimumCenterBaseGem(1),
    PriceMinimumCenterBaseArmor(1),
    PriceMinimumCenterBaseReagent(1),
    PriceMinimumCenterBaseProjectile(1),
    PriceMinimumCenterBaseTradeGood(1),
    PriceMinimumCenterBaseGeneric(1),
    PriceMinimumCenterBaseRecipe(1),
    PriceMinimumCenterBaseQuiver(1),
    PriceMinimumCenterBaseQuest(1),
    PriceMinimumCenterBaseKey(1),
    PriceMinimumCenterBaseMisc(1),
    ListedItemIDRestrictedEnabled(false),
    ListedItemIDMin(0),
    ListedItemIDMax(200000),
    AdvancedListingRuleUseDropRatesEnabled(false),
    AdvancedListingRuleUseDropRatesWeaponEnabled(true),
    AdvancedListingRuleUseDropRatesArmorEnabled(true),
    AdvancedListingRuleUseDropRatesRecipeEnabled(true),
    AdvancedListingRuleUseDropRatesMinDropRate(0.005),
    LastBuyCycleCount(0),
    LastSellCycleCount(0),
    ActiveListMultipleItemID(0),
    RemainingListMultipleCount(0),
    MailCleanupEnabled(true),
    MailCleanupIntervalMinutes(5),
    LastMailCleanupTime(0)
{
    AllianceConfig = FactionSpecificAuctionHouseConfig(1);
    HordeConfig = FactionSpecificAuctionHouseConfig(6);
    NeutralConfig = FactionSpecificAuctionHouseConfig(7);
}

AuctionHouseBot::~AuctionHouseBot()
{
}

uint32 AuctionHouseBot::GetStackSizeForItem(ItemPrototype const* itemProto) const
{
    // Determine the stack ratio based on class type
    if (itemProto == NULL)
        return 1;

    uint32 stackRatio = 0;
    switch (itemProto->Class)
    {
        case ITEM_CLASS_CONSUMABLE:     stackRatio = RandomStackRatioConsumable; break;
        case ITEM_CLASS_CONTAINER:      stackRatio = RandomStackRatioContainer; break;
        case ITEM_CLASS_WEAPON:         stackRatio = RandomStackRatioWeapon; break;
        case ITEM_CLASS_GEM:            stackRatio = RandomStackRatioGem; break;
        case ITEM_CLASS_REAGENT:        stackRatio = RandomStackRatioReagent; break;
        case ITEM_CLASS_ARMOR:          stackRatio = RandomStackRatioArmor; break;
        case ITEM_CLASS_PROJECTILE:     stackRatio = RandomStackRatioProjectile; break;
        case ITEM_CLASS_TRADE_GOODS:    stackRatio = RandomStackRatioTradeGood; break;
        case ITEM_CLASS_GENERIC:        stackRatio = RandomStackRatioGeneric; break;
        case ITEM_CLASS_RECIPE:         stackRatio = RandomStackRatioRecipe; break;
        case ITEM_CLASS_QUIVER:         stackRatio = RandomStackRatioQuiver; break;
        case ITEM_CLASS_QUEST:          stackRatio = RandomStackRatioQuest; break;
        case ITEM_CLASS_KEY:            stackRatio = RandomStackRatioKey; break;
        case ITEM_CLASS_JUNK:           stackRatio = RandomStackRatioMisc; break;
        default:                        stackRatio = 0; break;
    }

    uint32 stackIncrement = 1;
    switch (itemProto->Class)
    {
        case ITEM_CLASS_CONSUMABLE:     stackIncrement = RandomStackIncrementConsumable; break;
        case ITEM_CLASS_CONTAINER:      stackIncrement = RandomStackIncrementContainer; break;
        case ITEM_CLASS_WEAPON:         stackIncrement = RandomStackIncrementWeapon; break;
        case ITEM_CLASS_GEM:            stackIncrement = RandomStackIncrementGem; break;
        case ITEM_CLASS_REAGENT:        stackIncrement = RandomStackIncrementReagent; break;
        case ITEM_CLASS_ARMOR:          stackIncrement = RandomStackIncrementArmor; break;
        case ITEM_CLASS_PROJECTILE:     stackIncrement = RandomStackIncrementProjectile; break;
        case ITEM_CLASS_TRADE_GOODS:    stackIncrement = RandomStackIncrementTradeGood; break;
        case ITEM_CLASS_GENERIC:        stackIncrement = RandomStackIncrementGeneric; break;
        case ITEM_CLASS_RECIPE:         stackIncrement = RandomStackIncrementRecipe; break;
        case ITEM_CLASS_QUIVER:         stackIncrement = RandomStackIncrementQuiver; break;
        case ITEM_CLASS_QUEST:          stackIncrement = RandomStackIncrementQuest; break;
        case ITEM_CLASS_KEY:            stackIncrement = RandomStackIncrementKey; break;
        case ITEM_CLASS_JUNK:           stackIncrement = RandomStackIncrementMisc; break;
        default:                        stackIncrement = 1; break;
    }
    stackIncrement = std::max(stackIncrement, (uint32)1);

    uint32 configStackSizeMax = 0;
    switch (itemProto->Class)
    {
        case ITEM_CLASS_CONSUMABLE:     configStackSizeMax = MaximumStackSizeConsumable; break;
        case ITEM_CLASS_CONTAINER:      configStackSizeMax = MaximumStackSizeContainer; break;
        case ITEM_CLASS_WEAPON:         configStackSizeMax = MaximumStackSizeWeapon; break;
        case ITEM_CLASS_GEM:            configStackSizeMax = MaximumStackSizeGem; break;
        case ITEM_CLASS_REAGENT:        configStackSizeMax = MaximumStackSizeReagent; break;
        case ITEM_CLASS_ARMOR:          configStackSizeMax = MaximumStackSizeArmor; break;
        case ITEM_CLASS_PROJECTILE:     configStackSizeMax = MaximumStackSizeProjectile; break;
        case ITEM_CLASS_TRADE_GOODS:    configStackSizeMax = MaximumStackSizeTradeGood; break;
        case ITEM_CLASS_GENERIC:        configStackSizeMax = MaximumStackSizeGeneric; break;
        case ITEM_CLASS_RECIPE:         configStackSizeMax = MaximumStackSizeRecipe; break;
        case ITEM_CLASS_QUIVER:         configStackSizeMax = MaximumStackSizeQuiver; break;
        case ITEM_CLASS_QUEST:          configStackSizeMax = MaximumStackSizeQuest; break;
        case ITEM_CLASS_KEY:            configStackSizeMax = MaximumStackSizeKey; break;
        case ITEM_CLASS_JUNK:           configStackSizeMax = MaximumStackSizeMisc; break;
        default:                        configStackSizeMax = 0; break;
    }

    if (stackRatio > urand(0, 99))
    {
        uint32 maxPossibleStackSize = itemProto->GetMaxStackSize();
        if (configStackSizeMax != 0)
            maxPossibleStackSize = std::min(configStackSizeMax, maxPossibleStackSize);
        uint32 numOfPossibleStackIncrements = (uint32)std::ceil((float)maxPossibleStackSize / (float)stackIncrement);
        uint32 numOfStacks = urand(1, numOfPossibleStackIncrements);
        uint32 randomStackSize = numOfStacks * stackIncrement;
        if (randomStackSize > maxPossibleStackSize)
            return maxPossibleStackSize;
        else
            return randomStackSize;
    }
    else
        return 1;
}

void AuctionHouseBot::CalculateItemValue(ItemPrototype const* itemProto, uint64& outBidPrice, uint64& outBuyoutPrice)
{
    if (CompleteItemValueOverrideEnabled == true)
    {
        auto it = CompleteItemValueOverrideItemListByItemID.find(itemProto->ItemId);
        if (it != CompleteItemValueOverrideItemListByItemID.end())
        {
            outBuyoutPrice = it->second;
            if (CompleteItemValueOverrideDoApplyBuyoutVariations == true)
                outBuyoutPrice = urand(outBuyoutPrice * (1.0f - BuyoutVariationReducePercent), outBuyoutPrice * (1.0f + BuyoutVariationAddPercent));

            if (CompleteItemValueOverrideDoApplyBidVariations == true)
            {
                float sellVarianceBidPriceTopPercent = 1.0f - BidVariationHighReducePercent;
                float sellVarianceBidPriceBottomPercent = 1.0f - BidVariationLowReducePercent;
                outBidPrice = urand(sellVarianceBidPriceBottomPercent * outBuyoutPrice, sellVarianceBidPriceTopPercent * outBuyoutPrice);
            }
            else
                outBidPrice = outBuyoutPrice;

            return;
        }
    }


    // Start with a buyout price related to the sell price, if configured
    if (UseItemSellPriceIfHigherThanPriceMinimumCenterBase == true)
        outBuyoutPrice = itemProto->SellPrice;
    else
        outBuyoutPrice = 1; // Avoid zeros

    // Get the price multipliers
    float classPriceMultiplier = 1;
    switch (itemProto->Class)
    {
    case ITEM_CLASS_CONSUMABLE:     classPriceMultiplier = PriceMultiplierCategoryConsumable; break;
    case ITEM_CLASS_CONTAINER:      classPriceMultiplier = PriceMultiplierCategoryContainer; break;
    case ITEM_CLASS_WEAPON:         classPriceMultiplier = PriceMultiplierCategoryWeapon; break;
    case ITEM_CLASS_GEM:            classPriceMultiplier = PriceMultiplierCategoryGem; break;
    case ITEM_CLASS_REAGENT:        classPriceMultiplier = PriceMultiplierCategoryReagent; break;
    case ITEM_CLASS_ARMOR:          classPriceMultiplier = PriceMultiplierCategoryArmor; break;
    case ITEM_CLASS_PROJECTILE:     classPriceMultiplier = PriceMultiplierCategoryProjectile; break;
    case ITEM_CLASS_TRADE_GOODS:    classPriceMultiplier = PriceMultiplierCategoryTradeGood; break;
    case ITEM_CLASS_GENERIC:        classPriceMultiplier = PriceMultiplierCategoryGeneric; break;
    case ITEM_CLASS_RECIPE:         classPriceMultiplier = PriceMultiplierCategoryRecipe; break;
    case ITEM_CLASS_QUIVER:         classPriceMultiplier = PriceMultiplierCategoryQuiver; break;
    case ITEM_CLASS_QUEST:          classPriceMultiplier = PriceMultiplierCategoryQuest; break;
    case ITEM_CLASS_KEY:            classPriceMultiplier = PriceMultiplierCategoryKey; break;
    case ITEM_CLASS_JUNK:           classPriceMultiplier = PriceMultiplierCategoryMisc; break;
    default:                        break;
    }

    float qualityPriceMultplier = 1;
    switch (itemProto->Quality)
    {
    case ITEM_QUALITY_POOR:         qualityPriceMultplier = PriceMultiplierQualityPoor; break;
    case ITEM_QUALITY_NORMAL:       qualityPriceMultplier = PriceMultiplierQualityNormal; break;
    case ITEM_QUALITY_UNCOMMON:     qualityPriceMultplier = PriceMultiplierQualityUncommon; break;
    case ITEM_QUALITY_RARE:         qualityPriceMultplier = PriceMultiplierQualityRare; break;
    case ITEM_QUALITY_EPIC:         qualityPriceMultplier = PriceMultiplierQualityEpic; break;
    case ITEM_QUALITY_LEGENDARY:    qualityPriceMultplier = PriceMultiplierQualityLegendary; break;
    case ITEM_QUALITY_ARTIFACT:     qualityPriceMultplier = PriceMultiplierQualityArtifact; break;
    default: break;
    }

    // Custom items missing from Item.dbc skip the core's class/quality validation, so bounds check before indexing
    float classQualityPriceMultiplier = 1;
    if (itemProto->Class < MAX_ITEM_CLASS && itemProto->Quality < MAX_ITEM_QUALITY)
        classQualityPriceMultiplier = PriceMultiplierCategoryQuality[itemProto->Class][itemProto->Quality];

    float advancedPricingMultiplier = GetAdvancedPricingMultiplier(itemProto);

    // Grab the minimum prices
    uint64 PriceMinimumCenterBase = 1000;
    auto it = PriceMinimumCenterBaseOverridesByItemID.find(itemProto->ItemId);
    if (it != PriceMinimumCenterBaseOverridesByItemID.end())
        PriceMinimumCenterBase = it->second;
    else
    {
        switch (itemProto->Class)
        {
        case ITEM_CLASS_CONSUMABLE:     PriceMinimumCenterBase = PriceMinimumCenterBaseConsumable; break;
        case ITEM_CLASS_CONTAINER:      PriceMinimumCenterBase = PriceMinimumCenterBaseContainer; break;
        case ITEM_CLASS_WEAPON:         PriceMinimumCenterBase = PriceMinimumCenterBaseWeapon; break;
        case ITEM_CLASS_GEM:            PriceMinimumCenterBase = PriceMinimumCenterBaseGem; break;
        case ITEM_CLASS_REAGENT:        PriceMinimumCenterBase = PriceMinimumCenterBaseReagent; break;
        case ITEM_CLASS_ARMOR:          PriceMinimumCenterBase = PriceMinimumCenterBaseArmor; break;
        case ITEM_CLASS_PROJECTILE:     PriceMinimumCenterBase = PriceMinimumCenterBaseProjectile; break;
        case ITEM_CLASS_TRADE_GOODS:    PriceMinimumCenterBase = PriceMinimumCenterBaseTradeGood; break;
        case ITEM_CLASS_GENERIC:        PriceMinimumCenterBase = PriceMinimumCenterBaseGeneric; break;
        case ITEM_CLASS_RECIPE:         PriceMinimumCenterBase = PriceMinimumCenterBaseRecipe; break;
        case ITEM_CLASS_QUIVER:         PriceMinimumCenterBase = PriceMinimumCenterBaseQuiver; break;
        case ITEM_CLASS_QUEST:          PriceMinimumCenterBase = PriceMinimumCenterBaseQuest; break;
        case ITEM_CLASS_KEY:            PriceMinimumCenterBase = PriceMinimumCenterBaseKey; break;
        case ITEM_CLASS_JUNK:           PriceMinimumCenterBase = PriceMinimumCenterBaseMisc; break;
        default:                        break;
        }
    }

    // Set the minimum price
    if (outBuyoutPrice < PriceMinimumCenterBase)
        outBuyoutPrice = urand(PriceMinimumCenterBase * (1.0f - BuyoutVariationReducePercent), PriceMinimumCenterBase * (1.0f + BuyoutVariationAddPercent));
    else
        outBuyoutPrice = urand(outBuyoutPrice * (1.0f - BuyoutVariationReducePercent), outBuyoutPrice * (1.0f + BuyoutVariationAddPercent));

    // Ensure no multipliers are zero or negative
    if (classPriceMultiplier <= 0.0f)
        classPriceMultiplier = 1.0f;
    if (qualityPriceMultplier <= 0.0f)
        qualityPriceMultplier = 1.0f;
    if (classQualityPriceMultiplier <= 0.0f)
        classQualityPriceMultiplier = 1.0f;
    if (advancedPricingMultiplier <= 0.0f)
        advancedPricingMultiplier = 1.0f;

    // Grab any item level price multipliers
    float itemLevelPriceMultplier = 0.0f;
    switch (itemProto->Class)
    {
        case ITEM_CLASS_CONSUMABLE:     itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryConsumable; break;
        case ITEM_CLASS_CONTAINER:      itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryContainer; break;
        case ITEM_CLASS_WEAPON:         itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryWeapon; break;
        case ITEM_CLASS_GEM:            itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryGem; break;
        case ITEM_CLASS_REAGENT:        itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryReagent; break;
        case ITEM_CLASS_ARMOR:          itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryArmor; break;
        case ITEM_CLASS_PROJECTILE:     itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryProjectile; break;
        case ITEM_CLASS_TRADE_GOODS:    itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryTradeGood; break;
        case ITEM_CLASS_GENERIC:        itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryGeneric; break;
        case ITEM_CLASS_RECIPE:         itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryRecipe; break;
        case ITEM_CLASS_QUIVER:         itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryQuiver; break;
        case ITEM_CLASS_QUEST:          itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryQuest; break;
        case ITEM_CLASS_KEY:            itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryKey; break;
        case ITEM_CLASS_JUNK:           itemLevelPriceMultplier = PriceMultiplierItemLevelCategoryMisc; break;
        default:                        break;
    }

    // Multiply the price based on multipliers
    outBuyoutPrice *= qualityPriceMultplier;
    outBuyoutPrice *= classPriceMultiplier;
    outBuyoutPrice *= classQualityPriceMultiplier;
    outBuyoutPrice *= static_cast<float>(advancedPricingMultiplier);

    // Only apply item level multiplier if set, and no advanced pricing has been enabled
    if (itemLevelPriceMultplier > 0.0f && itemProto->ItemLevel > 0 && advancedPricingMultiplier == 1.0f)
        outBuyoutPrice *= itemProto->ItemLevel * itemLevelPriceMultplier;

    // Avoid price overflows
    if (outBuyoutPrice > MaxBuyoutPriceInCopper)
        outBuyoutPrice = MaxBuyoutPriceInCopper;

    // If variance brought price below sell price, bring it back up to avoid making money off vendoring AH items
    if (BuyoutBelowVendorVariationAddPercentEnabled == true && outBuyoutPrice < itemProto->SellPrice)
    {
        float minLowPriceAddVariancePercent = 1.0f + BuyoutBelowVendorVariationAddPercent;
        outBuyoutPrice = urand(itemProto->SellPrice, minLowPriceAddVariancePercent * itemProto->SellPrice);
    }

    // Calculate a bid price based on a variance against buyout price
    float sellVarianceBidPriceTopPercent = 1.0f - BidVariationHighReducePercent;
    float sellVarianceBidPriceBottomPercent = 1.0f - BidVariationLowReducePercent;
    outBidPrice = urand(sellVarianceBidPriceBottomPercent * outBuyoutPrice, sellVarianceBidPriceTopPercent * outBuyoutPrice);

    // Catch any zeros
    if (outBuyoutPrice == 0)
        outBuyoutPrice = 1;
}

// Vanilla/Tortoise item data does not use the later-expansion subclasses that the
// original advanced-pricing code expects (e.g. ITEM_SUBCLASS_CLOTH, HERB, POTION).
// Most trade goods and consumables are subclass 0, so we fall back to item-name
// matching when the subclass-specific branches did not apply.
static bool ItemNameContains(ItemPrototype const* itemProto, const char* substring)
{
    return std::string(itemProto->Name1).find(substring) != std::string::npos;
}

static bool ItemNameContainsAny(ItemPrototype const* itemProto, std::initializer_list<const char*> substrings)
{
    for (const char* substring : substrings)
        if (ItemNameContains(itemProto, substring))
            return true;
    return false;
}

float AuctionHouseBot::GetAdvancedPricingMultiplier(ItemPrototype const* itemProto)
{
    /* "ADVANCED" SUBCLASS PRICE MULTIPLIER FORMULA NOTES

      1. multiplierHelper = log(1 + b * ItemLevel)
      2.
            clothMultiplierHelper ^ p
        ---------------------------------   +   c * (clothMultiplierHelper ^ r)  -  d
          1 + a * clothMultiplierHelper

      Variables:
        b  // Influences the growth rate at low item levels
        p  // Exponent for first term (controls curve steepness)
        a  // Coefficient in denominator (dampens growth, especially at high item levels)
        c  // Scaling coefficient for second term
        r  // Exponent for second term (adds nonlinear boost)
        d  // Constant shift (adjusts baseline multiplier). This becomes apparent if you graph the equation - it shifts the entire curve down.

      Notes:
      - This formula produces a multiplier that grows logarithmically with ItemLevel (uses natural log, not base10)
      - The first term (before '+') heavily influences low item levels, the second term adds some fine-tuning for higher levels.
      - The subtraction of 'r' can help ensure low-level items don't get inflated excessively. Sometimes it isn't necessary
    */

    // Try to approximate real world prices based on subclass and item level.
    // advancedPricingMultiplier stays at 1.0 until a matching curve is found; that fact
    // is used below to decide whether to try the Vanilla name-based fallback.
    double advancedPricingMultiplier = 1.0f;
    if (itemProto->Class == ITEM_CLASS_CONSUMABLE)
    {
        switch (itemProto->SubClass)
        {
            case ITEM_SUBCLASS_POTION:
            {
                if (!AdvancedPricingConsumablePotionEnabled)
                    break;
                double potionMultiplierHelper = std::log(1.0 + (0.08 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(potionMultiplierHelper,3.0)) / (1 + (4.0 * potionMultiplierHelper))) + (std::pow(potionMultiplierHelper,2.5));
                break;
            }
            case ITEM_SUBCLASS_ELIXIR:
            {
                if (!AdvancedPricingConsumableElixirEnabled)
                    break;
                double elixirMultiplierHelper = std::log(1.0 + (1.6 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(elixirMultiplierHelper,3.1)) / (1 + (5.0 * elixirMultiplierHelper))) + (0.05 * std::pow(elixirMultiplierHelper,3.2)) - 1.0;
                break;
            }
            case ITEM_SUBCLASS_FLASK:
            {
                if (!AdvancedPricingConsumableFlaskEnabled)
                    break;
                // ITEM_SUBCLASS_FLASK is 3, but some vanilla databases also place
                // potions/elixirs at subclass 3. Require "Flask" in the name so the
                // extreme price-by-vendor-sell formula doesn't hit the wrong items.
                if (!ItemNameContains(itemProto, "Flask"))
                    break;
                // Use logarithmic scaling to compress large differences in vendorSellPrice to a range of ~22g-25g
                // advPricingMultiplier = LowTargetRange + (UpperTargetRange - LowTargetRange) * ( ln(vendorSellPrice) - ln(minVendorPrice) ) / ( ln(maxVendorPrice) - ln(minVendorPrice) ) / vendorSellPrice
                advancedPricingMultiplier = (220000 + (250000-220000) * (std::log(itemProto->SellPrice) - std::log(1250)) / (std::log(10000) - std::log(1250))) / itemProto->SellPrice;
            }
            default:
                break;
        }

        // Vanilla data frequently marks potions/elixirs/flasks as generic subclass 0.
        // Only run this fallback if no subclass-specific curve was applied above
        // and the item is actually in the generic consumable subclass.
        if (advancedPricingMultiplier == 1.0f && itemProto->SubClass == ITEM_SUBCLASS_CONSUMABLE)
        {
            if (AdvancedPricingConsumablePotionEnabled && ItemNameContains(itemProto, "Potion"))
            {
                double potionMultiplierHelper = std::log(1.0 + (0.08 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(potionMultiplierHelper,3.0)) / (1 + (4.0 * potionMultiplierHelper))) + (std::pow(potionMultiplierHelper,2.5));
            }
            else if (AdvancedPricingConsumableElixirEnabled && ItemNameContains(itemProto, "Elixir"))
            {
                double elixirMultiplierHelper = std::log(1.0 + (1.6 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(elixirMultiplierHelper,3.1)) / (1 + (5.0 * elixirMultiplierHelper))) + (0.05 * std::pow(elixirMultiplierHelper,3.2)) - 1.0;
            }
            else if (AdvancedPricingConsumableFlaskEnabled && ItemNameContains(itemProto, "Flask"))
            {
                advancedPricingMultiplier = (220000 + (250000-220000) * (std::log(itemProto->SellPrice) - std::log(1250)) / (std::log(10000) - std::log(1250))) / itemProto->SellPrice;
            }
        }
    }
    else if (itemProto->Class == ITEM_CLASS_GEM && AdvancedPricingGemEnabled)
    {
        // No switch for subclass needed since Gem subclass represents gem color
        double gemMultiplierHelper = std::log(1.0 + (0.05 * itemProto->ItemLevel));
        advancedPricingMultiplier = ((std::pow(gemMultiplierHelper,1.0)) / (1 + (10.0 * gemMultiplierHelper))) + (std::pow(gemMultiplierHelper,3.0));
    }
    else if (itemProto->Class == ITEM_CLASS_TRADE_GOODS)
    {
        switch (itemProto->SubClass)
        {
            case ITEM_SUBCLASS_CLOTH:
            {
                if (!AdvancedPricingTradeGoodClothEnabled)
                    break;
                double clothMultiplierHelper = std::log(1.0 + (itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(clothMultiplierHelper,2.0)) / (1 + (0.8 * clothMultiplierHelper))) + (0.001 * std::pow(clothMultiplierHelper,3.5)) - 0.3;
                break;
            }
            case ITEM_SUBCLASS_HERB:
            {
                if (!AdvancedPricingTradeGoodHerbEnabled)
                    break;
                double herbMultiplierHelper = std::log(1.0 + (5.0 * itemProto->ItemLevel));
                advancedPricingMultiplier = (std::pow(herbMultiplierHelper,3.0) / (1 + (1.8 * herbMultiplierHelper))) - 4.2;
                break;
            }
            case ITEM_SUBCLASS_METAL_STONE:
            {
                if (!AdvancedPricingTradeGoodMetalStoneEnabled)
                    break;
                double metalMultiplierHelper = std::log(1.0 + (75.0 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(metalMultiplierHelper,3.0)) / (1 + (7.0 * metalMultiplierHelper))) + (0.001 * std::pow(metalMultiplierHelper,3.5)) - 5.2;
                break;
            }
            case ITEM_SUBCLASS_LEATHER:
            {
                if (!AdvancedPricingTradeGoodLeatherEnabled)
                    break;
                double leatherMultiplierHelper = std::log(1.0 + (0.25 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(leatherMultiplierHelper,0.15)) / (1 + (2.0 * leatherMultiplierHelper))) + (0.4 * std::pow(leatherMultiplierHelper,3.0)) - 0.2;
                break;
            }
            case ITEM_SUBCLASS_ENCHANTING:
            {
                if (!AdvancedPricingTradeGoodEnchantingEnabled)
                    break;
                double enchantingMultiplierHelper = std::log(1.0 + (0.25 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(enchantingMultiplierHelper,0.15)) / (1 + (2.0 * enchantingMultiplierHelper))) + (0.4 * std::pow(enchantingMultiplierHelper,3.0)) - 0.2;
                break;
            }
            case ITEM_SUBCLASS_ELEMENTAL:
            {
                if (!AdvancedPricingTradeGoodElementalEnabled)
                    break;
                advancedPricingMultiplier = 85 - (itemProto->ItemLevel / 0.97);
                break;
            }
            case ITEM_SUBCLASS_MEAT:
            {
                if (!AdvancedPricingTradeGoodMeatEnabled)
                    break;
                double meatMultiplierHelper = std::log(1.0 + (0.5 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(meatMultiplierHelper,3.2)) / (1 + (2.0 * meatMultiplierHelper))) + (0.05 * std::pow(meatMultiplierHelper,3.2)) - 0.1;
                break;
            }
            default:
                break;
        }

        // Vanilla data packs most trade goods into subclass 0, so classify by name
        // when the subclass-specific branches above did not match. The order matters:
        // an item name may match multiple tokens (e.g. "Elemental" vs "Essence"),
        // so the most specific/important categories are checked first.
        if (advancedPricingMultiplier == 1.0f && itemProto->SubClass == ITEM_SUBCLASS_TRADE_GOODS)
        {
            if (AdvancedPricingTradeGoodClothEnabled && ItemNameContains(itemProto, "Cloth"))
            {
                double clothMultiplierHelper = std::log(1.0 + (itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(clothMultiplierHelper,2.0)) / (1 + (0.8 * clothMultiplierHelper))) + (0.001 * std::pow(clothMultiplierHelper,3.5)) - 0.3;
            }
            else if (AdvancedPricingTradeGoodLeatherEnabled && ItemNameContainsAny(itemProto, {"Leather", "Hide"}))
            {
                double leatherMultiplierHelper = std::log(1.0 + (0.25 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(leatherMultiplierHelper,0.15)) / (1 + (2.0 * leatherMultiplierHelper))) + (0.4 * std::pow(leatherMultiplierHelper,3.0)) - 0.2;
            }
            else if (AdvancedPricingTradeGoodMetalStoneEnabled && ItemNameContainsAny(itemProto, {"Ore", "Bar", "Stone"}))
            {
                double metalMultiplierHelper = std::log(1.0 + (75.0 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(metalMultiplierHelper,3.0)) / (1 + (7.0 * metalMultiplierHelper))) + (0.001 * std::pow(metalMultiplierHelper,3.5)) - 5.2;
            }
            else if (AdvancedPricingTradeGoodEnchantingEnabled && ItemNameContainsAny(itemProto, {"Dust", "Essence", "Shard", "Crystal"}))
            {
                double enchantingMultiplierHelper = std::log(1.0 + (0.25 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(enchantingMultiplierHelper,0.15)) / (1 + (2.0 * enchantingMultiplierHelper))) + (0.4 * std::pow(enchantingMultiplierHelper,3.0)) - 0.2;
            }
            else if (AdvancedPricingTradeGoodElementalEnabled && ItemNameContains(itemProto, "Elemental"))
            {
                advancedPricingMultiplier = 85 - (itemProto->ItemLevel / 0.97);
            }
            else if (AdvancedPricingTradeGoodHerbEnabled && ItemNameContainsAny(itemProto, {"bloom", "leaf", "root", "thorn", "weed", "grass", "lotus", "cap", "moss", "kelp", "tears", "foil", "mushroom", "whisker", "sage", "sansam", "petal"}))
            {
                double herbMultiplierHelper = std::log(1.0 + (5.0 * itemProto->ItemLevel));
                advancedPricingMultiplier = (std::pow(herbMultiplierHelper,3.0) / (1 + (1.8 * herbMultiplierHelper))) - 4.2;
            }
            else if (AdvancedPricingTradeGoodMeatEnabled && ItemNameContainsAny(itemProto, {"Meat", "Flesh", "Chunk", "Egg", "Fish"}))
            {
                double meatMultiplierHelper = std::log(1.0 + (0.5 * itemProto->ItemLevel));
                advancedPricingMultiplier = ((std::pow(meatMultiplierHelper,3.2)) / (1 + (2.0 * meatMultiplierHelper))) + (0.05 * std::pow(meatMultiplierHelper,3.2)) - 0.1;
            }
        }
    }
    else if (itemProto->Class == ITEM_CLASS_REAGENT)
    {
        // Vanilla elemental reagents (e.g. Essence of Fire) live in ITEM_CLASS_REAGENT
        // instead of ITEM_CLASS_TRADE_GOODS subclass 10. Enchanting essences are in
        // ITEM_CLASS_TRADE_GOODS, so the class check keeps them separate.
        if (AdvancedPricingTradeGoodElementalEnabled &&
            ItemNameContainsAny(itemProto, {"Elemental", "Essence", "Core", "Globe", "Heart", "Ichor"}))
        {
            advancedPricingMultiplier = 85 - (itemProto->ItemLevel / 0.97);
        }
    }
    else if (itemProto->Class == ITEM_CLASS_JUNK)
    {
        switch (itemProto->SubClass)
        {
            case ITEM_SUBCLASS_JUNK:
            {
                if (!AdvancedPricingMiscJunkEnabled)
                    break;
                double miscMultiplierHelper = std::log(1.0 + (0.12 * itemProto->ItemLevel));
                advancedPricingMultiplier = (std::pow(miscMultiplierHelper,3.2) / (1 + miscMultiplierHelper));
                break;
            }
            case ITEM_SUBCLASS_JUNK_MOUNT:
            {
                if (!AdvancedPricingMiscMountEnabled)
                    break;
                switch (itemProto->Quality)
                {
                    case ITEM_QUALITY_POOR:         advancedPricingMultiplier = PriceMultiplierCategoryMountQualityPoor;      break;
                    case ITEM_QUALITY_NORMAL:       advancedPricingMultiplier = PriceMultiplierCategoryMountQualityNormal;    break;
                    case ITEM_QUALITY_UNCOMMON:     advancedPricingMultiplier = PriceMultiplierCategoryMountQualityUncommon;  break;
                    case ITEM_QUALITY_RARE:         advancedPricingMultiplier = PriceMultiplierCategoryMountQualityRare;      break;
                    case ITEM_QUALITY_EPIC:         advancedPricingMultiplier = PriceMultiplierCategoryMountQualityEpic;      break;
                    case ITEM_QUALITY_LEGENDARY:    advancedPricingMultiplier = PriceMultiplierCategoryMountQualityLegendary; break;
                    case ITEM_QUALITY_ARTIFACT:     advancedPricingMultiplier = PriceMultiplierCategoryMountQualityArtifact;  break;
                    default: break;
                }
                break;
            }
            case ITEM_SUBCLASS_JUNK_PET:
            {
                if (!AdvancedPricingMiscPetEnabled)
                    break;
                switch (itemProto->Quality)
                {
                    case ITEM_QUALITY_POOR:         advancedPricingMultiplier = PriceMultiplierCategoryPetQualityPoor;      break;
                    case ITEM_QUALITY_NORMAL:       advancedPricingMultiplier = PriceMultiplierCategoryPetQualityNormal;    break;
                    case ITEM_QUALITY_UNCOMMON:     advancedPricingMultiplier = PriceMultiplierCategoryPetQualityUncommon;  break;
                    case ITEM_QUALITY_RARE:         advancedPricingMultiplier = PriceMultiplierCategoryPetQualityRare;      break;
                    case ITEM_QUALITY_EPIC:         advancedPricingMultiplier = PriceMultiplierCategoryPetQualityEpic;      break;
                    case ITEM_QUALITY_LEGENDARY:    advancedPricingMultiplier = PriceMultiplierCategoryPetQualityLegendary; break;
                    case ITEM_QUALITY_ARTIFACT:     advancedPricingMultiplier = PriceMultiplierCategoryPetQualityArtifact;  break;
                    default: break;
                }
                break;
            }
            default: break;
        }
    }

    return static_cast<float>(advancedPricingMultiplier);
}

ItemPrototype const* AuctionHouseBot::GetProducedItemFromRecipe(ItemPrototype const* recipeItemTemplate)
{
    if (!recipeItemTemplate)
        return nullptr;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (recipeItemTemplate->Spells[i].SpellId)
        {
            SpellEntry const* spellEntry = sSpellMgr.GetSpellEntry(recipeItemTemplate->Spells[i].SpellId);
            if (!spellEntry)
                continue;

            for (uint8 effIndex = 0; effIndex < MAX_EFFECT_INDEX; ++effIndex)
            {
                if (spellEntry->Effect[effIndex] == SPELL_EFFECT_CREATE_ITEM)
                {
                    uint32 createdItemId = spellEntry->EffectItemType[effIndex];
                    if (createdItemId)
                    {
                        ItemPrototype const* producedItem = sObjectMgr.GetItemPrototype(createdItemId);
                        if (producedItem)
                            return producedItem;
                    }
                }
            }
        }
    }

    return nullptr;
}

static const std::unordered_set<uint32> professionSkills = {
    164,  // Blacksmithing
    165,  // Leatherworking
    171,  // Alchemy
    182,  // Herbalism
    185,  // Cooking
    186,  // Mining
    197,  // Tailoring
    202,  // Engineering
    333,  // Enchanting
    356,  // Fishing
    393,  // Skinning
    129   // First Aid
};

std::unordered_set<uint32> AuctionHouseBot::GetItemIDsProducedByRecipes()
{
    std::unordered_set<uint32> recipeItemIDs;
    for (uint32 spellId = 1; spellId < sSpellMgr.GetMaxSpellId(); ++spellId)
    {
        SpellEntry const* spellEntry = sSpellMgr.GetSpellEntry(spellId);
        if (!spellEntry)
            continue;

        // Check if the spell is tied to a profession skill
        bool isProfessionSpell = false;
        SkillLineAbilityMapBounds skillBounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(spellId);
        for (SkillLineAbilityMap::const_iterator skillItr = skillBounds.first; skillItr != skillBounds.second; ++skillItr)
        {
            if (professionSkills.find(skillItr->second->skillId) != professionSkills.end())
            {
                isProfessionSpell = true;
                break;
            }
        }

        if (isProfessionSpell == false)
            continue;

        // SPELL_EFFECT_CREATE_ITEM (effect ID 24) identify created items
        for (uint8 effIndex = 0; effIndex < MAX_EFFECT_INDEX; ++effIndex)
        {
            if (spellEntry->Effect[effIndex] == SPELL_EFFECT_CREATE_ITEM && spellEntry->EffectItemType[effIndex] > 0)
            {
                uint32 itemID = spellEntry->EffectItemType[effIndex];
                recipeItemIDs.insert(itemID);
            }
        }
    }
    return recipeItemIDs;
}

bool AuctionHouseBot::IsItemADisabledRecipeProducedClassSubclass(ItemPrototype const* itemTemplate)
{
    if (DisabledRecipeProducedItemClassSubClasses.find(itemTemplate->Class) == DisabledRecipeProducedItemClassSubClasses.end())
        return false;
    else if (DisabledRecipeProducedItemClassSubClasses[itemTemplate->Class].find(itemTemplate->SubClass) == DisabledRecipeProducedItemClassSubClasses[itemTemplate->Class].end())
        return false;
    else if (ItemIDsProducedByRecipes.find(itemTemplate->ItemId) == ItemIDsProducedByRecipes.end())
        return false;
    return true;
}

void AuctionHouseBot::PopulateItemCandidatesAndProportions()
{
    // Clear old list and rebuild it
    ItemCandidatesByItemClassAndQuality.clear();

    // Item include exceptions
    set<uint32> includeItemIDExecptions;
    includeItemIDExecptions.insert(11732);
    includeItemIDExecptions.insert(11733);
    includeItemIDExecptions.insert(11734);
    includeItemIDExecptions.insert(11736);
    includeItemIDExecptions.insert(11737);
    includeItemIDExecptions.insert(18332);
    includeItemIDExecptions.insert(18333);
    includeItemIDExecptions.insert(18334);
    includeItemIDExecptions.insert(18335);

    ItemIDsProducedByRecipes.clear();
    ItemIDsProducedByRecipes = GetItemIDsProducedByRecipes();

    // Fill candidate item templates
    ItemPrototypeMap const& itemPrototypeMap = sObjectMgr.GetItemPrototypeMap();
    for (ItemPrototypeMap::const_iterator itr = itemPrototypeMap.begin(); itr != itemPrototypeMap.end(); ++itr)
    {
        // Never store curBlock zero
        if (itr->second.ItemId == 0)
            continue;

        // If there is an iLevel exception, honor it
        if (ListedItemLevelRestrictedEnabled == true)
        {
            // Only test if it's not an exception
            if (ListedItemLevelExceptionItems.find(itr->second.ItemId) == ListedItemLevelExceptionItems.end())
            {
                uint32 itemLevelToCompare = itr->second.ItemLevel;

                // Recipes might need to consider produced items
                if (ListedItemLevelRestrictedUseCraftedItemForCalculation == true && itr->second.Class == ITEM_CLASS_RECIPE)
                {
                    ItemPrototype const* producedItemTemplate = GetProducedItemFromRecipe(&itr->second);
                    if (producedItemTemplate != nullptr)
                    {
                        if (debug_Out_Filters)
                            sLog.outError("AuctionHouseBot: Using item %u for recipe %u for item level comparison since ListedItemLevelRestrictedUseCraftedItemForCalculation is true", producedItemTemplate->ItemId, itr->second.ItemId);
                        itemLevelToCompare = producedItemTemplate->ItemLevel;
                    }
                }

                if (itemLevelToCompare < ListedItemLevelMin)
                {
                    if (debug_Out_Filters)
                        sLog.outError("AuctionHouseBot: Item %u disabled since item level is lower than ListedItemLevelRestrict.MinItemLevel", itr->second.ItemId);
                    continue;
                }
                if (itemLevelToCompare > ListedItemLevelMax)
                {
                    if (debug_Out_Filters)
                        sLog.outError("AuctionHouseBot: Item %u disabled since item level is higher than ListedItemLevelRestrict.MaxItemLevel", itr->second.ItemId);
                    continue;
                }
            }
        }

        // If there is an item ID exception, honor it
        if (ListedItemIDRestrictedEnabled == true)
        {
            // Only test if it's not an exception
            if (ListedItemIDExceptionItems.find(itr->second.ItemId) == ListedItemIDExceptionItems.end())
            {
                if (itr->second.ItemId < ListedItemIDMin)
                {
                    if (debug_Out_Filters)
                        sLog.outError("AuctionHouseBot: Item %u disabled since item id is lower than ListedItemLevelRestrict.MinItemID", itr->second.ItemId);
                    continue;
                }
                if (itr->second.ItemId > ListedItemIDMax)
                {
                    if (debug_Out_Filters)
                        sLog.outError("AuctionHouseBot: Item %u disabled since item id is higher than ListedItemLevelRestrict.MaxItemID", itr->second.ItemId);
                    continue;
                }
            }
        }

        // If there is a use/equip level exception, honor it
        if (ListedItemUseOrEquipRestrictedEnabled == true)
        {
            // Only test if it's not an exception
            if (ListedItemUseOrEquipExceptionItems.find(itr->second.ItemId) == ListedItemUseOrEquipExceptionItems.end())
            {
                uint32 useOrEquipLevelCompare = itr->second.RequiredLevel;

                if (useOrEquipLevelCompare > 0 && useOrEquipLevelCompare < ListedItemUseOrEquipRestrictMinLevel)
                {
                    if (debug_Out_Filters)
                        sLog.outError("AuctionHouseBot: Item %u disabled since item use or equip level is lower than EquipItemUseOrEquipLevelRestrict.MinItemLevel", itr->second.ItemId);
                    continue;
                }
                if (useOrEquipLevelCompare > 0 && useOrEquipLevelCompare > ListedItemUseOrEquipRestrictMaxLevel)
                {
                    if (debug_Out_Filters)
                        sLog.outError("AuctionHouseBot: Item %u disabled since item use or equip level is higher than EquipItemUseOrEquipLevelRestrict.MaxItemLevel", itr->second.ItemId);
                    continue;
                }
            }
        }

        // Disabled items by Id
        if (DisabledItems.find(itr->second.ItemId) != DisabledItems.end())
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled (Configured by DisabledItemIDs and DisabledCraftedItemIDs)", itr->second.ItemId);
            continue;
        }

        // Disabled recipe item check
        if (DisabledRecipeProducedItemFilterEnabled == true)
        {
            if (IsItemADisabledRecipeProducedClassSubclass(&itr->second) == true)
            {
                if (debug_Out_Filters)
                    sLog.outError("AuctionHouseBot: Item %u disabled (Configured by DisabledRecipeProducedItemFilterEnabled and DisabledRecipeProducedItemClassSubClasses)", itr->second.ItemId);
                continue;
            }
        }

        // These items should be included and would otherwise be skipped due to conditions below
        if (includeItemIDExecptions.find(itr->second.ItemId) != includeItemIDExecptions.end())
        {
            ItemCandidatesByItemClassAndQuality[itr->second.Class][itr->second.Quality].push_back(itr->second.ItemId);
            continue;
        }

        // Skip any BOP items
        if (itr->second.Bonding == BIND_WHEN_PICKED_UP || itr->second.Bonding == BIND_QUEST_ITEM)
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled (BOP or BQI)", itr->second.ItemId);
            continue;
        }

        // Restrict quality to anything under 7 (artifact and below) or above poor
        if (itr->second.Quality == 0 || itr->second.Quality > 6)
            continue;

        // Disable conjured items
        if (itr->second.IsConjuredConsumable())
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled (Conjured Consumable)", itr->second.ItemId);
            continue;
        }

        // Disable money
        if (itr->second.Class == ITEM_CLASS_MONEY)
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled (Money)", itr->second.ItemId);
            continue;
        }

        // Disable moneyloot
        if (itr->second.MinMoneyLoot > 0)
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled (MoneyLoot)", itr->second.ItemId);
            continue;
        }

        // Disable items with duration
        if (itr->second.Duration > 0)
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled (Has a Duration)", itr->second.ItemId);
            continue;
        }

        // Disable containers with zero slots
        if (itr->second.Class == ITEM_CLASS_CONTAINER && itr->second.ContainerSlots == 0)
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled (Container with no slots)", itr->second.ItemId);
            continue;
        }

        // Disable normal class 'book' recipes, since they are junk
        if (itr->second.Class == ITEM_CLASS_RECIPE && itr->second.SubClass == ITEM_SUBCLASS_BOOK && itr->second.Quality <= ITEM_QUALITY_NORMAL)
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled (Normal or lower recipe book)", itr->second.ItemId);
            continue;
        }

        // Disable anything with the string literal of a testing or deprecated item
        if (DisabledItemTextFilter == true &&
            (itr->second.Name1.find("Test ") != std::string::npos ||
            itr->second.Name1.find("TEST") != std::string::npos ||
            itr->second.Name1.find("Deprecated") != std::string::npos ||
            itr->second.Name1.find("Depricated") != std::string::npos ||
            itr->second.Name1.find(" Epic ") != std::string::npos ||
            itr->second.Name1.find("]") != std::string::npos ||
            itr->second.Name1.find("D'Sak") != std::string::npos ||
            itr->second.Name1.find("(") != std::string::npos ||
            itr->second.Name1.find("OLD") != std::string::npos ||
            itr->second.Name1.find("PVP") != std::string::npos))
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled item with a temp or unused item name", itr->second.ItemId);
            continue;
        }

        // Disabled crafted gems that start with "Perfect"
        if (itr->second.Class == ITEM_CLASS_GEM && itr->second.Name1.find("Perfect ") != std::string::npos)
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled as it's a perfect crafted gem", itr->second.ItemId);
            continue;
        }

        // Disable all items that have neither a sell or a buy price, with exception of item enhancements and trade goods
        bool isEnchantingTradeGood = (itr->second.Class == ITEM_CLASS_TRADE_GOODS && itr->second.SubClass == ITEM_SUBCLASS_ENCHANTING);
        bool isItemEnhancement = (itr->second.Class == ITEM_CLASS_CONSUMABLE && itr->second.SubClass == ITEM_SUBCLASS_ITEM_ENHANCEMENT);
        bool hasNoPrice = (itr->second.SellPrice == 0 && itr->second.BuyPrice == 0);
        if (hasNoPrice == true && isItemEnhancement == false && isEnchantingTradeGood == false)
        {
            if (debug_Out_Filters)
                sLog.outError("AuctionHouseBot: Item %u disabled misc item", itr->second.ItemId);
            continue;
        }

        // Store the item ID
        ItemCandidatesByItemClassAndQuality[itr->second.Class][itr->second.Quality].push_back(itr->second.ItemId);
    }

    // Show any debugging information
    if (debug_Out)
    {
        sLog.outString("AHBot Candidate item counts by item category (class) and quality after appyling filters:");
        for (auto& itemCandidateQualityGroupInClass : ItemCandidatesByItemClassAndQuality)
            for (auto& itemCandidateInQualityGroup : itemCandidateQualityGroupInClass.second)
            {
                uint32 classID = itemCandidateQualityGroupInClass.first;
                uint32 qualityID = itemCandidateInQualityGroup.first;
                size_t elementCount = itemCandidateInQualityGroup.second.size();
                sLog.outString("Item count in class %u quality %u is %zu", classID, qualityID, elementCount);
            }
    }

    // Fill list proportions
    ItemListProportionNodesLookup.clear();
    for (uint32 i = 0; i < ItemListProportionNodesSeed.size(); ++i)
    {
        ListProportionNode curNode = ItemListProportionNodesSeed[i];
        if (ItemCandidatesByItemClassAndQuality[curNode.ItemClassID][curNode.ItemQualityID].size() > 0)
        {
            for (uint32 j = 0; j < curNode.Proportion; j++)
                ItemListProportionNodesLookup.push_back(curNode);
        }
    }
}

uint32 AuctionHouseBot::GetRandomItemIDForListing()
{
    // Start with a listing proportion
    if (ItemListProportionNodesLookup.size() == 0)
    {
        sLog.outError("No valid list proportion for new listing could be found (ItemListProportionNodesLookup was empty)");
        SellingBotEnabled = false;
        return 0;
    }
    ListProportionNode listProportionNode = ItemListProportionNodesLookup[urand(0, ItemListProportionNodesLookup.size() - 1)];

    // Grab an item
    size_t numOfValidItemsInGroup = ItemCandidatesByItemClassAndQuality[listProportionNode.ItemClassID][listProportionNode.ItemQualityID].size();
    if (numOfValidItemsInGroup == 0)
    {
        sLog.outError("Unable to find a candidate item with Category (class) %u and Quality %u", listProportionNode.ItemClassID, listProportionNode.ItemQualityID);
        return 0;
    }
    return ItemCandidatesByItemClassAndQuality[listProportionNode.ItemClassID][listProportionNode.ItemQualityID][urand(0, numOfValidItemsInGroup-1)];
}

void AuctionHouseBot::AddNewAuctions(std::vector<Player*> AHBPlayers, FactionSpecificAuctionHouseConfig *config)
{
    if (!SellingBotEnabled)
    {
        if (debug_Out)
            sLog.outString("AHSeller: Disabled");
        return;
    }

    uint32 minItems = config->GetMinItems();
    uint32 maxItems = config->GetMaxItems();

    if (maxItems == 0)
        return;

    AuctionHouseEntry const* ahEntry = sAuctionMgr.GetAuctionHouseEntry(config->GetAHFID());
    if (!ahEntry)
    {
        return;
    }
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
    if (!auctionHouse)
    {
        return;
    }

    uint32 currentAuctionItemListCount = auctionHouse->GetCount();
    if (currentAuctionItemListCount >= minItems)
    {
        if (debug_Out)
            sLog.outString("AHSeller: Auctions above minimum, so no auctions will be listed this cycle");
        return;
    }

    if (currentAuctionItemListCount >= maxItems)
    {
        if (debug_Out)
            sLog.outString("AHSeller: Auctions at or above maximum, so no auctions will be listed this cycle");
        return;
    }

    uint32 newItemsToListCount = 0;
    if ((maxItems - currentAuctionItemListCount) >= ItemsPerCycle)
        newItemsToListCount = ItemsPerCycle;
    else
        newItemsToListCount = (maxItems - currentAuctionItemListCount);

    if (debug_Out)
        sLog.outString("AHSeller: Adding %u Auctions", newItemsToListCount);

    if (debug_Out)
        sLog.outString("AHSeller: Current house id is %u", config->GetAHID());

    // only insert a few at a time, so as not to peg the processor
    uint32 itemsGenerated = 0;

    CharacterDatabase.BeginTransaction();

    for (uint32 cnt = 1; cnt <= newItemsToListCount; cnt++)
    {
        // GetRandomItemIDForListing can disable the seller mid-cycle, and every failed attempt must count
        // against the batch limit or a misconfigured item list spins this loop forever on the world thread
        if (!SellingBotEnabled)
            break;

        // Either generate a new item ID to list, or grab from the remaining list
        uint32 itemID;
        ItemPrototype const* prototype = nullptr;
        if (ActiveListMultipleItemID != 0)
        {
            itemID = ActiveListMultipleItemID;

            prototype = sObjectMgr.GetItemPrototype(itemID);
            if (!prototype)
            {
                if (debug_Out)
                    sLog.outError("AHSeller: prototype == NULL");
                ActiveListMultipleItemID = 0;
                continue;
            }

            RemainingListMultipleCount--;
            if (RemainingListMultipleCount <= 0)
                ActiveListMultipleItemID = 0;
        }
        else
        {
            itemID = GetRandomItemIDForListing();
            if (itemID == 0)
            {
                if (debug_Out)
                    sLog.outError("AHSeller: Item::CreateItem() failed as the ItemID is 0");
                continue;
            }

            prototype = sObjectMgr.GetItemPrototype(itemID);
            if (!prototype)
            {
                if (debug_Out)
                    sLog.outError("AHSeller: prototype == NULL");
                continue;
            }

            if (IsItemEligibleForDBDropRates(prototype))
            {
                bool foundDBDropRatesItem = HandleAdvancedListingRuleUseDropRates(prototype);
                if (foundDBDropRatesItem)
                    itemID = prototype->ItemId;
                else
                {
                    continue;
                }
            }

            if (ItemListProportionMultipliedItemIDs.find(itemID) != ItemListProportionMultipliedItemIDs.end() &&
                ItemListProportionMultipliedItemIDs[itemID] > 1)
            {
                ActiveListMultipleItemID = itemID;
                RemainingListMultipleCount = ItemListProportionMultipliedItemIDs[itemID] - 1;
                if (debug_Out)
                    sLog.outString("AHSeller: Is listing item ID %u which is configured for %u multiples from ListMultipliedItemIDs", itemID, ItemListProportionMultipliedItemIDs[itemID]);
            }
        }

        Player* AHBplayer = AHBPlayers[urand(0, AHBPlayers.size() - 1)];

        Item* item = Item::CreateItem(itemID, 1, AHBplayer);
        if (item == NULL)
        {
            if (debug_Out)
                sLog.outError("AHSeller: Item::CreateItem() returned NULL");
            break;
        }
        item->AddToUpdateQueueOf(AHBplayer);

        uint32 randomPropertyId = Item::GenerateItemRandomPropertyId(itemID);
        if (randomPropertyId != 0)
            item->SetItemRandomProperties(randomPropertyId);

        // Determine price
        uint64 buyoutPrice = 0;
        uint64 bidPrice = 0;
        CalculateItemValue(prototype, bidPrice, buyoutPrice);

        // Define a duration
        uint32 etime = urand(ListingExpireTimeInSecondsMin, ListingExpireTimeInSecondsMax);

        // Set stack size
        uint32 stackCount = GetStackSizeForItem(prototype);
        item->SetCount(stackCount);

        uint32 dep = sAuctionMgr.GetAuctionDeposit(ahEntry, etime, item);

        AuctionEntry* auctionEntry = new AuctionEntry();
        auctionEntry->Id = sObjectMgr.GenerateAuctionID();
        auctionEntry->auctionHouseEntry = ahEntry;
        auctionEntry->itemGuidLow = item->GetGUIDLow();
        auctionEntry->itemTemplate = item->GetEntry();
        auctionEntry->owner = AHBplayer->GetGUIDLow();
        auctionEntry->ownerAccount = AHBplayer->GetSession()->GetAccountId();
        auctionEntry->startbid = static_cast<uint32>(bidPrice * stackCount);
        auctionEntry->buyout = static_cast<uint32>(buyoutPrice * stackCount);
        auctionEntry->bid = 0;
        auctionEntry->bidder = 0;
        auctionEntry->deposit = dep;
        auctionEntry->expireTime = (time_t) etime + time(NULL);
        item->SaveToDB();
        item->RemoveFromUpdateQueueOf(AHBplayer);
        sAuctionMgr.AddAItem(item);
        auctionHouse->AddAuction(auctionEntry);
        auctionEntry->SaveToDB();
        itemsGenerated++;
    }

    CharacterDatabase.CommitTransaction();

    if (debug_Out)
        sLog.outString("AHSeller: Added %u items", itemsGenerated);
}

bool AuctionHouseBot::HandleAdvancedListingRuleUseDropRates(ItemPrototype const*& proto)
{
    // The AHBot has chosen a rare/epic armor/weapon/recipe, so select another item
    //   of that type based on drop rates. This way ListProportions are respected.

    // Custom items missing from Item.dbc skip the core's class/quality validation, so bounds check before indexing
    if (proto->Class >= ItemTiersByClassAndQuality.size() || proto->Quality >= ItemTiersByClassAndQuality[proto->Class].size())
        return false;

    // Roll for rarity tier
    double r = 100.0 * (urand(0, INT32_MAX) / static_cast<double>(INT32_MAX));
    int tier = GetItemDropChanceTier(r);

    // If chosen tier is empty, search more common tiers until not empty
    auto& tierBuckets = ItemTiersByClassAndQuality[proto->Class][proto->Quality];
    while (tierBuckets[tier].empty() && tier > 0) {
        tier--;
    }

    // Pull a random item from selected rarity tier
    uint32 itemID = proto->ItemId;
    auto& bucket = tierBuckets[tier];
    if (!bucket.empty())
    {
        itemID = bucket[urand(0, bucket.size() - 1)];
        proto = sObjectMgr.GetItemPrototype(itemID);
        if (!proto)
        {
            if (debug_Out)
                sLog.outError("AHSeller: prototype == NULL");
            return false;
        }
    }
    else return false;

    return true;
}

void AuctionHouseBot::PopulateItemDropChances()
{
    InitializeAdvancedListingRuleUseDropRatesTiers();

    if (!AdvancedListingRuleUseDropRatesWeaponEnabled &&
        !AdvancedListingRuleUseDropRatesArmorEnabled &&
        !AdvancedListingRuleUseDropRatesRecipeEnabled)
    {
        sLog.outError("AuctionHouseBot: No categories are enabled for AuctionHouseBot.Seller.AdvancedListingRules.UseDropRates");
        return;
    }

    auto handleDropChancesForCategoryAndQuality = [this](ItemClass category, std::set<uint32> affectedQualities) {
        std::string qualities = "";
        int i = 0;
        for (uint32 q : affectedQualities)
        {
            if (i > 0)
                qualities += ",";
            qualities += to_string(q);
            i++;
        }
        PopulateItemDropChancesForCategoryAndQuality(category, qualities);
    };

    if (AdvancedListingRuleUseDropRatesWeaponEnabled)
        handleDropChancesForCategoryAndQuality(ITEM_CLASS_WEAPON, AdvancedListingRuleUseDropRatesWeaponAffectedQualities);
    if (AdvancedListingRuleUseDropRatesArmorEnabled)
        handleDropChancesForCategoryAndQuality(ITEM_CLASS_ARMOR, AdvancedListingRuleUseDropRatesArmorAffectedQualities);
    if (AdvancedListingRuleUseDropRatesRecipeEnabled)
        handleDropChancesForCategoryAndQuality(ITEM_CLASS_RECIPE, AdvancedListingRuleUseDropRatesRecipeAffectedQualities);

    // Erase item candidates that are not crafted, not quest rewards, and missing DB drop rate
    for (auto& classQualityPair : ItemCandidatesByItemClassAndQuality)
    {
        auto& qualityGroups = classQualityPair.second;
        for (auto& qualityCandidatesPair : qualityGroups)
        {
            auto& candidates = qualityCandidatesPair.second;
            candidates.erase(
                std::remove_if(
                    candidates.begin(),
                    candidates.end(),
                    [&](uint32 id)
                    {
                        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(id);
                        if (!IsItemCategoryQualityInDBDropRatesConfig(proto))
                            return false;

                        bool shouldErase = !IsItemCrafted(id)
                                        && !IsItemQuestReward(id)
                                        && (CachedItemDropRates.find(id) == CachedItemDropRates.end());

                        return shouldErase;
                    }),
                candidates.end()
            );
        }
    }

    if (debug_Out)
    {
        // Show number of items in each tier
        sLog.outString("AHBot AdvancedListingRule UseDropRates item counts by class and quality after appyling filters:");
        for (size_t i = 0; i < ItemTiersByClassAndQuality.size(); ++i)
        {
            const auto& qualities = ItemTiersByClassAndQuality[i];
            for (size_t j = 0; j < qualities.size(); ++j)
            {
                const auto& tiers = qualities[j];
                for (size_t k = 0; k < tiers.size(); ++k)
                {
                    const auto& items = tiers[k];
                    if (i == ITEM_CLASS_WEAPON)
                        sLog.outString("Weapon Count: %s Tier %zu has %zu items", GetQualityName((ItemQualities)j), k, items.size());
                    if (i == ITEM_CLASS_ARMOR)
                        sLog.outString("Armor Count: %s Tier %zu has %zu items", GetQualityName((ItemQualities)j), k, items.size());
                    if (i == ITEM_CLASS_RECIPE)
                        sLog.outString("Recipe Count: %s Tier %zu has %zu items", GetQualityName((ItemQualities)j), k, items.size());
                }
            }
        }
    }
}

void AuctionHouseBot::PopulateItemDropChancesForCategoryAndQuality(ItemClass category, std::string qualities)
{
    if (qualities.empty())
    {
        sLog.outError("AuctionHouseBot: PopulateItemDropChancesForCategoryAndQuality() qualities are not set. "
                            "Verify that mod_ahbot.conf has values for AdvancedListingRules.UseDropRates.<Category>.AffectedQualities. "
                            "Defaulting to '2,3,4,5' to prevent crash.");
        qualities = "2,3,4,5";
    }

    // Search creature loot templates, referenced loot_loot_template, group_loot tables, and object_loot tables for items' drop rates
    std::string directDropString = R"SQL(
        SELECT clt.item AS itemID,
               ABS(clt.ChanceOrQuestChance) AS direct_chance,
               0 AS reference_chance
        FROM creature_loot_template clt
        JOIN item_template it ON it.entry = clt.item
        WHERE clt.mincountOrRef >= 0
          AND clt.groupid = 0
          AND clt.ChanceOrQuestChance != 0
          AND it.class IN (%u)
          AND it.quality IN (%s)
    )SQL";

    std::string referenceDropString = R"SQL(
        WITH creature_refs AS (
            SELECT -clt.mincountOrRef AS ref_entry,
                   LEAST(100.0, ABS(clt.ChanceOrQuestChance) * GREATEST(clt.maxcount, 1)) AS ref_chance
            FROM creature_loot_template clt
            WHERE clt.mincountOrRef < 0
        ),
        ref_direct AS (
            SELECT rlt.entry AS ref_entry,
                   rlt.item AS item_id,
                   rlt.ChanceOrQuestChance AS item_chance,
                   it.class AS itemClass,
                   it.quality AS itemQuality
            FROM reference_loot_template rlt
            JOIN item_template it ON it.entry = rlt.item
            WHERE rlt.groupid = 0
              AND rlt.mincountOrRef >= 0
        ),
        ref_group_counts AS (
            SELECT rlt.entry AS ref_entry,
                   rlt.groupid AS group_id,
                   COUNT(*) AS item_count
            FROM reference_loot_template rlt
            WHERE rlt.groupid != 0
              AND rlt.mincountOrRef >= 0
            GROUP BY rlt.entry, rlt.groupid
        ),
        ref_group AS (
            SELECT rlt.entry AS ref_entry,
                   rlt.item AS item_id,
                   rlt.ChanceOrQuestChance AS chance,
                   rgc.item_count,
                   it.class AS itemClass,
                   it.quality AS itemQuality
            FROM reference_loot_template rlt
            JOIN ref_group_counts rgc ON rlt.entry = rgc.ref_entry AND rlt.groupid = rgc.group_id
            JOIN item_template it ON it.entry = rlt.item
            WHERE rlt.groupid != 0
              AND rlt.mincountOrRef >= 0
        )
        SELECT item_id AS itemID,
               0 AS direct_chance,
               MIN(cr.ref_chance * item_drop_chance / 100.0) AS reference_chance
        FROM (
            SELECT rd.ref_entry, rd.item_id, rd.itemClass, rd.itemQuality,
                   ABS(rd.item_chance) AS item_drop_chance
            FROM ref_direct rd
            UNION ALL
            SELECT rg.ref_entry, rg.item_id, rg.itemClass, rg.itemQuality,
                   CASE
                       WHEN ABS(rg.chance) = 0 THEN (1.0 / rg.item_count) * (1 - POWER(1 - (1.0 / rg.item_count), rg.item_count)) * 100
                       ELSE ((1.0 / rg.item_count) * (1 - POWER(1 - (1.0 / rg.item_count), rg.item_count)) * 100) * ABS(rg.chance) / 100
                   END AS item_drop_chance
            FROM ref_group rg
        ) combined
        JOIN creature_refs cr ON combined.ref_entry = cr.ref_entry
        WHERE combined.itemClass IN (%u)
          AND combined.itemQuality IN (%s)
        GROUP BY item_id
    )SQL";

    // This was used to catch references not linked to any creature.  Tortoise uses a different
    // reference model, so this query is left empty; the other queries already cover the bulk
    // of obtainable items.
    std::string danglingReferenceDropString = R"SQL(
        SELECT 0 AS itemID, 0 AS direct_chance, 0 AS reference_chance FROM item_template WHERE 1 = 0
    )SQL";

    std::string groupDropString = R"SQL(
        WITH group_counts AS (
            SELECT entry, groupid, COUNT(*) AS item_count
            FROM creature_loot_template
            WHERE groupid != 0 AND mincountOrRef >= 0
            GROUP BY entry, groupid
        )
        SELECT clt.item AS itemID,
               0 AS direct_chance,
               CASE
                   WHEN ABS(clt.ChanceOrQuestChance) = 0 THEN (1.0 / gc.item_count) * (1 - POWER(1 - (1.0 / gc.item_count), gc.item_count)) * 100
                   ELSE ((1.0 / gc.item_count) * (1 - POWER(1 - (1.0 / gc.item_count), gc.item_count)) * 100) * ABS(clt.ChanceOrQuestChance) / 100
               END AS reference_chance
        FROM creature_loot_template clt
        JOIN group_counts gc ON clt.entry = gc.entry AND clt.groupid = gc.groupid
        JOIN item_template it ON it.entry = clt.item
        WHERE clt.groupid != 0
          AND clt.mincountOrRef >= 0
          AND it.class IN (%u)
          AND it.quality IN (%s)
    )SQL";

    std::string objectsDropString = R"SQL(
        SELECT it.entry AS itemID,
               ABS(ilt.ChanceOrQuestChance) AS direct_chance,
               0 AS reference_chance
        FROM item_loot_template ilt
        JOIN item_template it ON it.entry = ilt.item
        WHERE ilt.mincountOrRef >= 0
          AND ilt.groupid = 0
          AND ABS(ilt.ChanceOrQuestChance) != 0
          AND it.class IN (%u)
          AND it.quality IN (%s)
        UNION ALL
        SELECT it.entry AS itemID,
               ABS(golt.ChanceOrQuestChance) AS direct_chance,
               0 AS reference_chance
        FROM gameobject_loot_template golt
        JOIN item_template it ON it.entry = golt.item
        WHERE golt.mincountOrRef >= 0
          AND golt.groupid = 0
          AND ABS(golt.ChanceOrQuestChance) != 0
          AND it.class IN (%u)
          AND it.quality IN (%s)
    )SQL";

    QueryResult* directResult = WorldDatabase.PQuery(directDropString.c_str(), category, qualities.c_str());
    QueryResult* referenceResult = WorldDatabase.PQuery(referenceDropString.c_str(), category, qualities.c_str());
    QueryResult* danglingReferenceResult = WorldDatabase.PQuery(danglingReferenceDropString.c_str(), category, qualities.c_str());
    QueryResult* groupResult = WorldDatabase.PQuery(groupDropString.c_str(), category, qualities.c_str());
    QueryResult* objectsDropResult = WorldDatabase.PQuery(objectsDropString.c_str(),
                                                        category, qualities.c_str(),
                                                        category, qualities.c_str());
    if (!directResult || !referenceResult || !danglingReferenceResult || !groupResult || !objectsDropResult)
    {
        sLog.outError("AuctionHouseBot: PopulateItemDropChances() failed to query items' drop rates.");
        delete directResult;
        delete referenceResult;
        delete danglingReferenceResult;
        delete groupResult;
        delete objectsDropResult;
        return;
    }

    // Add drop rate of all results to CachedItemDropRates
    auto parseResults = [this](QueryResult* result, bool overwriteDropRate)
    {
        do {
            Field* fields = result->Fetch();
            double directDropChance = 0.0;
            double referenceDropChance = 0.0;
            uint32 itemID = fields[0].GetUInt32();

            // Ignore quest rewards and crafted items, they have "100%" drop rate
            if (IsItemQuestReward(itemID) || IsItemCrafted(itemID))
                continue;

            if (!overwriteDropRate && CachedItemDropRates.find(itemID) != CachedItemDropRates.end())
                continue;

            if (!fields[1].IsNULL())
                directDropChance = fields[1].GetFloat();
            if (!fields[2].IsNULL())
                referenceDropChance = fields[2].GetFloat();

            // Choose higher of two rates (one is normally 0), then raise to MinDropRate if less than
            double higherDropChance = (directDropChance > referenceDropChance) ? directDropChance : referenceDropChance;
            higherDropChance = (higherDropChance < AdvancedListingRuleUseDropRatesMinDropRate) ? AdvancedListingRuleUseDropRatesMinDropRate : higherDropChance;

            auto it = CachedItemDropRates.find(itemID);
            if (it == CachedItemDropRates.end() || it->second < higherDropChance)
                CachedItemDropRates[itemID] = higherDropChance;

        } while (result->NextRow());
    };

    parseResults(directResult, true);
    parseResults(referenceResult, true);
    parseResults(danglingReferenceResult, false);
    parseResults(groupResult, true);
    parseResults(objectsDropResult, true);

    delete directResult;
    delete referenceResult;
    delete danglingReferenceResult;
    delete groupResult;
    delete objectsDropResult;

    // Populate drop rates Tiers
    for (auto& classQualityPair : ItemCandidatesByItemClassAndQuality)
    {
        auto& qualityGroups = classQualityPair.second;
        for (auto& qualityCandidatesPair : qualityGroups)
        {
            auto& candidates = qualityCandidatesPair.second;
            for (uint32 id : candidates)
            {
                ItemPrototype const* proto = sObjectMgr.GetItemPrototype(id);
                if (!proto || !IsItemCategoryQualityInDBDropRatesConfig(proto))
                    continue;

                // Custom items missing from Item.dbc skip the core's class/quality validation, so bounds check
                if (proto->Class >= ItemTiersByClassAndQuality.size() || proto->Quality >= ItemTiersByClassAndQuality[proto->Class].size())
                    continue;
                // Skip items that haven't been populated yet.
                if (CachedItemDropRates.find(id) == CachedItemDropRates.end())
                    continue;

                double rate = CachedItemDropRates[id];
                int tier = GetItemDropChanceTier(rate);
                ItemTiersByClassAndQuality[proto->Class][proto->Quality][tier].push_back(id);
            }
        }
    }

    // Remove duplicates
    for (auto& byClass : ItemTiersByClassAndQuality)
        for (auto& byQuality : byClass)
            for (auto& byTier : byQuality) {
                std::sort(byTier.begin(), byTier.end());
                byTier.erase(std::unique(byTier.begin(), byTier.end()), byTier.end());
            }
}

void AuctionHouseBot::InitializeAdvancedListingRuleUseDropRatesTiers()
{
    // Re-initialize to reset
    DropRatesToTierMap = std::map<double, int, std::greater<double>>();
    ItemTiersByClassAndQuality = std::vector<std::vector<std::vector<std::vector<uint32>>>>(MAX_ITEM_CLASS);
    std::string tiersConfigString = GetConfigString("AuctionHouseBot.AdvancedListingRules.UseDropRates.TiersConfig", "50,10,5,2,1,0.5,0.2,0.1,0.05,0.02,0.01,0.005");

    // Parse tiers config, populate DropRate -> Tier map for later lookups
    int curTier = 0;
    std::string delimitedValue;
    std::stringstream tiersConfigStream;
    tiersConfigStream.str(tiersConfigString);
    while (std::getline(tiersConfigStream, delimitedValue, ',')) // Process each tier in the string, delimited by the comma ","
    {
        delimitedValue.erase(0, delimitedValue.find_first_not_of(" \t"));
        delimitedValue.erase(delimitedValue.find_last_not_of(" \t") + 1);

        if (delimitedValue.empty())
        {
            sLog.outError("AuctionHouseBot: Empty entry found in AuctionHouseBot.AdvancedListingRules.UseDropRates.TiersConfig: '%s'", tiersConfigString.c_str());
            continue;
        }

        double rate = std::stod(delimitedValue);
        if (rate <= 0.0)
        {
            sLog.outError("AuctionHouseBot: Invalid (non-positive) drop rate '%s' in AuctionHouseBot.AdvancedListingRules.UseDropRates.TiersConfig: '%s'", delimitedValue.c_str(), tiersConfigString.c_str());
            continue;
        }

        // Check for duplicates
        if (DropRatesToTierMap.find(rate) != DropRatesToTierMap.end())
        {
            sLog.outError("AuctionHouseBot: Duplicate drop rate '%f' found in AuctionHouseBot.AdvancedListingRules.UseDropRates.TiersConfig: '%s'", rate, tiersConfigString.c_str());
            continue;
        }

        DropRatesToTierMap[rate] = curTier;
        curTier++;
    }

    if (DropRatesToTierMap.empty())
        sLog.outError("AuctionHouseBot: Failed to parse any valid drop rates from AuctionHouseBot.AdvancedListingRules.UseDropRates.TiersConfig: '%s'", tiersConfigString.c_str());

    // Resize ItemTiersByClassAndQuality by tier count
    for (auto& qualities : ItemTiersByClassAndQuality)
    {
        qualities.resize(MAX_ITEM_QUALITY);
        for (auto& tiers : qualities)
            tiers.resize(DropRatesToTierMap.size() + 1); // Create an extra "catch-all" bucket
    }
}

void AuctionHouseBot::PopulateQuestRewardItemIDs()
{
    string questRewardsString = R"SQL(
        SELECT DISTINCT item_id
            FROM (
                SELECT RewItemId1 AS item_id FROM quest_template UNION ALL
                SELECT RewItemId2 AS item_id FROM quest_template UNION ALL
                SELECT RewItemId3 AS item_id FROM quest_template UNION ALL
                SELECT RewItemId4 AS item_id FROM quest_template UNION ALL
                SELECT ReqItemId1 AS item_id FROM quest_template UNION ALL
                SELECT ReqItemId2 AS item_id FROM quest_template UNION ALL
                SELECT ReqItemId3 AS item_id FROM quest_template UNION ALL
                SELECT ReqItemId4 AS item_id FROM quest_template UNION ALL
                SELECT RewChoiceItemId1 AS item_id FROM quest_template UNION ALL
                SELECT RewChoiceItemId2 AS item_id FROM quest_template UNION ALL
                SELECT RewChoiceItemId3 AS item_id FROM quest_template UNION ALL
                SELECT RewChoiceItemId4 AS item_id FROM quest_template UNION ALL
                SELECT RewChoiceItemId5 AS item_id FROM quest_template UNION ALL
                SELECT RewChoiceItemId6 AS item_id FROM quest_template
            ) AS quest_rewards
            WHERE item_id != 0
    )SQL";

    QueryResult* questRewardsResult = WorldDatabase.Query(questRewardsString.c_str());
    if (!questRewardsResult)
    {
        sLog.outError("AuctionHouseBot: Quest Rewards lookup failed.");
        return;
    }

    do
    {
        uint32 id = questRewardsResult->Fetch()->GetUInt32();
        QuestRewardItemIDs.insert(id);
    } while (questRewardsResult->NextRow());

    delete questRewardsResult;
}

int AuctionHouseBot::GetItemDropChanceTier(double dropRate)
{
    for(const auto& pair : DropRatesToTierMap)
    {
        double mapDropRate = pair.first;
        int mapTier = pair.second;
        if (dropRate >= mapDropRate)
            return mapTier;
    }

    // If dropRate is lower than smallest configured tier, return "catch-all" bucket index
    return DropRatesToTierMap.size();
}

void AuctionHouseBot::AddNewAuctionBuyerBotBid(std::vector<Player*> AHBPlayers, FactionSpecificAuctionHouseConfig *config)
{
    if (!BuyingBotEnabled)
    {
        if (debug_Out)
            sLog.outString("AHBuyer: Disabled");
        return;
    }

    AuctionHouseEntry const* ahEntry = sAuctionMgr.GetAuctionHouseEntry(config->GetAHFID());
    if (!ahEntry)
        return;

    // Fetches content of selected AH
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
    if (!auctionHouse)
        return;

    // Pull currentAuctionItemListCount.
    std::string queryString = "SELECT id FROM auction WHERE itemowner NOT IN (%s) AND buyguid NOT IN (%s)";

    if (BuyingBotWillBidAgainstPlayers == false)
        queryString = "SELECT id FROM auction WHERE itemowner NOT IN (%s) AND buyguid NOT IN (%s) AND lastbid = 0";
    QueryResult* result = CharacterDatabase.PQuery(queryString.c_str(), AHCharactersGUIDsForQuery.c_str(), AHCharactersGUIDsForQuery.c_str());

    if (!result)
        return;

    if (result->GetRowCount() == 0)
    {
        delete result;
        return;
    }

    vector<uint32> possibleBids;

    do
    {
        uint32 tmpdata = result->Fetch()->GetUInt32();
        possibleBids.push_back(tmpdata);
    } while (result->NextRow());

    delete result;

    int randBuyingBotBuyCandidatesPerBuyCycle = urand(BuyingBotBuyCandidatesPerBuyCycleMin, BuyingBotBuyCandidatesPerBuyCycleMax);
    for (int count = 1; count <= randBuyingBotBuyCandidatesPerBuyCycle; ++count)
    {
        // Do we have anything to bid? If not, stop here.
        if (possibleBids.empty())
        {
            //if (debug_Out) sLog->outError( "AHBuyer: I have no items to bid on.");
            count = randBuyingBotBuyCandidatesPerBuyCycle;
            continue;
        }

        // Choose random auction from possible currentAuctionItemListCount
        uint32 vectorPos = urand(0, possibleBids.size() - 1);
        vector<uint32>::iterator iter = possibleBids.begin();
        advance(iter, vectorPos);

        // from auctionhousehandler.cpp, creates auction pointer & player pointer
        AuctionEntry* auction = auctionHouse->GetAuction(*iter);

        // Erase the auction from the vector to prevent bidding on item in next iteration.
        possibleBids.erase(iter);

        if (!auction)
            continue;

        // get exact item information
        Item *pItem = sAuctionMgr.GetAItem(auction->itemGuidLow);
        if (!pItem || pItem->GetCount() == 0)
        {
            if (debug_Out)
                sLog.outError("AHBuyer: Item %u doesn't exist, perhaps bought already?", auction->itemGuidLow);
            continue;
        }

        // get item prototype
        ItemPrototype const* prototype = sObjectMgr.GetItemPrototype(auction->itemTemplate);
        if (!prototype)
        {
            if (debug_Out)
                sLog.outError("AHBuyer: Item template %u for auction %u does not exist, skipping", auction->itemTemplate, auction->Id);
            continue;
        }

        // Calculate a potential price for the item
        uint64 willingToSpendPerItemPrice = 0;
        uint64 discardBidPrice = 0;
        CalculateItemValue(prototype, discardBidPrice, willingToSpendPerItemPrice);
        willingToSpendPerItemPrice = (uint64)((float)willingToSpendPerItemPrice * BuyingBotAcceptablePriceModifier);
        uint64 willingToPayForStackPrice = willingToSpendPerItemPrice * pItem->GetCount();

        // Determine if it's a bid, buyout, or skip
        bool doBuyout = false;
        bool doBid = false;
        uint64 calcBidAmount = 0;

        if (auction->buyout != 0 && auction->buyout < willingToPayForStackPrice)
            doBuyout = true;
        else
        {
            if (auction->bid == 0 && auction->startbid <= willingToPayForStackPrice)
            {
                doBid = true;
                if (BuyingBotAlwaysBidMaxCalculatedPrice == true)
                    calcBidAmount = willingToPayForStackPrice;
                else
                    calcBidAmount = auction->startbid;
            }
            else if (auction->bid != 0 && (auction->bid + auction->GetAuctionOutBid()) < willingToPayForStackPrice)
            {
                doBid = true;
                if (BuyingBotAlwaysBidMaxCalculatedPrice == true)
                    calcBidAmount = willingToPayForStackPrice;
                else
                    calcBidAmount = auction->bid + auction->GetAuctionOutBid();
            }
        }

        // Check that the item isn't listed above Vendor sell price. Out-of-range entries (items added after the
        // price table was built) get UINT32_MAX, which behaves the same as "not sold by a vendor"
        bool preventedOverpayingForVendorItem = false;
        uint32 vendorSellPrice = prototype->ItemId < vendorItemsPrices.size() ? vendorItemsPrices[prototype->ItemId] : UINT32_MAX;
        if (PreventOverpayingForVendorItems && vendorSellPrice > 0)
        {
            if (doBuyout && auction->buyout > vendorSellPrice)
            {
                doBuyout = false;
                preventedOverpayingForVendorItem = true;
            }
            if (doBid && calcBidAmount > vendorSellPrice)
            {
                doBid = false;
                preventedOverpayingForVendorItem = true;
            }
        }

        if (debug_Out)
        {
            sLog.outString("-------------------------------------------------");
            sLog.outString("AHBuyer: Info for Auction #%u:", auction->Id);
            sLog.outString("AHBuyer: AuctionHouse: %u", auction->GetHouseId());
            sLog.outString("AHBuyer: Owner: %u", auction->owner);
            sLog.outString("AHBuyer: Bidder: %u", auction->bidder);
            sLog.outString("AHBuyer: Expire Time: %u", uint32(auction->expireTime));
            sLog.outString("AHBuyer: Item GUID: %u", auction->itemGuidLow);
            sLog.outString("AHBuyer: Item Template: %u", auction->itemTemplate);
            sLog.outString("AHBuyer: Item Info:");
            sLog.outString("AHBuyer: Item ID: %u", prototype->ItemId);
            sLog.outString("AHBuyer: Vendor Buy Price: %u", prototype->BuyPrice);
            sLog.outString("AHBuyer: Vendor Sell Price (Base): %u", prototype->SellPrice);
            if (PreventOverpayingForVendorItems == true)
                sLog.outString("AHBuyer: Vender Sell Price (Vendor): %u", vendorSellPrice);
            sLog.outString("AHBuyer: Deposit: %u", auction->deposit);
            sLog.outString("AHBuyer: Bonding: %u", prototype->Bonding);
            sLog.outString("AHBuyer: Quality: %u", prototype->Quality);
            sLog.outString("AHBuyer: Item Level: %u", prototype->ItemLevel);
            sLog.outString("AHBuyer: Ammo Type: %u", prototype->AmmoType);
            sLog.outString("AHBuyer: Stack Size: %u", pItem->GetCount());
            sLog.outString("AHBuyer: Starting Bid: %u", auction->startbid);
            sLog.outString("AHBuyer: Current Bid: %u", auction->bid);
            sLog.outString("AHBuyer: Buyout Price: %u", auction->buyout);
            sLog.outString("AHBuyer: Willing To Pay Per Item Price (Buyout): %u", willingToSpendPerItemPrice);
            sLog.outString("AHBuyer: Willing To Pay For Stack Price (Buyout): %u", willingToPayForStackPrice);
            sLog.outString("AHBuyer: Calculated Bid Amount (0 means too expensive to bid): %u", calcBidAmount);
            sLog.outString("AHBuyer: Decided to Buyout?: %s", doBuyout ? "true" : "false");
            sLog.outString("AHBuyer: Decided to Bid?: %s", doBid ? "true" : "false");
            sLog.outString("AHBuyer: Stopped from buying due to 'PreventOverpayingForVendorItems'?: %s", preventedOverpayingForVendorItem ? "true" : "false");
            sLog.outString("-------------------------------------------------");
        }

        Player* AHBplayer = AHBPlayers[urand(0, AHBPlayers.size() - 1)];

        if (doBid)
        {
            if (auction->bidder && auction->bidder != AHBplayer->GetGUIDLow())
            {
                ObjectGuid oldBidderGuid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
                if (Player* oldBidder = sObjectMgr.GetPlayer(oldBidderGuid))
                {
                    std::ostringstream subject;
                    subject << auction->itemTemplate << ":0:" << AUCTION_OUTBIDDED;
                    MailDraft(subject.str())
                        .SetMoney(auction->bid)
                        .SendMailTo(MailReceiver(oldBidder, oldBidderGuid), auction, MAIL_CHECK_MASK_COPIED);
                }
            }

            auction->bidder = AHBplayer->GetGUIDLow();
            auction->bid = static_cast<uint32>(calcBidAmount);

            CharacterDatabase.PExecute("UPDATE auction SET buyguid = '%u', lastbid = '%u' WHERE id = '%u'",
                auction->bidder, auction->bid, auction->Id);
        }
        else if (doBuyout)
        {
            if (auction->bidder && auction->bidder != AHBplayer->GetGUIDLow())
            {
                ObjectGuid oldBidderGuid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
                if (Player* oldBidder = sObjectMgr.GetPlayer(oldBidderGuid))
                {
                    std::ostringstream subject;
                    subject << auction->itemTemplate << ":0:" << AUCTION_OUTBIDDED;
                    MailDraft(subject.str())
                        .SetMoney(auction->bid)
                        .SendMailTo(MailReceiver(oldBidder, oldBidderGuid), auction, MAIL_CHECK_MASK_COPIED);
                }
            }

            auction->bidder = AHBplayer->GetGUIDLow();
            auction->bid = auction->buyout;

            CharacterDatabase.PExecute("UPDATE auction SET buyguid = '%u', lastbid = '%u' WHERE id = '%u'",
                auction->bidder, auction->bid, auction->Id);
            // Send mails to buyer & seller
            sAuctionMgr.SendAuctionSuccessfulMail(auction);
            sAuctionMgr.SendAuctionWonMail(auction);
            auction->DeleteFromDB();

            sAuctionMgr.RemoveAItem(auction->itemGuidLow);
            auctionHouse->RemoveAuction(auction);
            delete auction;
        }
    }
}

void AuctionHouseBot::Update()
{
    if (AHCharacters.empty() == true)
    {
        sLog.outError("AuctionHouseBot: Update() aborted - no bot characters configured");
        return;
    }

    if ((SellingBotEnabled == false) && (BuyingBotEnabled == false))
    {
        sLog.outError("AuctionHouseBot: Update() aborted - seller and buyer are both disabled");
        return;
    }

    LastBuyCycleCount++;
    LastSellCycleCount++;

    bool buyReady = false;
    bool sellReady = false;

    // Check if an update cycle has been hit. Set # of cycles until next update
    if (LastBuyCycleCount >= CyclesBetweenBuyAction) // Should never be >, but we check anyway
    {
        LastBuyCycleCount = 0;
        CyclesBetweenBuyAction = urand(CyclesBetweenBuyActionMin, CyclesBetweenBuyActionMax);
        buyReady = true;
    }
    if (LastSellCycleCount >= CyclesBetweenSellAction) // Should never be >, but we check anyway
    {
        LastSellCycleCount = 0;
        CyclesBetweenSellAction = urand(CyclesBetweenSellActionMin, CyclesBetweenSellActionMax);
        sellReady = true;
    }

    // Only update if a Buy or Sell update cycle has been hit
    if (!buyReady && !sellReady)
        return;

    if (MailCleanupEnabled && (time(NULL) - LastMailCleanupTime) >= (time_t)(MailCleanupIntervalMinutes * 60))
    {
        CleanupBotMail();
        LastMailCleanupTime = time(NULL);
    }

    // Load all AH Bot Players
    std::vector<std::pair<Player*, WorldSession*>> AHBPlayers;
    AHBPlayers.reserve(AHCharacters.size());
    for (uint32 botIndex = 0; botIndex < AHCharacters.size(); ++botIndex)
    {
        CurrentBotCharGUID = AHCharacters[botIndex].CharacterGUID;
        std::string accountName = "AuctionHouseBot" + std::to_string(AHCharacters[botIndex].AccountID);

        // Wrap session and player in unique pointer to manage lifetime
        WorldSession* session = new WorldSession(
            AHCharacters[botIndex].AccountID, nullptr, SEC_PLAYER, 0, LOCALE_enUS, accountName, 0, SessionTransport::Network
        );
        Player* player = new Player(session);
        player->Initialize(AHCharacters[botIndex].CharacterGUID);
        sObjectAccessor.AddObject(player);
        AHBPlayers.emplace_back(player, session);
    }

    // Create a vector of Player* for passing to methods
    std::vector<Player*> playersPointerVector;
    playersPointerVector.reserve(AHBPlayers.size());
    for (const auto& pair : AHBPlayers)
        playersPointerVector.emplace_back(pair.first);

     if (sellReady)
     {
         if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION) == false)
         {
             AddNewAuctions(playersPointerVector, &AllianceConfig);
             AddNewAuctions(playersPointerVector, &HordeConfig);
         }
         AddNewAuctions(playersPointerVector, &NeutralConfig);
     }

     // Place New Bids
     if (buyReady && BuyingBotBuyCandidatesPerBuyCycleMin > 0)
     {
         if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION) == false)
         {
             AddNewAuctionBuyerBotBid(playersPointerVector, &AllianceConfig);
             AddNewAuctionBuyerBotBid(playersPointerVector, &HordeConfig);
         }
         AddNewAuctionBuyerBotBid(playersPointerVector, &NeutralConfig);
     }

    // Remove AH Bot Players from world
    for (auto& pair : AHBPlayers)
    {
        Player* player = pair.first;
        WorldSession* session = pair.second;
        sObjectAccessor.RemoveObject(player);
        delete player;
        delete session;
    }
}

bool AuctionHouseBot::IsModuleEnabled()
{
    bool sellerEnabled = GetConfigBool("AuctionHouseBot.EnableSeller", false);
    bool buyerEnabled = GetConfigBool("AuctionHouseBot.Buyer.Enabled", false);
    if (sellerEnabled == false && buyerEnabled == false)
        return false;
    std::string charString = GetConfigString("AuctionHouseBot.GUIDs", "0");
    if (charString == "0" || charString.empty())
    {
        sLog.outString("AuctionHouseBot: AuctionHouseBot.GUIDs is not configured so this module will be disabled");
        return false;
    }
    return true;
}

void AuctionHouseBot::InitializeConfiguration()
{
    debug_Out = GetConfigBool("AuctionHouseBot.DEBUG", false);
    debug_Out_Filters = GetConfigBool("AuctionHouseBot.DEBUG_FILTERS", false);

    SellingBotEnabled = GetConfigBool("AuctionHouseBot.EnableSeller", false);
    BuyingBotEnabled = GetConfigBool("AuctionHouseBot.Buyer.Enabled", false);

    std::string charString = GetConfigString("AuctionHouseBot.GUIDs", "0");
    AddCharacters(charString);

    // Top level overrides
    CompleteItemValueOverrideEnabled = GetConfigBool("AuctionHouseBot.CompleteItemValueOverride.Enabled", false);
    AddItemValuePairsToItemIDMap(CompleteItemValueOverrideItemListByItemID, GetConfigString("AuctionHouseBot.CompleteItemValueOverride.Items", ""));
    CompleteItemValueOverrideDoApplyBidVariations = GetConfigBool("AuctionHouseBot.CompleteItemValueOverride.DoApplyBidVariations", false);
    CompleteItemValueOverrideDoApplyBuyoutVariations = GetConfigBool("AuctionHouseBot.CompleteItemValueOverride.DoApplyBuyoutVariations", false);

    // Buyer & Seller core properties
    SetCyclesBetweenBuyOrSell();
    ReturnExpiredAuctionItemsToBot = GetConfigBool("AuctionHouseBot.ReturnExpiredAuctionItemsToBot", false);
    ItemsPerCycle = GetConfigUInt("AuctionHouseBot.ItemsPerCycle", 75);
    AdvancedListingRuleUseDropRatesEnabled = GetConfigBool("AuctionHouseBot.AdvancedListingRules.UseDropRates.Enabled", false);
    AdvancedListingRuleUseDropRatesWeaponEnabled = GetConfigBool("AuctionHouseBot.AdvancedListingRules.UseDropRates.Weapon", true);
    AdvancedListingRuleUseDropRatesArmorEnabled = GetConfigBool("AuctionHouseBot.AdvancedListingRules.UseDropRates.Armor", true);
    AdvancedListingRuleUseDropRatesRecipeEnabled = GetConfigBool("AuctionHouseBot.AdvancedListingRules.UseDropRates.Recipe", true);
    AdvancedListingRuleUseDropRatesMinDropRate = GetConfigFloat("AuctionHouseBot.AdvancedListingRules.UseDropRates.MinDropRate", 0.005f);
    if (AdvancedListingRuleUseDropRatesMinDropRate < 0 || AdvancedListingRuleUseDropRatesMinDropRate > 100) AdvancedListingRuleUseDropRatesMinDropRate = 0.005f;
    AdvancedListingRuleUseDropRatesWeaponAffectedQualities.clear();
    AdvancedListingRuleUseDropRatesArmorAffectedQualities.clear();
    AdvancedListingRuleUseDropRatesRecipeAffectedQualities.clear();
    ParseNumberListToSet(AdvancedListingRuleUseDropRatesWeaponAffectedQualities, GetConfigString("AuctionHouseBot.AdvancedListingRules.UseDropRates.Weapon.AffectedQualities", "2,3,4,5"), "AdvancedListingRules.UseDropRates.Weapon.AffectedQualities");
    ParseNumberListToSet(AdvancedListingRuleUseDropRatesArmorAffectedQualities, GetConfigString("AuctionHouseBot.AdvancedListingRules.UseDropRates.Armor.AffectedQualities", "2,3,4,5"), "AdvancedListingRules.UseDropRates.Armor.AffectedQualities");
    ParseNumberListToSet(AdvancedListingRuleUseDropRatesRecipeAffectedQualities, GetConfigString("AuctionHouseBot.AdvancedListingRules.UseDropRates.Recipe.AffectedQualities", "2,3,4,5"), "AdvancedListingRules.UseDropRates.Recipe.AffectedQualities");
    AdvancedListingRuleUseDropRatesExceptionItems.clear();
    ParseNumberListToSet(AdvancedListingRuleUseDropRatesExceptionItems, GetConfigString("AuctionHouseBot.AdvancedListingRules.UseDropRates.DisabledItemIDs", ""), "AdvancedListingRules.UseDropRates.DisabledItemIDs");
    MaxBuyoutPriceInCopper = GetConfigUInt("AuctionHouseBot.MaxBuyoutPriceInCopper", 1000000000);
    BuyoutVariationReducePercent = GetConfigFloat("AuctionHouseBot.BuyoutVariationReducePercent", 0.15f);
    BuyoutVariationAddPercent = GetConfigFloat("AuctionHouseBot.BuyoutVariationAddPercent", 0.25f);
    BidVariationHighReducePercent = GetConfigFloat("AuctionHouseBot.BidVariationHighReducePercent", 0);
    BidVariationLowReducePercent = GetConfigFloat("AuctionHouseBot.BidVariationLowReducePercent", 0.25f);
    BuyoutBelowVendorVariationAddPercentEnabled = GetConfigBool("AuctionHouseBot.BuyoutBelowVendorVariationAddPercentEnabled", true);
    BuyoutBelowVendorVariationAddPercent = GetConfigFloat("AuctionHouseBot.BuyoutBelowVendorVariationAddPercent", 0.25f);
    ListingExpireTimeInSecondsMin = GetConfigUInt("AuctionHouseBot.ListingExpireTimeInSecondsMin", 900);
    if (ListingExpireTimeInSecondsMin < 900)
    {
        sLog.outError("AuctionHouseBot: ListingExpireTimeInSecondsMin was set below 900 (15 min), so setting to 900");
        ListingExpireTimeInSecondsMin = 900;
    }
    ListingExpireTimeInSecondsMax = GetConfigUInt("AuctionHouseBot.ListingExpireTimeInSecondsMax", 86400);
    if (ListingExpireTimeInSecondsMax > 172800)
    {
        sLog.outError("AuctionHouseBot: ListingExpireTimeInSecondsMax was set above 172800 (48 hours), so setting to 172800");
        ListingExpireTimeInSecondsMax = 172800;
    }
    if (ListingExpireTimeInSecondsMax < ListingExpireTimeInSecondsMin)
    {
        sLog.outError("AuctionHouseBot: ListingExpireTimeInSecondsMax was smaller than ListingExpireTimeInSecondsMin, setting to 172800 (48 hours) and 900 (15 min)");
        ListingExpireTimeInSecondsMin = 900;
        ListingExpireTimeInSecondsMax = 172800;
    }

    // Buyer Bot
    SetBuyingBotBuyCandidatesPerBuyCycle();
    BuyingBotAcceptablePriceModifier = GetConfigFloat("AuctionHouseBot.Buyer.AcceptablePriceModifier", 1);
    BuyingBotAlwaysBidMaxCalculatedPrice = GetConfigBool("AuctionHouseBot.Buyer.AlwaysBidMaxCalculatedPrice", false);
    PreventOverpayingForVendorItems = GetConfigBool("AuctionHouseBot.Buyer.PreventOverpayingForVendorItems", true);
    if (PreventOverpayingForVendorItems)
        PopulateVendorItemsPrices();
    BuyingBotWillBidAgainstPlayers = GetConfigBool("AuctionHouseBot.Buyer.BidAgainstPlayers", false);

    MailCleanupEnabled = GetConfigBool("AuctionHouseBot.MailCleanup.Enabled", true);
    MailCleanupIntervalMinutes = GetConfigUInt("AuctionHouseBot.MailCleanup.IntervalMinutes", 5);

    // Stack Ratios
    RandomStackRatioConsumable = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Consumable", 50);
    RandomStackRatioContainer = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Container", 0);
    RandomStackRatioWeapon = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Weapon", 0);
    RandomStackRatioGem = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Gem", 5);
    RandomStackRatioArmor = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Armor", 0);
    RandomStackRatioReagent = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Reagent", 50);
    RandomStackRatioProjectile = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Projectile", 100);
    RandomStackRatioTradeGood = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.TradeGood", 50);
    RandomStackRatioGeneric = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Generic", 100);
    RandomStackRatioRecipe = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Recipe", 0);
    RandomStackRatioQuiver = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Quiver", 0);
    RandomStackRatioQuest = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Quest", 10);
    RandomStackRatioKey = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Key", 10);
    RandomStackRatioMisc = GetRandomStackValue("AuctionHouseBot.ListingStack.RandomRatio.Misc", 100);

    // Stack Increments
    RandomStackIncrementConsumable = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Consumable", 5);
    RandomStackIncrementContainer = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Container", 1);
    RandomStackIncrementWeapon = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Weapon", 1);
    RandomStackIncrementGem = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Gem", 1);
    RandomStackIncrementArmor = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Armor", 1);
    RandomStackIncrementReagent = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Reagent", 1);
    RandomStackIncrementProjectile = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Projectile", 1000);
    RandomStackIncrementTradeGood = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.TradeGood", 5);
    RandomStackIncrementGeneric = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Generic", 1);
    RandomStackIncrementRecipe = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Recipe", 1);
    RandomStackIncrementQuiver = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Quiver", 1);
    RandomStackIncrementQuest = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Quest", 1);
    RandomStackIncrementKey = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Key", 1);
    RandomStackIncrementMisc = GetRandomStackIncrementValue("AuctionHouseBot.ListingStack.RandomStackIncrement.Misc", 1);

    // Max stack size
    MaximumStackSizeConsumable = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Consumable", 0);
    MaximumStackSizeContainer = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Container", 0);
    MaximumStackSizeWeapon = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Weapon", 0);
    MaximumStackSizeGem = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Gem", 0);
    MaximumStackSizeArmor = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Armor", 0);
    MaximumStackSizeReagent = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Reagent", 0);
    MaximumStackSizeProjectile = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Projectile", 0);
    MaximumStackSizeTradeGood = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.TradeGood", 0);
    MaximumStackSizeGeneric = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Generic", 0);
    MaximumStackSizeRecipe = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Recipe", 0);
    MaximumStackSizeQuiver = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Quiver", 0);
    MaximumStackSizeQuest = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Quest", 0);
    MaximumStackSizeKey = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Key", 0);
    MaximumStackSizeMisc = GetConfigUInt("AuctionHouseBot.ListingStack.MaxStackSize.Misc", 0);

    // List proportions
    ItemListProportionNodesSeed.clear();
    for (int category = 0; category < MAX_ITEM_CLASS; category++)
    {
        for (int quality = 0; quality < MAX_ITEM_QUALITY; quality++)
        {
            if (category != ITEM_CLASS_MONEY && category != ITEM_CLASS_PERMANENT)
            {
                std::string key = std::string("AuctionHouseBot.ListProportion.Category") + GetCategoryName((ItemClass)category) + ".Quality" + GetQualityName((ItemQualities)quality);
                ListProportionNode node;
                node.ItemClassID = category;
                node.ItemQualityID = quality;
                node.Proportion = GetConfigUInt(key.c_str(), 0);
                ItemListProportionNodesSeed.push_back(node);
            }
        }
    }
    ItemListProportionMultipliedItemIDs.clear();
    AddItemValuePairsToItemIDMap(ItemListProportionMultipliedItemIDs, GetConfigString("AuctionHouseBot.ListProportion.ListMultipliedItemIDs", ""));

    PriceMultiplierCategoryConsumable = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Consumable", 1);
    PriceMultiplierCategoryContainer = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Container", 1);
    PriceMultiplierCategoryWeapon = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Weapon", 1);
    PriceMultiplierCategoryGem = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Gem", 1);
    PriceMultiplierCategoryArmor = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Armor", 1);
    PriceMultiplierCategoryReagent = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Reagent", 1);
    PriceMultiplierCategoryProjectile = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Projectile", 1);
    PriceMultiplierCategoryTradeGood = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.TradeGood", 1);
    PriceMultiplierCategoryGeneric = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Generic", 1);
    PriceMultiplierCategoryRecipe = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Recipe", 1);
    PriceMultiplierCategoryQuiver = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Quiver", 1);
    PriceMultiplierCategoryQuest = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Quest", 1);
    PriceMultiplierCategoryKey = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Key", 1);
    PriceMultiplierCategoryMisc = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Category.Misc", 1);
    PriceMultiplierItemLevelCategoryConsumable = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Consumable", 0);
    PriceMultiplierItemLevelCategoryContainer = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Container", 0);
    PriceMultiplierItemLevelCategoryWeapon = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Weapon", 0);
    PriceMultiplierItemLevelCategoryGem = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Gem", 0);
    PriceMultiplierItemLevelCategoryArmor = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Armor", 0);
    PriceMultiplierItemLevelCategoryReagent = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Reagent", 0);
    PriceMultiplierItemLevelCategoryProjectile = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Projectile", 0);
    PriceMultiplierItemLevelCategoryTradeGood = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.TradeGood", 0);
    PriceMultiplierItemLevelCategoryGeneric = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Generic", 0);
    PriceMultiplierItemLevelCategoryRecipe = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Recipe", 0);
    PriceMultiplierItemLevelCategoryQuiver = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Quiver", 0);
    PriceMultiplierItemLevelCategoryQuest = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Quest", 0);
    PriceMultiplierItemLevelCategoryKey = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Key", 0);
    PriceMultiplierItemLevelCategoryMisc = GetConfigFloat("AuctionHouseBot.PriceMultiplier.ItemLevel.Category.Misc", 0);
    PriceMultiplierQualityPoor = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Quality.Poor", 1);
    PriceMultiplierQualityNormal = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Quality.Normal", 1);
    PriceMultiplierQualityUncommon = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Quality.Uncommon", 1.8f);
    PriceMultiplierQualityRare = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Quality.Rare", 1.9f);
    PriceMultiplierQualityEpic = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Quality.Epic", 2.1f);
    PriceMultiplierQualityLegendary = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Quality.Legendary", 3);
    PriceMultiplierQualityArtifact = GetConfigFloat("AuctionHouseBot.PriceMultiplier.Quality.Artifact", 3);
    for (int category = 0; category < MAX_ITEM_CLASS; category++)
    {
        for (int quality = 0; quality < MAX_ITEM_QUALITY; quality++)
        {
            std::string key = std::string("AuctionHouseBot.PriceMultiplier.Category") + GetCategoryName((ItemClass)category) +
                            ".Quality" + GetQualityName((ItemQualities)quality);

            float multiplier = GetConfigFloat(key.c_str(), 1.0f);
            PriceMultiplierCategoryQuality[category][quality] = multiplier;
        }
    }
    PriceMultiplierCategoryMountQualityPoor = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityPoor", 1.0f);
    PriceMultiplierCategoryMountQualityNormal = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityNormal", 1.0f);
    PriceMultiplierCategoryMountQualityUncommon = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityUncommon", 1.0f);
    PriceMultiplierCategoryMountQualityRare = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityRare", 3000.0f);
    PriceMultiplierCategoryMountQualityEpic = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityEpic", 5750.0f);
    PriceMultiplierCategoryMountQualityLegendary = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityLegendary", 1.0f);
    PriceMultiplierCategoryMountQualityArtifact = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryMount.QualityArtifact", 1.0f);

    PriceMultiplierCategoryPetQualityPoor = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityPoor", 1.0f);
    PriceMultiplierCategoryPetQualityNormal = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityNormal", 1.0f);
    PriceMultiplierCategoryPetQualityUncommon = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityUncommon", 1.0f);
    PriceMultiplierCategoryPetQualityRare = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityRare", 1.0f);
    PriceMultiplierCategoryPetQualityEpic = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityEpic", 1.0f);
    PriceMultiplierCategoryPetQualityLegendary = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityLegendary", 1.0f);
    PriceMultiplierCategoryPetQualityArtifact = GetConfigFloat("AuctionHouseBot.PriceMultiplier.CategoryPet.QualityArtifact", 1.0f);

    // Advanced Pricing
    AdvancedPricingConsumablePotionEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.Consumable.Potion.Enabled", true);
    AdvancedPricingConsumableElixirEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.Consumable.Elixir.Enabled", true);
    AdvancedPricingConsumableFlaskEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.Consumable.Flask.Enabled", true);
    AdvancedPricingGemEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.Gem.Enabled", true);
    AdvancedPricingTradeGoodClothEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.TradeGood.Cloth.Enabled", true);
    AdvancedPricingTradeGoodHerbEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.TradeGood.Herb.Enabled", true);
    AdvancedPricingTradeGoodMetalStoneEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.TradeGood.MetalStone.Enabled", true);
    AdvancedPricingTradeGoodLeatherEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.TradeGood.Leather.Enabled", true);
    AdvancedPricingTradeGoodEnchantingEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.TradeGood.Enchanting.Enabled", true);
    AdvancedPricingTradeGoodElementalEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.TradeGood.Elemental.Enabled", true);
    AdvancedPricingTradeGoodMeatEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.TradeGood.Meat.Enabled", true);
    AdvancedPricingMiscJunkEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.Misc.Junk.Enabled", true);
    AdvancedPricingMiscMountEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.Misc.Mount.Enabled", true);
    AdvancedPricingMiscPetEnabled = GetConfigBool("AuctionHouseBot.AdvancedPricing.Misc.Pet.Enabled", true);

    // Price minimums
    UseItemSellPriceIfHigherThanPriceMinimumCenterBase = GetConfigBool("AuctionHouseBot.PriceMinimumCenterBase.UseItemSellPriceIfHigher", true);
    PriceMinimumCenterBaseConsumable = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Consumable",1000);
    PriceMinimumCenterBaseContainer = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Container", 1000);
    PriceMinimumCenterBaseWeapon = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Weapon", 1000);
    PriceMinimumCenterBaseGem = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Gem", 1000);
    PriceMinimumCenterBaseArmor = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Armor", 1000);
    PriceMinimumCenterBaseReagent = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Reagent", 1000);
    PriceMinimumCenterBaseProjectile = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Projectile", 5);
    PriceMinimumCenterBaseTradeGood = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.TradeGood", 850);
    PriceMinimumCenterBaseGeneric = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Generic", 1000);
    PriceMinimumCenterBaseRecipe = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Recipe", 1000);
    PriceMinimumCenterBaseQuiver = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Quiver", 1000);
    PriceMinimumCenterBaseQuest = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Quest", 1000);
    PriceMinimumCenterBaseKey = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Key", 1000);
    PriceMinimumCenterBaseMisc = GetConfigUInt("AuctionHouseBot.PriceMinimumCenterBase.Misc", 1000);
    AddItemValuePairsToItemIDMap(PriceMinimumCenterBaseOverridesByItemID, GetConfigString("AuctionHouseBot.PriceMinimumCenterBase.OverrideItems", ""));

    // Item level Restrictions
    ListedItemLevelRestrictedEnabled = GetConfigBool("AuctionHouseBot.ListedItemLevelRestrict.Enabled", false);
    ListedItemLevelRestrictedUseCraftedItemForCalculation = GetConfigBool("AuctionHouseBot.ListedItemLevelRestrict.UseCraftedItemForCalculation", true);
    ListedItemLevelMin = GetConfigUInt("AuctionHouseBot.ListedItemLevelRestrict.MinItemLevel", 0);
    ListedItemLevelMax = GetConfigUInt("AuctionHouseBot.ListedItemLevelRestrict.MaxItemLevel", 999);
    ListedItemLevelExceptionItems.clear();
    ParseNumberListToSet(ListedItemLevelExceptionItems, GetConfigString("AuctionHouseBot.ListedItemLevelRestrict.ExceptionItemIDs", ""), "ListedItemLevelRestrict.ExceptionItemIDs");

    // Item ID Restrictions
    ListedItemIDRestrictedEnabled = GetConfigBool("AuctionHouseBot.ListedItemIDRestrict.Enabled", false);
    ListedItemIDMin = GetConfigUInt("AuctionHouseBot.ListedItemIDRestrict.MinItemID", 0);
    ListedItemIDMax = GetConfigUInt("AuctionHouseBot.ListedItemIDRestrict.MaxItemID", 200000);
    ListedItemIDExceptionItems.clear();
    ParseNumberListToSet(ListedItemIDExceptionItems, GetConfigString("AuctionHouseBot.ListedItemIDRestrict.ExceptionItemIDs", ""), "ListedItemLevelRestrict.ExceptionItemIDs");

    // Equip or use restrictions
    ListedItemUseOrEquipRestrictedEnabled = GetConfigBool("AuctionHouseBot.EquipItemUseOrEquipLevelRestrict.Enabled", false);
    ListedItemUseOrEquipRestrictMinLevel = GetConfigUInt("AuctionHouseBot.EquipItemUseOrEquipLevelRestrict.MinLevel", 0);
    ListedItemUseOrEquipRestrictMaxLevel = GetConfigUInt("AuctionHouseBot.EquipItemUseOrEquipLevelRestrict.MaxLevel", 999);
    ListedItemUseOrEquipExceptionItems.clear();
    ParseNumberListToSet(ListedItemUseOrEquipExceptionItems, GetConfigString("AuctionHouseBot.EquipItemUseOrEquipLevelRestrict.ExceptionItemIDs", ""), "EquipItemUseOrEquipLevelRestrict.ExceptionItemIDs");

    // Disabled Items
    DisabledItemTextFilter = GetConfigBool("AuctionHouseBot.DisabledItemTextFilter", true);
    DisabledRecipeProducedItemFilterEnabled = GetConfigBool("AuctionHouseBot.DisabledRecipeProducedItemFilterEnabled", false);
    DisabledItems.clear();
    ParseNumberListToSet(DisabledItems, GetConfigString("AuctionHouseBot.DisabledInvalidItemIDs", ""), "AuctionHouseBot.DisabledInvalidItemIDs");
    ParseNumberListToSet(DisabledItems, GetConfigString("AuctionHouseBot.DisabledCustomItemIDs", ""), "AuctionHouseBot.DisabledCustomItemIDs");
    AddValuesToSetByKeyMap(DisabledRecipeProducedItemClassSubClasses, GetConfigString("AuctionHouseBot.DisabledRecipeProducedItemClassSubClasses", ""), 0, 20);

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
    {
        AllianceConfig.SetMinItems(GetConfigUInt("AuctionHouseBot.Alliance.MinItems", 15000));
        AllianceConfig.SetMaxItems(GetConfigUInt("AuctionHouseBot.Alliance.MaxItems", 15000));

        HordeConfig.SetMinItems(GetConfigUInt("AuctionHouseBot.Horde.MinItems", 15000));
        HordeConfig.SetMaxItems(GetConfigUInt("AuctionHouseBot.Horde.MaxItems", 15000));
    }
    NeutralConfig.SetMinItems(GetConfigUInt("AuctionHouseBot.Neutral.MinItems", 15000));
    NeutralConfig.SetMaxItems(GetConfigUInt("AuctionHouseBot.Neutral.MaxItems", 15000));
}

void AuctionHouseBot::EmptyAuctionHouses()
{
    if (AHCharactersGUIDsForQuery.empty())
    {
        sLog.outError("AuctionHouseBot: No character GUIDs found when emptying Auction Houses via '.ahbot empty' .");
        return;
    }

    struct AuctionInfo {
        uint32 itemID {0};
        uint32 characterGUID {0};
        uint32 houseID {0};
    };
    vector<AuctionInfo> ahBotActiveAuctions;

    CharacterDatabase.BeginTransaction();

    // Get all auctions owned by AHBots
    std::string queryString = "SELECT id, buyguid, houseid FROM auction WHERE itemowner IN (%s)";
    QueryResult* result = CharacterDatabase.PQuery(queryString.c_str(), AHCharactersGUIDsForQuery.c_str());
    if (!result)
    {
        CharacterDatabase.CommitTransaction();
        return;
    }

    if (result->GetRowCount() > 0)
    {
        // Load results into vector<AuctionInfo> for lookups later
        do
        {
            Field* fields = result->Fetch();
            AuctionInfo ai = {
                fields[0].GetUInt32(),
                fields[1].GetUInt32(),
                fields[2].GetUInt32()
            };
            ahBotActiveAuctions.push_back(ai);
        } while (result->NextRow());

        // For each auction, refund bidder where possible, delete entry from AH
        delete result;

        AuctionHouseObject* auctionHouse;
        for (auto iter = ahBotActiveAuctions.begin(); iter != ahBotActiveAuctions.end(); ++iter)
        {
            AuctionInfo& ai = *iter;

            // Get Auction House and Auction references
            auctionHouse = nullptr;
            switch (ai.houseID) {
                case 1: auctionHouse = sAuctionMgr.GetAuctionsMap(sAuctionMgr.GetAuctionHouseEntry(AllianceConfig.GetAHFID())); break;
                case 6: auctionHouse = sAuctionMgr.GetAuctionsMap(sAuctionMgr.GetAuctionHouseEntry(HordeConfig.GetAHFID())); break;
                case 7: auctionHouse = sAuctionMgr.GetAuctionsMap(sAuctionMgr.GetAuctionHouseEntry(NeutralConfig.GetAHFID())); break;
            }
            if (auctionHouse == nullptr)
                continue;

            AuctionEntry* auction = auctionHouse->GetAuction(ai.itemID);
            if (!auction)
                continue;

            // If auction has a bidder, refund that character
            if (ai.characterGUID != 0)
            {
                ObjectGuid bidderGuid = ObjectGuid(HIGHGUID_PLAYER, ai.characterGUID);
                if (Player* bidder = sObjectMgr.GetPlayer(bidderGuid))
                {
                    std::ostringstream subject;
                    subject << auction->itemTemplate << ":0:" << AUCTION_CANCELLED_TO_BIDDER;
                    MailDraft(subject.str())
                        .SetMoney(auction->bid)
                        .SendMailTo(MailReceiver(bidder, bidderGuid), auction, MAIL_CHECK_MASK_COPIED);
                }
            }

            CharacterDatabase.PExecute("DELETE FROM item_instance WHERE guid = '%u'", auction->itemGuidLow);
            // Remove auction from AH
            auction->DeleteFromDB();
            sAuctionMgr.RemoveAItem(auction->itemGuidLow);
            auctionHouse->RemoveAuction(auction);
        }
    }
    else
    {
        delete result;
    }

    CharacterDatabase.CommitTransaction();
}

void AuctionHouseBot::CleanupBotMail()
{
    if (AHCharactersGUIDsForQuery.empty())
        return;

    CharacterDatabase.BeginTransaction();

    std::string itemGuidsQuery = "SELECT mi.item_guid FROM mail_items mi JOIN mail m ON mi.mail_id = m.id WHERE m.receiver IN (%s)";
    QueryResult* itemGuidsResult = CharacterDatabase.PQuery(itemGuidsQuery.c_str(), AHCharactersGUIDsForQuery.c_str());

    std::vector<uint32> itemGuidsToDelete;
    if (itemGuidsResult)
    {
        do
        {
            itemGuidsToDelete.push_back(itemGuidsResult->Fetch()->GetUInt32());
        } while (itemGuidsResult->NextRow());
        delete itemGuidsResult;
    }

    CharacterDatabase.PExecute("DELETE mi FROM mail_items mi JOIN mail m ON mi.mail_id = m.id WHERE m.receiver IN (%s)", AHCharactersGUIDsForQuery.c_str());
    CharacterDatabase.PExecute("DELETE FROM mail WHERE receiver IN (%s)", AHCharactersGUIDsForQuery.c_str());

    if (ReturnExpiredAuctionItemsToBot)
    {
        for (uint32 itemGuid : itemGuidsToDelete)
            CharacterDatabase.PExecute("UPDATE item_instance SET owner_guid = '%u' WHERE guid = '%u'", AHCharacters[0].CharacterGUID, itemGuid);
    }
    else
    {
        for (uint32 itemGuid : itemGuidsToDelete)
            CharacterDatabase.PExecute("DELETE FROM item_instance WHERE guid = '%u'", itemGuid);
    }

    CharacterDatabase.CommitTransaction();

    if (debug_Out)
        sLog.outString("AuctionHouseBot: Cleaned up bot mail. Items handled: %zu", itemGuidsToDelete.size());
}

uint32 AuctionHouseBot::GetRandomStackValue(std::string configKeyString, uint32 defaultValue)
{
    uint32 stackValue = GetConfigUInt(configKeyString.c_str(), defaultValue);
    if (stackValue > 100 || stackValue < 0)
    {
        sLog.outError("%s value is invalid.  Setting to default (%u).", configKeyString.c_str(), defaultValue);
        stackValue = defaultValue;
    }
    return stackValue;
}

uint32 AuctionHouseBot::GetRandomStackIncrementValue(std::string configKeyString, uint32 defaultValue)
{
    uint32 stackIncrementValue = GetConfigUInt(configKeyString.c_str(), defaultValue);
    if (stackIncrementValue <= 0)
    {
        sLog.outError("%s value is invalid.  Setting to default (%u).", configKeyString.c_str(), defaultValue);
        stackIncrementValue = defaultValue;
    }
    return stackIncrementValue;
}

void AuctionHouseBot::SetCyclesBetweenBuyOrSell()
{
    std::string buyCyclesConfigString = GetConfigString("AuctionHouseBot.MinutesBetweenBuyCycle", "1");
    GetConfigMinAndMax(buyCyclesConfigString, CyclesBetweenBuyActionMin, CyclesBetweenBuyActionMax);
    CyclesBetweenBuyAction = urand(CyclesBetweenBuyActionMin, CyclesBetweenBuyActionMax);

    std::string sellCyclesConfigString = GetConfigString("AuctionHouseBot.MinutesBetweenSellCycle", "1");
    GetConfigMinAndMax(sellCyclesConfigString, CyclesBetweenSellActionMin, CyclesBetweenSellActionMax);
    CyclesBetweenSellAction = urand(CyclesBetweenSellActionMin, CyclesBetweenSellActionMax);
}

void AuctionHouseBot::SetBuyingBotBuyCandidatesPerBuyCycle()
{
    std::string candidatesPerCycleString = GetConfigString("AuctionHouseBot.Buyer.BuyCandidatesPerBuyCycle", "1");
    GetConfigMinAndMax(candidatesPerCycleString, BuyingBotBuyCandidatesPerBuyCycleMin, BuyingBotBuyCandidatesPerBuyCycleMax);
}

void AuctionHouseBot::GetConfigMinAndMax(std::string config, uint32& min, uint32& max)
{
    size_t pos = config.find(':');
    if (pos != std::string::npos)
    {
        min = std::stoul(config.substr(0, pos));
        max = std::stoul(config.substr(pos + 1));

        if (min < 1)
            min = 1;
        if (max < min)
            max = min;
    }
    else
        min = max = std::stoul(config);
}

void AuctionHouseBot::AddCharacters(std::string characterGUIDString)
{
    AHCharacters.clear();
    std::string delimitedValue;
    std::stringstream characterGUIDStream;
    std::set<uint32> characterGUIDs;

    // Grab from the string
    characterGUIDStream.str(characterGUIDString);
    while (std::getline(characterGUIDStream, delimitedValue, ','))
    {
        std::string valueOne;
        std::stringstream characterGUIDStream(delimitedValue);
        characterGUIDStream >> valueOne;
        auto characterGUID = atoi(valueOne.c_str());
        if (characterGUID == 0)
            continue;
        if (characterGUIDs.find(characterGUID) != characterGUIDs.end())
        {
            if (debug_Out)
                sLog.outError("AuctionHouseBot: Duplicate character with GUID of %u found, skipping", characterGUID);
        }
        else
            characterGUIDs.insert(characterGUID);
    }

    // Lookup accounts and add them
    if (characterGUIDs.empty() == true)
    {
        sLog.outError("AuctionHouseBot: No character GUIDs were supplied. Be sure to set AuctionHouseBot.GUIDs");
        return;
    }
    AHCharactersGUIDsForQuery = "";
    bool first = true;
    for (uint32 curGUID : characterGUIDs)
    {
        if (first == false)
        {
            AHCharactersGUIDsForQuery += ", ";
        }
        AHCharactersGUIDsForQuery += std::to_string(curGUID);
        first = false;
    }
    QueryResult* queryResult = CharacterDatabase.PQuery("SELECT `guid`, `account` FROM `characters` WHERE guid IN (%s)", AHCharactersGUIDsForQuery.c_str());
    if (!queryResult || queryResult->GetRowCount() == 0)
    {
        sLog.outError("AuctionHouseBot: No character GUIDs found when looking up values from AuctionHouseBot.GUIDs from the character database 'characters.guid'.");
        if (queryResult)
            delete queryResult;
        return;
    }
    do
    {
        // Pull the data out
        Field* fields = queryResult->Fetch();
        uint32 guid = fields[0].GetUInt32();
        uint32 account = fields[1].GetUInt32();
        AuctionHouseBotCharacter curChar = AuctionHouseBotCharacter(account, guid);
        AHCharacters.push_back(curChar);
    } while (queryResult->NextRow());

    delete queryResult;
}

template <typename ValueType>
void AuctionHouseBot::AddItemValuePairsToItemIDMap(std::unordered_map<uint32, ValueType>& workingValueToItemIDMap, std::string valueToItemIDMapString)
{
    std::string delimitedValue;
    std::stringstream valueToItemIDStream;
    valueToItemIDStream.str(valueToItemIDMapString);
    while (std::getline(valueToItemIDStream, delimitedValue, ',')) // Process each item ID in the string, delimited by the comma ","
    {
        std::string curBlock;
        std::stringstream itemPairStream(delimitedValue);
        itemPairStream >> curBlock;

        // Only process if it has a colon (:)
        if (curBlock.find(":") != std::string::npos)
        {
            std::string itemIDString = curBlock.substr(0, curBlock.find(":"));
            auto itemId = atoi(itemIDString.c_str());
            std::string valueString = curBlock.substr(curBlock.find(":") + 1);
            auto convertedValue = atoi(valueString.c_str());
            if (itemId > 0 && convertedValue > 0)
                workingValueToItemIDMap.insert({ itemId, convertedValue });
        }
    }
}

void AuctionHouseBot::AddValuesToSetByKeyMap(std::map<uint32, std::unordered_set<uint32>>& workingSetByKeyMap, std::string valuesToKeyMapString,
    uint32 wildcardLowValue, uint32 wildcardHighValue)
{
    std::string delimitedValue;
    std::stringstream valueToKeyStream;
    valueToKeyStream.str(valuesToKeyMapString);
    while (std::getline(valueToKeyStream, delimitedValue, ',')) // Process each block delimited by the comma ","
    {
        std::string curBlock;
        std::stringstream itemPairStream(delimitedValue);
        itemPairStream >> curBlock;

        // Only process if it has a colon (:)
        if (curBlock.find(":") != std::string::npos)
        {
            std::string itemIDString = curBlock.substr(0, curBlock.find(":"));
            auto keyValue = atoi(itemIDString.c_str());
            std::string valueString = curBlock.substr(curBlock.find(":") + 1);
            if (valueString == "*")
            {
                for (uint32 i = wildcardLowValue; i <= wildcardHighValue; i++)
                    workingSetByKeyMap[keyValue].insert(i);
            }
            else
            {
                auto convertedValue = atoi(valueString.c_str());
                workingSetByKeyMap[keyValue].insert(convertedValue);
            }
        }
    }
}

void AuctionHouseBot::ParseNumberListToSet(std::set<uint32>& workingItemIDSet, std::string itemString, const char* parentOperationName)
{
    std::string delimitedValue;
    std::stringstream itemIdStream;

    itemIdStream.str(itemString);
    while (std::getline(itemIdStream, delimitedValue, ',')) // Process each item ID in the string, delimited by the comma ","
    {
        // Trim whitespace
        delimitedValue.erase(0, delimitedValue.find_first_not_of(" \t"));
        delimitedValue.erase(delimitedValue.find_last_not_of(" \t") + 1);

        std::string valueOne;
        std::stringstream itemPairStream(delimitedValue);
        itemPairStream >> valueOne;

        // If it has a hypen, then it's a range of numbers
        if (valueOne.find("-") != std::string::npos)
        {
            std::string leftIDString = valueOne.substr(0, valueOne.find("-"));
            std::string rightIDString = valueOne.substr(valueOne.find("-") + 1);

            auto leftId = atoi(leftIDString.c_str());
            auto rightId = atoi(rightIDString.c_str());

            if (leftId > rightId)
            {
                sLog.outError("AuctionHouseBot: Duplicate item ID range of %u to %u needs to be smallest to largest for %s, skipping", leftId, rightId, parentOperationName);
            }
            else
            {
                for (int32 i = leftId; i <= rightId; ++i)
                    AddToNumberListSet(workingItemIDSet, i, parentOperationName);
            }
        }
        else
        {
            auto itemId = atoi(valueOne.c_str());
            AddToNumberListSet(workingItemIDSet, itemId, parentOperationName);
        }
    }
}

void AuctionHouseBot::AddToNumberListSet(std::set<uint32>& workingItemIDSet, uint32 itemID, const char* parentOperationName)
{
    if (workingItemIDSet.find(itemID) != workingItemIDSet.end())
    {
        if (debug_Out)
            sLog.outError("AuctionHouseBot: Duplicate item id %u attempted to be put into a working item set from operation %s, skipping", itemID, parentOperationName);
    }
    else
    {
        workingItemIDSet.insert(itemID);
    }
}

const char* AuctionHouseBot::GetQualityName(ItemQualities quality)
{
    switch (quality)
    {
        case ITEM_QUALITY_POOR:       return "Poor";
        case ITEM_QUALITY_NORMAL:     return "Normal";
        case ITEM_QUALITY_UNCOMMON:   return "Uncommon";
        case ITEM_QUALITY_RARE:       return "Rare";
        case ITEM_QUALITY_EPIC:       return "Epic";
        case ITEM_QUALITY_LEGENDARY:  return "Legendary";
        case ITEM_QUALITY_ARTIFACT:   return "Artifact";
        default:                      return "Unknown";
    }
}

const char* AuctionHouseBot::GetCategoryName(ItemClass category)
{
    switch (category)
    {
        case ITEM_CLASS_CONSUMABLE:   return "Consumable";
        case ITEM_CLASS_CONTAINER:    return "Container";
        case ITEM_CLASS_WEAPON:       return "Weapon";
        case ITEM_CLASS_GEM:          return "Gem";
        case ITEM_CLASS_ARMOR:        return "Armor";
        case ITEM_CLASS_REAGENT:      return "Reagent";
        case ITEM_CLASS_PROJECTILE:   return "Projectile";
        case ITEM_CLASS_TRADE_GOODS:  return "TradeGood";
        case ITEM_CLASS_GENERIC:      return "Generic";
        case ITEM_CLASS_RECIPE:       return "Recipe";
        case ITEM_CLASS_MONEY:        return "Money";
        case ITEM_CLASS_QUIVER:       return "Quiver";
        case ITEM_CLASS_QUEST:        return "Quest";
        case ITEM_CLASS_KEY:          return "Key";
        case ITEM_CLASS_PERMANENT:    return "Permanent";
        case ITEM_CLASS_JUNK:         return "Misc";
        default:                      return "Unknown";
    }
}

void AuctionHouseBot::PopulateVendorItemsPrices()
{
    // Load vendor items' prices into a vector for fast lookup
    QueryResult* r = WorldDatabase.PQuery("SELECT MAX(entry) FROM item_template");
    if (!r)
    {
        vendorItemsPrices.clear();
        return;
    }
    Field* f = r->Fetch();
    uint32 maxItemID = f[0].GetUInt32();
    // Size by max entry + 1 so the highest entry itself is a valid index
    vendorItemsPrices = std::vector<uint32>(maxItemID + 1, UINT32_MAX);
    delete r;

    QueryResult* result = WorldDatabase.PQuery("SELECT v.entry, MIN(v.sell_price) AS sell_price FROM item_template v JOIN npc_vendor p ON v.entry = p.item WHERE v.`class` != %u GROUP BY v.entry", ITEM_CLASS_TRADE_GOODS);
    if (result)
    {
        do
        {
            Field* pFields = result->Fetch();
            uint32 itemID = pFields[0].GetUInt32();
            uint32 itemPrice = pFields[1].GetUInt32();
            if (itemID < vendorItemsPrices.size())
                vendorItemsPrices[itemID] = itemPrice;
        } while (result->NextRow());
        delete result;
    }
}

void AuctionHouseBot::CleanupExpiredAuctionItems()
{
    if (AHCharactersGUIDsForQuery.empty() ||
        ReturnExpiredAuctionItemsToBot)
        return;

    // Delete item_instances that are not in the Auction Houses
    std::string queryItemInstancesString = R"SQL(
        SELECT guid
            FROM item_instance
            LEFT JOIN auction ON auction.itemguid = item_instance.guid
            WHERE item_instance.owner_guid IN (%s)
            AND auction.id IS NULL
    )SQL";

    QueryResult* queryItemInstancesResult = CharacterDatabase.PQuery(queryItemInstancesString.c_str(), AHCharactersGUIDsForQuery.c_str());
    if (!queryItemInstancesResult)
        return;

    CharacterDatabase.BeginTransaction();

    do
    {
        uint32 guid = queryItemInstancesResult->Fetch()[0].GetUInt32();
        CharacterDatabase.PExecute("DELETE FROM item_instance WHERE guid = '%u'", guid);
    } while (queryItemInstancesResult->NextRow());

    CharacterDatabase.CommitTransaction();

    delete queryItemInstancesResult;
}

bool AuctionHouseBot::IsItemQuestReward(uint32 itemID)
{
    return (QuestRewardItemIDs.find(itemID) != QuestRewardItemIDs.end());
}

bool AuctionHouseBot::IsItemCrafted(uint32 itemID)
{
    return (ItemIDsProducedByRecipes.find(itemID) != ItemIDsProducedByRecipes.end());
}

bool AuctionHouseBot::IsItemCategoryQualityInDBDropRatesConfig(ItemPrototype const* proto)
{
    if (!proto)
        return false;

    switch (proto->Class)
    {
        case ITEM_CLASS_WEAPON:
            return (AdvancedListingRuleUseDropRatesWeaponEnabled &&
                    AdvancedListingRuleUseDropRatesWeaponAffectedQualities.find(proto->Quality) != AdvancedListingRuleUseDropRatesWeaponAffectedQualities.end());
        case ITEM_CLASS_ARMOR:
            return (AdvancedListingRuleUseDropRatesArmorEnabled &&
                    AdvancedListingRuleUseDropRatesArmorAffectedQualities.find(proto->Quality) != AdvancedListingRuleUseDropRatesArmorAffectedQualities.end());
        case ITEM_CLASS_RECIPE:
            return (AdvancedListingRuleUseDropRatesRecipeEnabled &&
                    AdvancedListingRuleUseDropRatesRecipeAffectedQualities.find(proto->Quality) != AdvancedListingRuleUseDropRatesRecipeAffectedQualities.end());
        default:
            return false;
    }
}

bool AuctionHouseBot::IsItemEligibleForDBDropRates(ItemPrototype const* proto)
{
    if (!proto || !AdvancedListingRuleUseDropRatesEnabled)
        return false;

    // Only continue if the itemID isn't an exception
    if (AdvancedListingRuleUseDropRatesExceptionItems.find(proto->ItemId) != AdvancedListingRuleUseDropRatesExceptionItems.end())
        return false;


    // If feature enabled, && item category/quality enabled
    //   && is not crafted, && is not a quest reward
    return (AdvancedListingRuleUseDropRatesEnabled &&
            IsItemCategoryQualityInDBDropRatesConfig(proto) &&
            !IsItemCrafted(proto->ItemId) &&
            !IsItemQuestReward(proto->ItemId)
    );
}

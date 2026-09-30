## Auction Bot Plus (Tortoise-WoW Port)

This is a port of [AuctionHouseBot Plus](https://github.com/NathanHandley/mod-ah-bot-plus) for **Tortoise-WoW**. It gives a more Blizzlike auction-house population and is built around the ethos: *"Maximum understandable customization in the hands of admins via non-SQL configurations"*.

Feature highlights:
- All configuration is done via config files, with no SQL configuration needed.
- Listing control via item inclusion/exclusion rules, category list proportions, stack-size management, etc.
- Customizable pricing based on category, subcategory, quality, item level, etc.
- Optional advanced price calculation rules for a more out-of-the-box Blizzlike feel.
- A customizable buying bot that can be fair, stingy, or greedy.
- Multiple AH-bot character support for diverse listing names.
- GM server commands to reload configs and regenerate auctions.
- Periodic mail cleanup for the bot characters (Tortoise does not expose mail-suppression hooks).

## Requirements

- Tortoise-WoW source and build environment.
- A character database with at least one character created to act as the auction bot.
- **MySQL 8.0+ or MariaDB 10.2+** is required if you enable `AuctionHouseBot.AdvancedListingRules.UseDropRates.Enabled`, because that feature uses SQL `WITH` / CTE queries.

## Installation

1. Place the module under the `modules` directory of your Tortoise-WoW source.
2. Re-run CMake and launch a clean build.
3. Copy or rename `modules/tw-mod-ah-bot-plus/conf/mod_ahbot.conf.dist` to the `modules/` directory next to your `mangosd.conf` as `mod_ahbot.conf`.
4. Edit `mod_ahbot.conf` and set `AuctionHouseBot.GUIDs` to one or more character GUIDs from your `characters.characters` table.

## Vanilla-specific changes

This port removes content that does not exist in Vanilla WoW:
- **Heirloom** quality handling and config entries removed.
- **Glyph** config sections removed (no `ITEM_CLASS_GLYPH` in Tortoise).
- **Jewelcrafting** and **Inscription** removed from the recipe-produced-item skill list.
- **Artifact** quality and **Gem** item class are kept, because they exist in Vanilla/Tortoise.
- Default item-ID lists were validated against the live `item_template` database and pruned of non-Vanilla / missing entries.

The module uses Tortoise's `ITEM_CLASS_JUNK` enum internally, but exposes it as the **Misc** auction-house category in config keys to match the in-game naming.

## Usage

**Before you do anything, make at least one character to use as the bot.** Edit `mod_ahbot.conf` and add one or more character GUIDs to `AuctionHouseBot.GUIDs`. These names will be visible in the auction house, so pick good names. **Important:** If you use a bot mod (like PlayerBots), use regular non-bot characters for your auction-house character(s).

After that, set `AuctionHouseBot.EnableSeller = true`. Nothing else is required unless you want to tune pricing or enable the buyer bot with `AuctionHouseBot.Buyer.Enabled`.

Notes:
- These accounts do not need any security level and can be player accounts.
- The characters used by the AHBot are not meant to be used in-game. Browsing the auction house on them may cause issues like "Searching for items..." displaying forever.
- It takes a few hours for the auction house to fully populate, as only 75 items are added by default every tick. Change this with `AuctionHouseBot.ItemsPerCycle`.
- All price multipliers (along with advanced pricing) are applied multiplicatively. Example: a Category of 1.5x, Quality of 2x, and CategoryQuality of 1.4x yields 4.2x. Advanced pricing then multiplies that value further. You cannot combine item-level multipliers with advanced pricing for the same category; advanced pricing takes priority.
- Bot-listed prices will not exceed 100k gold buyout. This can be reduced in the config.

## In-Game Commands

The module adds the following GM-only commands:

| Command | Description |
|----------|--------------|
| `.ahbot reload` | Reloads the AuctionHouseBot configuration file and updates settings. |
| `.ahbot empty` | Removes all AuctionHouseBot auctions from all auction houses. Player auctions are unaffected; bids on affected items are returned to players. Use with caution! |
| `.ahbot update` | Forces the bot to run an immediate buy/sell cycle. You may need to run it multiple times if multiple minutes are configured between cycles. |
| `.ahbot help` | Shows the available AHBot commands. |

## Mail cleanup

Because Tortoise does not provide the AzerothCore mail-suppression hooks, the bot characters will receive auction mail (successful auctions, outbids, expired items, etc.). The module runs `CleanupBotMail()` automatically during each update cycle, controlled by:

```ini
AuctionHouseBot.MailCleanup.Enabled = true
AuctionHouseBot.MailCleanup.IntervalMinutes = 5
```

This deletes mail and attached items sent to the bot characters. If you want expired items returned to the bots instead, enable `AuctionHouseBot.ReturnExpiredAuctionItemsToBot = true`.

## Buying Bot Behavior

1. **Determining Items to Buy:** Every cycle the buyer bot selects `BuyCandidatesPerBuyCycle` player-listed auctions as potential purchases.
2. **Price Willing to Pay:** It uses the same price calculation as the seller bot, including the random +/- 25% band, then multiplies the result by `AcceptablePriceModifier`.
3. **Buying:** If the calculated price is higher than the buyout, it buys out. Otherwise, if the item has no bot bid and the bid is below the calculated price, it places a bid.

This runs independently for each enabled auction house.

## Additional Resources

[Advanced Pricing Calculator](tools/AdvancedPricingCalculator/Advanced_Pricing_Calculator.xlsx) — If `AuctionHouseBot.AdvancedPricing.<Category>.<Subclass>.Enabled` is enabled, this spreadsheet can help visualize how config settings affect in-game prices.

## Credits

- NathanHandley: Created the original AuctionHouseBot Plus rewrite for AzerothCore.
- Zeb139: Enhanced pricing formulas and performance improvements.
- Ayase: Original port to AzerothCore.
- Other contributors (see the contributors list in the upstream repository).

This Tortoise-WoW port was adapted from the AzerothCore version.
